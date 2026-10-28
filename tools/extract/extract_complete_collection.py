#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
# distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
"""Extract the game data of YOUR OWN copy of The Sims Complete Collection into the folder layout sims3ds expects on
the SD card (sdmc:/3ds/sims/). Players normally start it through extract.command (macOS), extract.bat (Windows) or
extract.sh (Linux), which install the Python helpers and then run this script.

    python3 extract_complete_collection.py [SOURCE] [--out <dir>]

SOURCE is either
    * a folder with images of YOUR OWN discs (see below), or
    * YOUR OWN installed game folder (e.g. C:\\Program Files (x86)\\Maxis\\The Sims) - just copied, no extra tools needed.
Without SOURCE the script asks for it (you can drag the folder into the window). --discs DIR is the old spelling.

--out defaults to the 3ds/sims/ folder of the unpacked release zip (when the script runs from its extractor/ folder),
otherwise to ./Sims3DS-SD/3ds/sims. Afterwards the EA intro is converted for the 3DS when ffmpeg is installed
(src/app/tools/convert_intro.py -> <out>/cache/intro.stv + intro.wav); without ffmpeg that step is skipped.

Accepted disc inputs (in --discs, any mix, one per disc; the disc number is taken from "(Disc N)" / "disc N" in the
name, otherwise from the sorted order):
    *.7z       archives holding a .bin/.cue or .iso (needs `pip install py7zr`)
    *.bin      raw CD images (MODE1/2352, MODE2/2352 or 2448-byte sectors with subchannel), the .cue is not needed
    *.iso      plain 2048-byte-sector images
    *.mdf      Alcohol 120% images (2352/2448-byte sectors)
    folders    a copied disc (e.g. the contents of a mounted disc / drive letter)

Pipeline
  1. 7z  -> bin/iso                         (py7zr)
  2. bin -> iso: strip the 16-byte sync/header and the ECC (and subchannel) of every raw sector, keep 2048 bytes
  3. iso -> the InstallShield cabinet: data1.hdr + data1.cab ... data5.cab, which are spread over the discs
     (Disc 1: data1.hdr, data1.cab, data2.cab; Disc 2: data3.cab; Disc 3: data4.cab; Disc 4: data5.cab).
     Read with pycdlib (`pip install pycdlib`), Joliet names first. Copy-protected discs can carry a primary volume
     descriptor (PVD) or directory records that pycdlib rejects; then a small tolerant ISO 9660/Joliet reader built
     into this script is used instead (it only follows directory records and never validates the PVD's sizes).
  4. unshield (https://github.com/twogood/unshield) extracts every file group of the cabinet set:
         unshield -d <work>/groups x data1.hdr
  5. the groups are laid out like a PC install:
         GameData_ALL_Recursive + GameData_ALL_Ranger -> GameData/
         ExpansionPack1 -> ExpansionPack/     ExpansionPack2..7 -> ExpansionPack2..7/
         ExpansionPackGOLD -> Deluxe/         Music_ALL_Recursive -> Music/
         SoundData_SimsSounds -> SoundData/   ExpansionShared, Downloads, UIGraphics, UserData, UserData2 and
         every Template* group keep their names.  Creator_Files only with --with-creator.  Everything else
         (installer, executables, copy protection, localisation templates) is skipped and listed.

Installed game instead of discs: the folders GameData, UserData*, ExpansionPack*, ExpansionShared, Deluxe,
Downloads, UIGraphics, Music, SoundData and Template* are copied (never Saves).

unshield (discs only): macOS `brew install unshield` (offered automatically); Debian/Ubuntu `sudo apt install
unshield`; Fedora `sudo dnf install unshield`; Arch `sudo pacman -S unshield`; Windows: install the game from your
discs and use the installed folder, or use WSL (Ubuntu). Pass --unshield PATH if it is not on PATH.

Disk space: about 3 GB for the output plus up to ~1.6 GB of temporary files (one disc image at a time + the
cabinet files). Temporary files go to --work (default: <out>/../sims3ds_extract_work) and are removed at the end
unless --keep-work is given.
"""
import argparse
import os
import re
import shutil
import struct
import subprocess
import sys

CAB_RE = re.compile(r'^data\d+\.(hdr|cab)$', re.I)

