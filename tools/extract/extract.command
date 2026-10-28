#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
# distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# Sims3DS game-data extractor for macOS: double-click this file in Finder.
# (Downloaded from the internet? The first time, right-click it > Open > Open.)
# It asks for the folder with YOUR OWN copy of The Sims Complete Collection (images of your own discs, or your
# installed game) and writes a ready 3ds/sims/ folder for the SD card. Disc images also need "unshield"
# (Homebrew: brew install unshield - the extractor offers to do it for you).
cd "$(dirname "$0")" || exit 1
# Homebrew is not on the PATH of a double-clicked script on Apple Silicon
for d in /opt/homebrew/bin /usr/local/bin; do
    case ":$PATH:" in *":$d:"*) ;; *) [ -d "$d" ] && PATH="$d:$PATH" ;; esac
done
export PATH
SIMS3DS_PAUSE=1 exec bash ./extract.sh "$@"
