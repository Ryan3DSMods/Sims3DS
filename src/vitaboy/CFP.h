/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.Vitaboy.CFP (sims.common/rendering/vitaboy/CFP.cs)
#pragma once
#include <cstdint>
#include <functional>
#include <vector>
#include "core/io.h"

namespace FSO { namespace Vitaboy {

class Animation;

/// <summary>
/// Compressed Floating Point
///
/// Used to contain the animation and translation vectors for animations
/// separately from their "head" information. This makes it easier for the
/// TS1 content system to scan them without loading all of the animation data.
/// </summary>
class CFP {
public:
    static const float* Delta(); // 256-entry table, built on first use

    std::vector<uint8_t> Data;

    void Read(const std::vector<uint8_t>& stream) { Data = stream; }
    void Read(std::vector<uint8_t>&& stream) { Data = std::move(stream); }

    void EnrichAnim(Animation* anim);

    template <class F>
    static void ReadNFloats(st::Reader& io, int32_t floats, F output);
};

}}