# unshield file group -> folder under the output root ('' = skip). Groups not listed here are skipped and reported.
GROUP_MAP = {
    'GameData_ALL_Recursive': 'GameData',
    'GameData_ALL_Ranger': 'GameData',
    'ExpansionPack1': 'ExpansionPack',
    'ExpansionPack2': 'ExpansionPack2',
    'ExpansionPack3': 'ExpansionPack3',
    'ExpansionPack4': 'ExpansionPack4',
    'ExpansionPack5': 'ExpansionPack5',
    'ExpansionPack6': 'ExpansionPack6',
    'ExpansionPack7': 'ExpansionPack7',
    'ExpansionPackGOLD': 'Deluxe',
    'ExpansionShared': 'ExpansionShared',
    'Downloads': 'Downloads',
    'Music_ALL_Recursive': 'Music',
    'SoundData_SimsSounds': 'SoundData',
    'UIGraphics': 'UIGraphics',
    'UserData': 'UserData',
    'UserData2': 'UserData2',
}
OPTIONAL_GROUPS = {'Creator_Files': 'Creator_Files'}


def log(*a):
    print('[extract]', *a, flush=True)


# ------------------------------------------------------------------------------------------------ disc inputs
def disc_number(name, fallback):
    m = re.search(r'disc\s*(\d+)', name, re.I)
    return int(m.group(1)) if m else fallback


def find_discs(d):
    items = []
    for n in sorted(os.listdir(d)):
        p = os.path.join(d, n)
        low = n.lower()
        if os.path.isdir(p) or low.endswith(('.7z', '.bin', '.iso', '.mdf', '.img')):
            items.append(p)
    # prefer one input per disc number: an extracted image over its .7z
    rank = {'.iso': 0, '.bin': 1, '.img': 1, '.mdf': 1, '.7z': 3}
    best = {}
    for i, p in enumerate(items):
        k = disc_number(os.path.basename(p), i + 1)
        r = 2 if os.path.isdir(p) else rank.get(os.path.splitext(p)[1].lower(), 9)
        if k not in best or r < best[k][0]:
            best[k] = (r, p)
    return [(k, best[k][1]) for k in sorted(best)]


def unpack_7z(path, work):
    try:
        import py7zr
    except ImportError:
        sys.exit('py7zr is needed for .7z inputs: python3 -m pip install py7zr')
    with py7zr.SevenZipFile(path) as z:
        names = [n for n in z.getnames() if n.lower().endswith(('.bin', '.iso', '.img', '.mdf'))]
        if not names:
            sys.exit('%s: no .bin/.iso inside' % path)
        log('unpacking', os.path.basename(path), '->', names[0])
        z.extract(work, targets=[names[0]])
    return os.path.join(work, names[0])


def raw_to_iso(path, iso):
    """Raw CD image -> 2048-byte-sector ISO. Returns the path of a usable ISO (may be `path` itself)."""
    size = os.path.getsize(path)
    with open(path, 'rb') as f:
        head = f.read(16)
    if head[:12] != b'\x00' + b'\xff' * 10 + b'\x00':
        return path                                                  # already 2048-byte sectors
    ss = 2448 if (size % 2448 == 0 and size % 2352 != 0) else 2352
    mode = head[15]
    log('converting %s (%d-byte sectors, mode %d) -> ISO' % (os.path.basename(path), ss, mode))
    with open(path, 'rb') as f, open(iso, 'wb') as o:
        while True:
            s = f.read(ss)
            if len(s) < ss:
                break
            # MODE1: 12 sync + 4 header, then 2048 data. MODE2 form 1: 12 sync + 4 header + 8 subheader.
            off = 24 if s[15] == 2 else 16
            o.write(s[off:off + 2048])
    return iso


