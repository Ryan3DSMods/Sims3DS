/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// TS1(sims): writing The Sims 1 "Arry" lot grids (floors, walls, objects, flags, ...).
// FreeSims/FreeSO only read ARRY (ARRY.cpp). The encoder below produces the run-length form the game writes and the
// reader (ARRY::Read) decodes:
//   u32 0 | s32 width | s32 height | s32 bytes per element (1 floors/flags/pool, 2 objects, 4 altitude, 8 walls) | s32 1
//   then 16-bit little-endian words over the *bytes* of the grid (file order, see ARRY::TransposeData):
//     0x8000 | n, value, 0      n bytes of `value` (a run)
//     n (< 0x8000), n bytes, pad to even        literal bytes
//   The game starts with a literal block when the grid does not begin with a run, and uses runs of 8 bytes or more
//   (seen in the shipped houses); the same rule is used here.
#pragma once
#include <cstdint>
#include <vector>

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

/// data: the grid in file order (ARRY::Data), width * height * bytesPerElement bytes.
std::vector<uint8_t> WriteARRYChunk(int32_t width, int32_t height, int32_t bytesPerElement, const std::vector<uint8_t>& data,
                                    int32_t unknown = 1);

/// ARRY::TransposeData's operation (its own inverse for square grids): file order <-> loader order (y * width + x).
std::vector<uint8_t> TransposeARRY(const std::vector<uint8_t>& data, int32_t width, int32_t stride);

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
