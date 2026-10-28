#!/usr/bin/env bash
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
# distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# Build and pack the prebuilt release zip for players: dist/Sims3DS-<version>.zip  (see docs/RELEASING.md)
#
#   tools/release/make_release.sh 1.0                 # builds sims.3dsx + sims.cia, then packs the zip
#   tools/release/make_release.sh 1.0 --cia-mem 96    # same with the 96 MB CIA memory mode
#   tools/release/make_release.sh 1.0 --flat-banner   # flat HOME Menu banner instead of the 3D one
#
# Needs devkitPro (3ds-dev), makerom + bannertool in PATH, zip; the 3D banner also needs git, Python >= 3.12 and
# network on its first run (otherwise the flat banner is used automatically).
# `make release` in src/platform/3ds calls this script with --package-only after building.
#
# Zip contents (everything that goes on the SD card except the game data, plus the extractor for the PC):
#   README.txt                         quick start
#   3ds/sims/sims.3dsx                 Homebrew Launcher version
#   3ds/sims/README-on-SD.txt          where the game folders go
#   cia/sims.cia                       install with FBI
#   extras/debug                       optional empty file: copy to sdmc:/3ds/sims/ to get a log.txt
#   extractor/extract.command|.bat|.sh + extract_complete_collection.py + convert_intro.py
#   licenses/                          LICENSE, CREDITS.md, logo attribution, font licence
# Never add game data or game audio here: the banner sound of a release is always the silent one.
set -euo pipefail

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
PLAT="$REPO/src/platform/3ds"
VERSION="" PACKAGE_ONLY=0 CIA_MEM=124 BANNER3D=1 OUTDIR="$REPO/dist"
F3DSX="$PLAT/sims.3dsx" FCIA="$PLAT/sims.cia"

usage() { sed -n '5,13p' "$0" | sed 's/^# \{0,1\}//'; exit 2; }
while [ $# -gt 0 ]; do
    case "$1" in
        --version) VERSION="$2"; shift 2 ;;
        --package-only) PACKAGE_ONLY=1; shift ;;
        --cia-mem) CIA_MEM="$2"; shift 2 ;;
        --flat-banner) BANNER3D=0; shift ;;
        --3dsx) F3DSX="$2"; shift 2 ;;
        --cia) FCIA="$2"; shift 2 ;;
        --out) OUTDIR="$2"; shift 2 ;;
        -h|--help) usage ;;
        -*) echo "make_release: unknown option $1" >&2; usage ;;
        *) VERSION="$1"; shift ;;
    esac
done
VERSION="${VERSION#v}"
[ -n "$VERSION" ] || { echo "make_release: give a version, e.g. tools/release/make_release.sh 1.0" >&2; exit 2; }

if [ "$PACKAGE_ONLY" = "0" ]; then
    [ -n "${DEVKITARM:-}" ] || { echo "make_release: DEVKITARM not set - run: source /etc/profile.d/devkit-env.sh" >&2; exit 1; }
    for t in makerom bannertool zip; do
        command -v "$t" >/dev/null 2>&1 || { echo "make_release: '$t' not found in PATH (see docs/RELEASING.md)" >&2; exit 1; }
    done
    make -C "$PLAT" -j4
    exec make -C "$PLAT" release VERSION="$VERSION" CIA_MEM="$CIA_MEM" BANNER3D="$BANNER3D"
fi

command -v zip >/dev/null 2>&1 || { echo "make_release: 'zip' not found" >&2; exit 1; }
for f in "$F3DSX" "$FCIA"; do
    [ -s "$f" ] || { echo "make_release: missing $f (build it first)" >&2; exit 1; }
done

NAME="Sims3DS-$VERSION"
STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT
R="$STAGE/$NAME"
mkdir -p "$R/3ds/sims" "$R/cia" "$R/extras" "$R/extractor" "$R/licenses"