# ------------------------------------------------------------------------------------------------ ISO reading
class TolerantIso:
    """Minimal ISO 9660 / Joliet reader: walks directory records only (no PVD size/consistency checks)."""

    def __init__(self, path):
        self.f = open(path, 'rb')
        self.root, self.joliet = None, False
        for lba in range(16, 64):
            d = self.sector(lba)
            if d[1:6] != b'CD001':
                break
            t = d[0]
            if t == 255:
                break
            if t == 1 and self.root is None:
                self.root = d[156:156 + 34]
            if t == 2 and d[88:91] in (b'%/@', b'%/C', b'%/E'):          # Joliet SVD (UCS-2 escapes)
                self.root, self.joliet = d[156:156 + 34], True
        if self.root is None:
            raise IOError('no ISO 9660 volume descriptor')

    def sector(self, lba, n=1):
        self.f.seek(lba * 2048)
        return self.f.read(2048 * n)

    def records(self, rec):
        lba, size = struct.unpack_from('<I', rec, 2)[0], struct.unpack_from('<I', rec, 10)[0]
        data = self.sector(lba, (size + 2047) // 2048)[:size]
        i = 0
        while i < len(data):
            ln = data[i]
            if ln == 0:
                i = (i // 2048 + 1) * 2048
                continue
            r = data[i:i + ln]
            nl = r[32]
            raw = r[33:33 + nl]
            i += ln
            if raw in (b'\x00', b'\x01'):
                continue
            name = raw.decode('utf-16-be', 'replace') if self.joliet else raw.decode('latin-1')
            yield name.split(';')[0].rstrip('.'), r, bool(r[25] & 2)

    def walk(self, rec=None, path=''):
        for name, r, isdir in self.records(rec or self.root):
            if isdir:
                yield from self.walk(r, path + '/' + name)
            else:
                yield path + '/' + name, r

    def extract(self, rec, dst):
        lba, size = struct.unpack_from('<I', rec, 2)[0], struct.unpack_from('<I', rec, 10)[0]
        self.f.seek(lba * 2048)
        with open(dst, 'wb') as o:
            left = size
            while left:
                b = self.f.read(min(left, 1 << 20))
                if not b:
                    raise IOError('image ends inside ' + dst)
                o.write(b)
                left -= len(b)


def copy_cabinet_files(iso, cabdir):
    """Copy data*.hdr / data*.cab from an ISO into cabdir. Returns the copied names."""
    got = []
    try:
        import pycdlib
        p = pycdlib.PyCdlib()
        p.open(iso)
        use_joliet = p.has_joliet()
        fac = p.get_joliet_facade() if use_joliet else p.get_iso9660_facade()
        for dirname, _dirs, files in fac.walk('/'):
            for fn in files:
                clean = fn.split(';')[0]
                if CAB_RE.match(clean):
                    src = dirname.rstrip('/') + '/' + fn
                    dst = os.path.join(cabdir, clean.lower())
                    log('  %s -> %s' % (src, os.path.basename(dst)))
                    kw = {'joliet_path': src} if use_joliet else {'iso_path': src}
                    fac.get_file_from_iso(dst, **kw)
                    got.append(clean.lower())
        p.close()
        return got
    except ImportError:
        log('pycdlib not installed (python3 -m pip install pycdlib) - using the built-in reader')
    except Exception as e:                                           # protected PVD / odd directory records
        log('pycdlib could not read this image (%s: %s) - using the built-in tolerant reader' % (type(e).__name__, e))
    t = TolerantIso(iso)
    for path, rec in t.walk():
        clean = path.rsplit('/', 1)[-1]
        if CAB_RE.match(clean):
            dst = os.path.join(cabdir, clean.lower())
            log('  %s -> %s' % (path, os.path.basename(dst)))
            t.extract(rec, dst)
            got.append(clean.lower())
    return got


def copy_cabinet_from_folder(folder, cabdir):
    got = []
    for root, _dirs, files in os.walk(folder):
        for fn in files:
            if CAB_RE.match(fn):
                shutil.copyfile(os.path.join(root, fn), os.path.join(cabdir, fn.lower()))
                got.append(fn.lower())
    return got


# ------------------------------------------------------------------------------------------------ layout
def merge_tree(src, dst):
    """Move src/* into dst/, merging folders (case-insensitive match on existing names)."""
    os.makedirs(dst, exist_ok=True)
    existing = {n.lower(): n for n in os.listdir(dst)}
    for n in os.listdir(src):
        s = os.path.join(src, n)
        d = os.path.join(dst, existing.get(n.lower(), n))
        if os.path.isdir(s) and os.path.isdir(d):
            merge_tree(s, d)
            os.rmdir(s)
        else:
            if os.path.exists(d):
                os.remove(d)
            shutil.move(s, d)


def layout(groups_dir, out, with_creator):
    groups = sorted(os.listdir(groups_dir))
    skipped = []
    mapping = dict(GROUP_MAP)
    if with_creator:
        mapping.update(OPTIONAL_GROUPS)
    for g in groups:
        src = os.path.join(groups_dir, g)
        if not os.path.isdir(src):
            continue
        target = mapping.get(g)
        if target is None and g.startswith('Template'):
            target = g
        if not target:
            skipped.append(g)
            continue
        log('group %-26s -> %s/' % (g, target))
        merge_tree(src, os.path.join(out, target))
    if skipped:
        log('skipped groups (not needed by sims3ds):', ', '.join(skipped))


def check(out):
    need = ['GameData/Objects/Objects.far', 'GameData/Global/Global.far', 'GameData/walls.iff', 'GameData/floors.iff',
            'UserData/Neighborhood.iff']
    missing = []
    for rel in need:
        cur = out
        for part in rel.split('/'):
            names = {n.lower(): n for n in os.listdir(cur)} if os.path.isdir(cur) else {}
            if part.lower() not in names:
                missing.append(rel)
                break
            cur = os.path.join(cur, names[part.lower()])
    eps = [n for n in ['ExpansionPack', 'ExpansionPack2', 'ExpansionPack3', 'ExpansionPack4', 'ExpansionPack5',
                       'ExpansionPack6', 'ExpansionPack7', 'Deluxe'] if os.path.isdir(os.path.join(out, n))]
    total = 0
    for root, _d, files in os.walk(out):
        total += sum(os.path.getsize(os.path.join(root, f)) for f in files)
    log('expansion folders present: %d/8 (%s)' % (len(eps), ', '.join(eps)))
    log('output size: %.2f GB' % (total / 1e9))
    if missing:
        log('MISSING:', ', '.join(missing))
        return False
    return True


# ------------------------------------------------------------------------------------------------ installed game
# folders of a PC install that sims3ds reads (case-insensitive); every Template* folder is copied too
INSTALL_DIRS = ['GameData', 'UserData', 'UserData2', 'UserData3', 'UserData4', 'UserData5', 'UserData6', 'UserData7',
                'UserData8', 'ExpansionPack', 'ExpansionPack2', 'ExpansionPack3', 'ExpansionPack4', 'ExpansionPack5',
                'ExpansionPack6', 'ExpansionPack7', 'ExpansionShared', 'Deluxe', 'Downloads', 'UIGraphics', 'Music',
                'SoundData']


def is_install(d):
    try:
        names = {n.lower() for n in os.listdir(d) if os.path.isdir(os.path.join(d, n))}
    except OSError:
        return False
    return 'gamedata' in names and 'userdata' in names


def find_install(d):
    """d itself, or a game folder up to two levels below it (e.g. the 'Maxis' folder was picked)."""
    if is_install(d):
        return d
    for root, dirs, _files in os.walk(d):
        if root[len(d):].count(os.sep) >= 2:
            dirs[:] = []
            continue
        for n in sorted(dirs):
            if is_install(os.path.join(root, n)):
                return os.path.join(root, n)
    return None


def install_plan(src):
    want = {n.lower(): n for n in INSTALL_DIRS}
    plan = []
    for n in sorted(os.listdir(src)):
        p = os.path.join(src, n)
        if not os.path.isdir(p):
            continue
        if n.lower() in want:
            plan.append((p, want[n.lower()]))
        elif n.lower().startswith('template'):
            plan.append((p, n))
    return plan


def copy_install(src, out):
    for p, target in install_plan(src):
        log('copying %-18s -> %s/' % (os.path.basename(p), target))
        shutil.copytree(p, os.path.join(out, target), dirs_exist_ok=True,
                        ignore=shutil.ignore_patterns('Saves', '*.exe', '*.dll', 'Thumbs.db', '.DS_Store'))


# ------------------------------------------------------------------------------------------------ helpers
def clean_path(text):
    """A path typed or dragged into a terminal: strip quotes and shell escapes (a backslash before a space)."""
    t = text.strip()
    if len(t) >= 2 and t[0] == t[-1] and t[0] in '"\'':
        t = t[1:-1]
    elif os.name != 'nt':
        t = re.sub(r'\\(.)', r'\1', t)
    return os.path.expanduser(t.strip())


def ask_source():
    print()
    print('Which folder holds YOUR OWN copy of The Sims Complete Collection?')
    print('  - a folder with images of your own discs (.iso / .bin / .7z), or')
    print('  - your installed game folder (for example C:\\Program Files (x86)\\Maxis\\The Sims)')
    print('Drag the folder into this window (or type its path) and press Enter:')
    try:
        return clean_path(input('> '))
    except EOFError:
        return ''


def default_out():
    here = os.path.dirname(os.path.abspath(__file__))
    release_sims = os.path.join(os.path.dirname(here), '3ds', 'sims')
    if os.path.isdir(release_sims):                                  # running from the release zip's extractor/
        return release_sims
    return os.path.join(os.getcwd(), 'Sims3DS-SD', '3ds', 'sims')


def unshield_help():
    if sys.platform == 'darwin':
        return 'install it with Homebrew (https://brew.sh):  brew install unshield'
    if os.name == 'nt':
        return ('unshield is not available as a simple Windows download. Easiest: install The Sims from your discs '
                'on this PC, then run the extractor again and pick the installed game folder (no unshield needed). '
                'Or use WSL (Ubuntu): sudo apt install unshield, then run extract.sh there.')
    return ('install it with your package manager:  sudo apt install unshield  (Debian/Ubuntu)  |  '
            'sudo dnf install unshield  (Fedora)  |  sudo pacman -S unshield  (Arch)')


def find_unshield(given, interactive):
    if given and os.path.exists(given):
        return given
    u = shutil.which('unshield')
    if u:
        return u
    brew = shutil.which('brew')
    if sys.platform == 'darwin' and brew and interactive:
        ans = input('unshield is needed to unpack the discs. Install it now with Homebrew? [Y/n] ').strip().lower()
        if ans in ('', 'y', 'yes'):
            subprocess.run([brew, 'install', 'unshield'])
            u = shutil.which('unshield') or next((p for p in ('/opt/homebrew/bin/unshield', '/usr/local/bin/unshield')
                                                   if os.path.exists(p)), None)
            if u:
                return u
    sys.exit('\nThe tool "unshield" is needed for disc images but was not found.\n-> ' + unshield_help() +
             '\nThen run the extractor again.')


def find_convert_intro():
    here = os.path.dirname(os.path.abspath(__file__))
    for c in (os.path.join(here, 'convert_intro.py'),
              os.path.join(here, '..', '..', 'src', 'app', 'tools', 'convert_intro.py')):
        if os.path.exists(c):
            return os.path.abspath(c)
    return None


def find_ci(root, rel):
    cur = root
    for part in rel.split('/'):
        names = {n.lower(): n for n in os.listdir(cur)} if os.path.isdir(cur) else {}
        if part.lower() not in names:
            return None
        cur = os.path.join(cur, names[part.lower()])
    return cur


def convert_intro(out):
    """EA intro -> <out>/cache/intro.stv + intro.wav, only when ffmpeg is installed (silently skipped otherwise)."""
    avi = find_ci(out, 'Music/eag_us.avi')
    script = find_convert_intro()
    if not avi or not script or not shutil.which('ffmpeg'):
        return
    cache = os.path.join(out, 'cache')
    os.makedirs(cache, exist_ok=True)
    log('converting the intro video (ffmpeg)')
    r = subprocess.run([sys.executable, script, avi, os.path.join(cache, 'intro.stv')])
    if r.returncode != 0:
        log('intro conversion failed - the game simply starts at the loading screen')


# ------------------------------------------------------------------------------------------------ main
def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('source', nargs='?', help='your disc images folder, or your installed The Sims folder')
    ap.add_argument('--discs', help='same as SOURCE (older spelling)')
    ap.add_argument('--out', help='output folder = sdmc:/3ds/sims/ (default: the release zip\'s 3ds/sims, '
                                  'else ./Sims3DS-SD/3ds/sims)')
    ap.add_argument('--work', help='temporary folder (default: next to --out)')
    ap.add_argument('--unshield', help='path to the unshield binary (disc images only)')
    ap.add_argument('--with-creator', action='store_true', help='also extract Creator_Files (not used by sims3ds)')
    ap.add_argument('--keep-work', action='store_true', help='keep the temporary files')
    ap.add_argument('--no-intro', action='store_true', help='do not convert the intro video even if ffmpeg exists')
    ap.add_argument('--dry-run', action='store_true', help='only show what would be done')
    a = ap.parse_args()

    interactive = sys.stdin.isatty()
    src = clean_path(a.source or a.discs or '') if (a.source or a.discs) else ask_source()
    if not src:
        sys.exit('No folder given.')
    if os.path.isfile(src):                                           # a single image was dropped: use its folder
        src = os.path.dirname(os.path.abspath(src))
    if not os.path.isdir(src):
        sys.exit('Folder not found: ' + src)
    src = os.path.abspath(src)
    out = os.path.abspath(clean_path(a.out) if a.out else default_out())
    install = find_install(src)
    discs = [] if install else find_discs(src)
    if not install and not discs:
        sys.exit('No disc images (.iso/.bin/.7z/.mdf) and no installed game (GameData + UserData folders) found in\n  '
                 + src)

    log('source:', install or src, '(installed game)' if install else '(%d disc image(s))' % len(discs))
    log('output:', out)
    log('intro :', 'skipped (--no-intro)' if a.no_intro else
        ('yes (ffmpeg found)' if shutil.which('ffmpeg') else 'skipped (ffmpeg not installed)'))
    if a.dry_run:
        if install:
            for p, target in install_plan(install):
                log('would copy %s -> %s/' % (os.path.basename(p), target))
        else:
            for num, p in discs:
                log('would read disc %d: %s' % (num, os.path.basename(p)))
            log('unshield:', shutil.which('unshield') or a.unshield or 'NOT FOUND - ' + unshield_help())
        return

    os.makedirs(out, exist_ok=True)
    if install:
        copy_install(install, out)
    else:
        extract_discs(a, discs, out, find_unshield(a.unshield, interactive))
    ok = check(out)
    if not a.no_intro:
        convert_intro(out)
    log('done ->', out)
    if ok:
        print('\nAll set. Copy the folder "3ds" that contains this "sims" folder to the root of your SD card\n'
              '(together with the release zip\'s "cia" folder), then install cia/sims.cia with FBI.')
    else:
        print('\nSome key game files are missing (see MISSING above). Check that you picked the right folder:\n'
              'all disc images of YOUR OWN Complete Collection, or a complete installed game.')
    sys.exit(0 if ok else 1)


def extract_discs(a, discs, out, unshield):
    work = os.path.abspath(a.work or os.path.join(os.path.dirname(out), 'sims3ds_extract_work'))
    cabdir, groups_dir = os.path.join(work, 'cab'), os.path.join(work, 'groups')
    os.makedirs(cabdir, exist_ok=True)
    for num, src in discs:
        log('disc %d: %s' % (num, os.path.basename(src)))
        tmp = os.path.join(work, 'disc%d' % num)
        os.makedirs(tmp, exist_ok=True)
        if os.path.isdir(src):
            got = copy_cabinet_from_folder(src, cabdir)
        else:
            img = unpack_7z(src, tmp) if src.lower().endswith('.7z') else src
            iso = raw_to_iso(img, os.path.join(tmp, 'disc.iso'))
            got = copy_cabinet_files(iso, cabdir)
        log('disc %d: %s' % (num, ', '.join(sorted(got)) or 'no cabinet files'))
        if not a.keep_work:
            shutil.rmtree(tmp, ignore_errors=True)

    if not os.path.exists(os.path.join(cabdir, 'data1.hdr')):
        sys.exit('data1.hdr not found on any disc (is Disc 1 included?)')
    cabs = sorted(n for n in os.listdir(cabdir) if n.endswith('.cab'))
    log('cabinet set:', ', '.join(cabs))

    if os.path.isdir(groups_dir):
        shutil.rmtree(groups_dir)
    log('unshield: extracting all file groups (this takes a few minutes)')
    r = subprocess.run([unshield, '-d', groups_dir, 'x', os.path.join(cabdir, 'data1.hdr')],
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
    tail = r.stdout.strip().splitlines()[-3:]
    for line in tail:
        log('unshield:', line)
    if r.returncode != 0:
        sys.exit('unshield failed (exit %d); are all data*.cab files present?' % r.returncode)

    layout(groups_dir, out, a.with_creator)
    if not a.keep_work:
        shutil.rmtree(work, ignore_errors=True)


if __name__ == '__main__':
    main()
