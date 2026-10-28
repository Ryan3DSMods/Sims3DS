/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.Vitaboy.CFP (sims.common/rendering/vitaboy/CFP.cs)
#include "vitaboy/CFP.h"
#include "vitaboy/Animation.h"
#include <cmath>

namespace FSO { namespace Vitaboy {

const float* CFP::Delta() {
    // Enumerable.Range(0, 256).Select(x => (float)(3.9676e-10 * (Math.Pow((double)x - 126, 3) * Math.Abs(x - 126))))
    static float table[256];
    static bool built = false;
    if (!built) {
        for (int x = 0; x < 256; x++)
            table[x] = (float)(3.9676e-10 * (std::pow((double)x - 126, 3) * std::abs(x - 126)));
        built = true;
    }
    return table;
}

template <class F>
void CFP::ReadNFloats(st::Reader& io, int32_t floats, F output) {
    const float* delta = Delta();
    float lastValue = 0;
    for (int32_t i = 0; i < floats; i++) {
        auto code = io.ReadByte();
        switch (code) {
            case 0xFF:
                lastValue = io.ReadFloat();
                break;
            case 0xFE: {
                //repeat count
                auto repeats = io.ReadUInt16();
                for (int32_t j = 0; j < repeats; j++) {
                    if (i < floats) output(i, lastValue);
                    i++;
                }
                break;
            }
            default:
                lastValue += delta[code];
                break;
        }

        if (i < floats) output(i, lastValue);
    }
}

void CFP::EnrichAnim(Animation* anim) {
    st::Reader io(Data, st::Endian::Little);

    anim->Translations.assign(anim->TranslationCount, Vector3());
    anim->Rotations.assign(anim->RotationCount, Quaternion(0, 0, 0, 0));
    //read the floating point values and give them to the anim.
    //the anim supplies us with the count for the translation and rotation vector arrays.
    auto& T = anim->Translations;
    auto& R = anim->Rotations;
    int32_t tc = (int32_t)T.size(), rc = (int32_t)R.size();
    ReadNFloats(io, tc, [&](int32_t i, float x) { T[i].X = -x; });
    ReadNFloats(io, tc, [&](int32_t i, float y) { T[i].Y = y; });
    ReadNFloats(io, tc, [&](int32_t i, float z) { T[i].Z = z; });

    ReadNFloats(io, rc, [&](int32_t i, float x) { R[i].X = x; });
    ReadNFloats(io, rc, [&](int32_t i, float y) { R[i].Y = -y; });
    ReadNFloats(io, rc, [&](int32_t i, float z) { R[i].Z = -z; });
    ReadNFloats(io, rc, [&](int32_t i, float w) { R[i].W = -w; });
    //Data = null;
}

}}
