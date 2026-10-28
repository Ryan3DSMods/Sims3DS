#!/usr/bin/env bash
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
# distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
# Build the 3D HOME Menu "extended banner" for the CIA (called by `make cia BANNER3D=1`).
# Scene: banner_scene*.gltf (CC BY-SA 4.0, made by tools/banner3d/make_scene.py from assets/logo/; see ATTRIBUTION.md).
#
#   banner_scene.gltf  --pycgfx (skyfloogle, pinned commit) + convert_banner3d.py patches-->  banner3d.cgfx
#   banner3d.cgfx + banner_sound.wav (fallback banner_silence.wav)  --bannertool makebanner -ci-->  banner.bnr
#   icon.png  --bannertool makesmdh -f ...,extendedbanner-->  sims_cia.smdh   (the flag is REQUIRED for 3D)
#
# Usage: banner3d/make_banner3d.sh <output dir>      (default: build)
#        BANNER3D_STATIC=1 banner3d/make_banner3d.sh ...  -> use the static (non-swaying) logo directly
# Needs: git, python3 (3.12+ for pycgfx: it uses itertools.batched), bannertool in PATH, network on first run.
# Exit status != 0 -> the Makefile falls back to the flat banner.png.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"          # src/platform/3ds/banner3d
PLAT="$(cd "$HERE/.." && pwd)"                 # src/platform/3ds
OUT="${1:-$PLAT/build}"
case "$OUT" in /*) ;; *) OUT="$PLAT/$OUT" ;; esac
TOOLS="$PLAT/.tools"
PYCGFX="$TOOLS/pycgfx"
PYCGFX_REPO="https://github.com/skyfloogle/pycgfx.git"
PYCGFX_COMMIT="1f78850086f3a77c41e07162e842f97a5bf3c18a"
SMDH_FLAGS="visible,allow3d,recordusage,extendedbanner"

if [ ! -f "$HERE/banner_scene_static.gltf" ]; then
    echo "make_banner3d: no banner_scene*.gltf - run tools/banner3d/make_scene.py first" >&2
    exit 1
fi
mkdir -p "$OUT" "$TOOLS"

# 1. pycgfx at the commit the GBAtemp guide (Epic0522) was hardware-tested with
if [ ! -f "$PYCGFX/main.py" ]; then
    git clone "$PYCGFX_REPO" "$PYCGFX"
fi
if [ "$(git -C "$PYCGFX" rev-parse HEAD)" != "$PYCGFX_COMMIT" ]; then
    git -C "$PYCGFX" fetch --quiet origin || true
    git -C "$PYCGFX" checkout --quiet "$PYCGFX_COMMIT"
fi

# 2. venv with gltflib + pillow (pycgfx needs Python >= 3.12)
PY=""
for c in python3.13 python3.12 python3; do
    if command -v "$c" >/dev/null 2>&1 && "$c" -c 'import sys; sys.exit(0 if sys.version_info >= (3, 12) else 1)'; then
        PY="$c"; break
    fi
done
if [ -z "$PY" ]; then
    echo "make_banner3d: need Python >= 3.12 for pycgfx (brew install python@3.12)" >&2
    exit 1
fi
if [ ! -x "$PYCGFX/.venv/bin/python" ]; then
    "$PY" -m venv "$PYCGFX/.venv"
fi
if ! "$PYCGFX/.venv/bin/python" -c 'import gltflib, PIL' 2>/dev/null; then
    "$PYCGFX/.venv/bin/pip" install --quiet gltflib pillow
fi

# 3. glTF -> CGFX (+ matte / mesh-node / validation patches)
#    banner_scene.gltf = model swaying +-SWAY_DEG (rigid node animation, CANM COMMON); if that conversion or its
#    checks fail, banner_scene_static.gltf = same logo without animation (no rotation at all)
SCENE="$HERE/banner_scene.gltf"
[ -f "$SCENE" ] || SCENE="$HERE/banner_scene_static.gltf"
[ "${BANNER3D_STATIC:-0}" = "1" ] && SCENE="$HERE/banner_scene_static.gltf"
if ! "$PYCGFX/.venv/bin/python" "$HERE/convert_banner3d.py" \
        "$SCENE" "$OUT/banner3d.cgfx" --pycgfx-root "$PYCGFX"; then
    echo "make_banner3d: animated banner failed - trying the static logo (banner_scene_static.gltf)" >&2
    "$PYCGFX/.venv/bin/python" "$HERE/convert_banner3d.py" \
        "$HERE/banner_scene_static.gltf" "$OUT/banner3d.cgfx" --pycgfx-root "$PYCGFX"
fi

# 3b. independent structural check with our own parser (informational, never fatal)
CHECK="$PLAT/../../../tools/banner3d/cgfx_parse.py"
if [ -f "$CHECK" ]; then
    "$PYCGFX/.venv/bin/python" "$CHECK" --summary "$OUT/banner3d.cgfx" || echo "make_banner3d: (cgfx_parse reported issues - informational only)"
fi

# 4. banner (CBMD + CWAV; bannertool LZ11-compresses the CGFX) and SMDH with the extendedbanner flag
# banner sound: your own banner_sound.wav (<= 3 s, 44.1 kHz 16-bit stereo; none is included); silence if missing
# BANNER_AUDIO=<wav> overrides it (`make release` passes banner_silence.wav so no game audio is ever published)
AUDIO="${BANNER_AUDIO:-$HERE/banner_sound.wav}"
[ -f "$AUDIO" ] || AUDIO="$HERE/banner_silence.wav"
bannertool makebanner -ci "$OUT/banner3d.cgfx" -a "$AUDIO" -o "$OUT/banner.bnr"
bannertool makesmdh -s "sims3ds" -l "Sims 1 3DS (unofficial) - needs your own game data" -p "sims3ds contributors" \
    -i "${ICON_PNG:-$PLAT/icon.png}" -f "$SMDH_FLAGS" -o "$OUT/sims_cia.smdh"
echo "make_banner3d: OK -> $OUT/banner.bnr (audio $(basename "$AUDIO")) + $OUT/sims_cia.smdh (flags $SMDH_FLAGS)"
