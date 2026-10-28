/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Entities.VMTS1MotiveDecay (engine/VMTS1MotiveDecay.cs)
#pragma once
#include <cstdint>
#include <array>
#include "simantics/entities/VMAvatarMotiveDecay.h"
namespace FSO { namespace SimAntics { namespace Entities {
class VMTS1MotiveDecay : public VMIMotiveDecay {
public:
    static const float Constants[17];
    static const Model::VMMotive DecrementMotives[7];
    std::array<int16_t, 7> MotiveFractions{}; //in 1/1000ths
    int32_t LastMinute = 0;
    void Tick(VMAvatar* avatar, VMContext* context) override;
    int32_t ToFixed1000(float input) { return (int32_t)(input * 1000); }
    int32_t FracMul(int32_t input, int32_t frac) { return (int32_t)((int64_t)input * frac) / 1000; }
};
}}}
