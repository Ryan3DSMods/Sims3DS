/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Entities.VMIMotiveDecay / VMAvatarMotiveDecay (TSO motive decay; uses tuning.dat lotmotives/simmotives)
#pragma once
#include <cstdint>
#include <array>
#include <vector>
#include <string>
#include "simantics/model/VMMotive.h"
namespace FSO { namespace Content { class TuningEntry; } }
namespace FSO { namespace SimAntics {
class VMAvatar; class VMContext;
namespace Entities {
class VMIMotiveDecay {
public:
    virtual ~VMIMotiveDecay() {}
    virtual void Tick(VMAvatar* avatar, VMContext* context) = 0;
};

class VMAvatarMotiveDecay : public VMIMotiveDecay {
public:
    static const char* const LotMotiveNames[7];
    static const char* const CategoryNames[11];
    static const Model::VMMotive DecrementMotives[7];

    std::array<int16_t, 7> MotiveFractions{}; //in 1/1000ths
    int32_t LastMinute = 0;
    int32_t LastCategory = -1;
    std::vector<int32_t> LotMuls;
    std::vector<int32_t> FlatSimMotives;

    VMAvatarMotiveDecay();
    void UpdateCategory(VMContext* context);
    void Tick(VMAvatar* avatar, VMContext* context) override;
    int32_t ToFixed1000(float input) { return (int32_t)(input * 1000); }
    int32_t FracMul(int32_t input, int32_t frac) { return (int32_t)((int64_t)input * frac) / 1000; }
private:
    FSO::Content::TuningEntry* LotMotives;
    FSO::Content::TuningEntry* SimMotives;
};
}}}
