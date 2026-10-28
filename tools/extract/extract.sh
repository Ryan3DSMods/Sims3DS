#!/usr/bin/env bash
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
# distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# Sims3DS game-data extractor (Linux; macOS uses extract.command, which runs this).
# Turns YOUR OWN copy of The Sims Complete Collection (images of your own discs, or your installed game folder)
# into a ready 3ds/sims/ folder for the SD card.
#
#   ./extract.sh                      asks for the folder
#   ./extract.sh <folder>             your disc images folder or your installed "The Sims" folder
#   ./extract.sh <folder> --out <dir> other options: see  python3 extract_complete_collection.py --help
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
SCRIPT="$HERE/extract_complete_collection.py"

finish() {
    if [ "${SIMS3DS_PAUSE:-0}" = "1" ]; then
        echo
        read -r -p "Press Enter to close this window." _ || true
    fi
    exit "$1"
}

echo "== Sims3DS extractor =="
PY=""
for c in python3 python; do
    if command -v "$c" >/dev/null 2>&1 && "$c" -c 'import sys; sys.exit(0 if sys.version_info >= (3, 8) else 1)' 2>/dev/null; then
        PY="$(command -v "$c")"; break
    fi
done
if [ -z "$PY" ]; then
    echo "Python 3.8 or newer is needed but was not found."
    case "$(uname -s)" in
        Darwin) echo "Install it from https://www.python.org/downloads/ (or: brew install python), then run this again."
                echo "(If macOS offers to install the 'command line developer tools', click Install - that includes Python.)" ;;
        *)      echo "Install it with your package manager, e.g.  sudo apt install python3 python3-venv" ;;
    esac
    finish 1
fi

# Python helpers for disc images: py7zr (.7z) and pycdlib (.iso/.bin). Not needed for an installed game folder,
# so a failed install is only a warning. Order: already there -> pip --user -> a private venv next to this script.
if ! "$PY" -c 'import py7zr, pycdlib' 2>/dev/null; then
    echo "Installing the Python helpers (py7zr, pycdlib) - one time only ..."
    if ! "$PY" -m pip install --user --quiet py7zr pycdlib 2>/dev/null; then
        VENV="$HERE/.venv"
        [ -w "$HERE" ] || VENV="$HOME/.sims3ds-extractor-venv"
        if { [ -x "$VENV/bin/python" ] || "$PY" -m venv "$VENV" >/dev/null 2>&1; } \
           && "$VENV/bin/python" -m pip install --quiet py7zr pycdlib; then
            PY="$VENV/bin/python"
        else
            echo "WARNING: could not install py7zr/pycdlib. That is fine for an installed game folder;"
            echo "         for disc images run:  $PY -m pip install --user py7zr pycdlib"
        fi
    fi
fi

"$PY" "$SCRIPT" "$@"
finish $?
