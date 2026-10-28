/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.Files.Utils.IoBuffer / IoWriter / ByteOrder are provided by st::Reader / st::Writer
// (src/core/io.h). This header only supplies the C# names inside the FSO namespace so
// translated code reads the same.
#pragma once
#include "core/io.h"

namespace FSO { namespace Files { namespace Utils {

using IoBuffer = st::Reader;
using IoWriter = st::Writer;
using ByteOrder = st::Endian;   // ByteOrder::Big / ByteOrder::Little (BIG_ENDIAN/LITTLE_ENDIAN are libc macros)
using SeekOrigin = st::SeekOrigin;

}}} // namespace FSO::Files::Utils
