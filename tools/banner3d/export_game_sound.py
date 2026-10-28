#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
# distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
"""Make a HOME Menu banner sound from YOUR OWN game data (no game audio is included in this repository).

Decodes a sound with the port's own sound engine (host tool build-port/simaudio: HIT events, XA/UTK/WAV/MP3) and
turns it into what `bannertool makebanner -a` wants: 44.1 kHz, 16-bit, stereo, at most 3 s, with a short fade-out.
The result goes to src/platform/3ds/banner3d/banner_sound.wav, which `make cia` picks up automatically (otherwise the
banner is silent). That file is ignored by git: keep it on your machine, do not publish it.

    python3 tools/banner3d/export_game_sound.py <installRoot> --event ui_object_place
    python3 tools/banner3d/export_game_sound.py <installRoot> --music 3 --start 0.5
    python3 tools/banner3d/export_game_sound.py <installRoot> --file <installRoot>/Music/Modes/Load/<some file>.mp3
    python3 tools/banner3d/export_game_sound.py <installRoot> --sample <name>.xa       (a sample inside the archives)

<installRoot> = the extracted game folder (the one with GameData/, SoundData/, Music/ ...).
Ideas (names only): HIT events `ui_object_place` / `ui_buy_place` (the cash-register purchase sound),
`ui_moneyback`, `ui_whoosh`; mode music `--music 3` (neighbourhood), `--music 5` (loading screen), `--music 1`
(buy mode); `simaudio <installRoot> info` lists every bank, event and sample your data has.

Needs: a host C++ compiler + zlib (the script runs `make -f Makefile.port build-port/simaudio` if the tool is
missing) and Python 3 with numpy.
"""
import argparse
import os
import subprocess
import sys
import tempfile
import wave

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, '..', '..'))
SIMAUDIO = os.path.join(REPO, 'build-port', 'simaudio')
OUT = os.path.join(REPO, 'src', 'platform', '3ds', 'banner3d', 'banner_sound.wav')
RATE = 44100


def read_wav(path):
    with wave.open(path, 'rb') as w:
        ch, sw, rate, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        raw = w.readframes(n)
    if sw != 2:
        raise SystemExit('%s: expected 16-bit PCM' % path)
    a = np.frombuffer(raw, '<i2').astype(np.float32).reshape(-1, ch)
    if ch == 1:
        a = np.repeat(a, 2, axis=1)
    return a[:, :2], rate


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('install_root')
    src = ap.add_mutually_exclusive_group(required=True)
    src.add_argument('--event', help='HIT sound event name (e.g. ui_object_place)')
    src.add_argument('--music', help='mode music number or music event name')
    src.add_argument('--file', help='a loose .mp3/.wav file of your install')
    src.add_argument('--sample', help='a sample inside the sound archives (.xa/.wav)')
    ap.add_argument('--start', type=float, default=0.0, help='skip this many seconds first')
    ap.add_argument('--length', type=float, default=3.0, help='max length in seconds (HOME Menu limit: 3)')
    ap.add_argument('--fade', type=float, default=0.4, help='fade-out length in seconds')
    ap.add_argument('--gain', type=float, default=0.0, help='extra gain in dB (default: normalise to -1 dBFS)')
    ap.add_argument('--out', default=OUT)
    a = ap.parse_args()
    a.length = min(a.length, 3.0)

    if not os.path.exists(SIMAUDIO):
        print('building build-port/simaudio ...', flush=True)
        subprocess.check_call(['make', '-f', 'Makefile.port', '-j4', 'build-port/simaudio'], cwd=REPO)
    tmp = tempfile.NamedTemporaryFile(suffix='.wav', delete=False).name
    span = '%g' % (a.start + a.length + 0.5)
    if a.event:
        cmd = [SIMAUDIO, a.install_root, 'event', a.event, tmp, span]
    elif a.music:
        cmd = [SIMAUDIO, a.install_root, 'music', a.music, tmp, span]
    elif a.file:
        cmd = [SIMAUDIO, a.install_root, 'playfile', a.file, tmp, span]
    else:
        cmd = [SIMAUDIO, a.install_root, 'decode', a.sample, tmp]
    env = dict(os.environ, SIMAUDIO_QUIET='1')
    subprocess.check_call(cmd, env=env)

    pcm, rate = read_wav(tmp)
    os.unlink(tmp)
    if rate != RATE:                                         # linear resample to 44.1 kHz
        n = int(round(len(pcm) * RATE / rate))
        x = np.linspace(0, len(pcm) - 1, n)
        pcm = np.stack([np.interp(x, np.arange(len(pcm)), pcm[:, c]) for c in range(2)], axis=1)
    pcm = pcm[int(a.start * RATE):]
    nz = np.nonzero(np.abs(pcm).max(axis=1) > 64)[0]         # drop leading silence
    if len(nz) == 0:
        raise SystemExit('the decoded sound is silent - try another event/file (see `simaudio <root> info`)')
    pcm = pcm[nz[0]:nz[0] + int(a.length * RATE)]
    nf = min(len(pcm), int(a.fade * RATE))
    if nf:
        pcm[-nf:] *= np.linspace(1.0, 0.0, nf)[:, None]
    peak = float(np.abs(pcm).max())
    gain = (10 ** (a.gain / 20)) if a.gain else (32767 * 0.89 / peak if peak else 1.0)
    out = np.clip(pcm * gain, -32768, 32767).astype('<i2')
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    with wave.open(a.out, 'wb') as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(out.tobytes())
    print('wrote %s: %.2f s, 44.1 kHz 16-bit stereo (personal use only - do not publish it)' % (a.out, len(out) / RATE))


if __name__ == '__main__':
    main()
