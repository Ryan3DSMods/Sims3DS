#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
# distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
"""Extract the game data of YOUR OWN copy of The Sims Complete Collection (USA, 4 discs) into the folder layout
sims3ds expects on the SD card (sdmc:/3ds/sims/).

    python3 extract_complete_collection.py --discs <dir with your disc images> --out <output dir>

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

Alternative without discs: copy the install folder of a full PC installation (The Sims + all expansions) instead,
see README.md "Option B".

unshield: macOS `brew install unshield`; Debian/Ubuntu `sudo apt install unshield`; Arch `pacman -S unshield`;
Windows: use WSL (Ubuntu) or build it from source (cmake + zlib). Pass --unshield PATH if it is not on PATH.

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


# ------------------------------------------------------------------------------------------------ main
def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--discs', required=True, help='folder with your disc images (.7z/.bin/.iso/.mdf) or disc folders')
    ap.add_argument('--out', required=True, help='output folder (its contents go to sdmc:/3ds/sims/)')
    ap.add_argument('--work', help='temporary folder (default: next to --out)')
    ap.add_argument('--unshield', default=shutil.which('unshield'), help='path to the unshield binary')
    ap.add_argument('--with-creator', action='store_true', help='also extract Creator_Files (not used by sims3ds)')
    ap.add_argument('--keep-work', action='store_true', help='keep the temporary files')
    a = ap.parse_args()

    if not a.unshield or not os.path.exists(a.unshield):
        sys.exit('unshield not found: install it (brew/apt install unshield) or pass --unshield /path/to/unshield')
    out = os.path.abspath(a.out)
    work = os.path.abspath(a.work or os.path.join(os.path.dirname(out), 'sims3ds_extract_work'))
    cabdir, groups_dir = os.path.join(work, 'cab'), os.path.join(work, 'groups')
    os.makedirs(cabdir, exist_ok=True)
    os.makedirs(out, exist_ok=True)

    discs = find_discs(a.discs)
    if not discs:
        sys.exit('no disc images found in ' + a.discs)
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
    r = subprocess.run([a.unshield, '-d', groups_dir, 'x', os.path.join(cabdir, 'data1.hdr')],
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace')
    tail = r.stdout.strip().splitlines()[-3:]
    for line in tail:
        log('unshield:', line)
    if r.returncode != 0:
        sys.exit('unshield failed (exit %d); are all data*.cab files present?' % r.returncode)

    layout(groups_dir, out, a.with_creator)
    ok = check(out)
    if not a.keep_work:
        shutil.rmtree(work, ignore_errors=True)
    log('done ->', out)
    log('copy the CONTENTS of this folder to sdmc:/3ds/sims/ (next to sims.3dsx), see README.md')
    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()