cp "$F3DSX" "$R/3ds/sims/sims.3dsx"
cp "$FCIA" "$R/cia/sims.cia"
: > "$R/extras/debug"
cp "$REPO/tools/extract/extract.command" "$REPO/tools/extract/extract.sh" "$REPO/tools/extract/extract.bat" \
   "$REPO/tools/extract/extract_complete_collection.py" "$REPO/src/app/tools/convert_intro.py" "$R/extractor/"
chmod 755 "$R/extractor/extract.command" "$R/extractor/extract.sh"
cp "$REPO/LICENSE" "$R/licenses/LICENSE.txt"
cp "$REPO/CREDITS.md" "$R/licenses/CREDITS.md"
cp "$REPO/assets/logo/ATTRIBUTION.md" "$R/licenses/LOGO-ATTRIBUTION.md"
cp "$REPO/LICENSES/"* "$R/licenses/" 2>/dev/null || true

# Windows line endings for the .txt files and the .bat (Notepad / cmd.exe)
crlf() { sed 's/$/\r/' "$1" > "$1.tmp" && mv "$1.tmp" "$1"; }

cat > "$R/3ds/sims/README-on-SD.txt" <<'EOF'
Sims3DS - this folder is sdmc:/3ds/sims/ on your SD card
=========================================================

Your game folders go HERE, next to sims.3dsx:

  GameData/  UserData/  UserData2/  ExpansionPack/  ExpansionPack2/ ... ExpansionPack7/
  ExpansionShared/  Deluxe/  Downloads/  UIGraphics/  Music/  SoundData/  Template*/
  cache/intro.stv + cache/intro.wav   (the converted intro, optional)

The extractor (extractor/extract.command, extract.bat or extract.sh) puts them here for you,
from YOUR OWN copy of The Sims Complete Collection (your discs or your installed PC game).

The game creates Saves/ and cache/ itself on the first start (takes 1-2 minutes once).

Optional: copy the empty file extras/debug into this folder to get a log.txt for bug reports.
EOF
crlf "$R/3ds/sims/README-on-SD.txt"

cat > "$R/README.txt" <<EOF
Sims3DS $VERSION - The Sims (2000) for the New Nintendo 3DS (unofficial fan project)
====================================================================================

No game files are included. You need your own copy of The Sims Complete Collection
(your discs, or the game installed on your PC).

1. Run the extractor on your own game (on your computer):
     macOS:   double-click extractor/extract.command
              (first time: right-click it > Open, because it was downloaded)
     Windows: double-click extractor\\extract.bat (needs Python from python.org)
     Linux:   run extractor/extract.sh
   Point it at the folder with images of your discs, or at your installed
   "The Sims" folder. It fills 3ds/sims/ in THIS folder with the game data.

2. Copy the folders 3ds and cia from this folder to the root of the SD card.

3. On the 3DS: FBI > SD > cia > sims.cia > Install CIA.

4. Start "sims3ds" from the HOME Menu. The first start takes 1-2 minutes.

Tip: close the game with START > Quit (pressing HOME while playing can crash
the HOME Menu - being investigated).

Help, controls, known issues: https://github.com/Ryan3DSMods/Sims3DS
Not affiliated with Electronic Arts or Maxis. "The Sims" is a trademark of
Electronic Arts Inc. Licences: see the licenses folder.
EOF
crlf "$R/README.txt"
crlf "$R/extractor/extract.bat"

find "$STAGE" -name '.DS_Store' -delete
mkdir -p "$OUTDIR"
ZIP="$OUTDIR/$NAME.zip"
rm -f "$ZIP"
# the files sit at the zip's root so "copy everything to the SD card" just works; -X: no extra macOS attributes
(cd "$R" && zip -q -r -X "$ZIP" .)
echo "make_release: CIA memory mode CIA_MEM=$CIA_MEM"
echo "make_release: $(unzip -Z1 "$ZIP" | grep -vc '/$') files:"
unzip -Z1 "$ZIP" | grep -v '/$' | sort | sed 's/^/  /'
echo "make_release: OK -> $ZIP  ($(du -h "$ZIP" | cut -f1))"
