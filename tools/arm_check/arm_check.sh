#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
# distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# Compile-only check of every 3DS source file with the 3DS CPU flags (PORTING.md), WITHOUT devkitPro: stock
# arm-none-eabi-g++ + libctru's headers + a tiny shim for the two devkitARM-newlib headers stock newlib lacks
# (sys/iosupport.h, sys/dirent.h). Nothing is linked; for a real build use devkitPro (src/platform/3ds/Makefile).
#
#   tools/arm_check/arm_check.sh <path to libctru/include> [jobs]
#   (libctru headers: git clone https://github.com/devkitPro/libctru  ->  libctru/libctru/include)
set -u
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
CTRU_INC="${1:?usage: arm_check.sh <libctru include dir> [jobs]}"
JOBS="${2:-4}"
SHIM="$REPO/tools/arm_check/shim"
FLAGS="-march=armv6k -mtune=mpcore -mfloat-abi=hard -mfpu=vfp -mtp=soft -std=gnu++17 -fexceptions -frtti -D__3DS__
       -DST_FILES_REAL -DST_HAVE_DIRENT -I$SHIM -I$REPO/src -I$CTRU_INC -O1 -w"
cd "$REPO"
LIST=$(find src/files src/simantics src/content src/world src/vitaboy src/common src/app src/hit src/audio src/platform/3ds \
       -name '*.cpp' ! -name selftest.cpp | sort)
export FLAGS
printf '%s\n' $LIST | xargs -P "$JOBS" -I{} sh -c 'arm-none-eabi-g++ $FLAGS -c "{}" -o /dev/null 2>/tmp/armchk.$$.err && echo "OK {}" || { echo "FAIL {}"; head -15 /tmp/armchk.$$.err; }; rm -f /tmp/armchk.$$.err' > "${TMPDIR:-/tmp}/arm_check.txt"
ok=$(grep -c '^OK ' "${TMPDIR:-/tmp}/arm_check.txt"); fail=$(grep -c '^FAIL ' "${TMPDIR:-/tmp}/arm_check.txt")
grep -A15 '^FAIL ' "${TMPDIR:-/tmp}/arm_check.txt"
echo "ARM compile check: $ok OK, $fail failed (of $(printf '%s\n' $LIST | wc -l))"
[ "$fail" = 0 ]
