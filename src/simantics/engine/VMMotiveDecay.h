/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// tso.simantics.Engine.VMMotiveDecay (engine/VMMotiveDecay.cs — legacy TSO decay, not in the FreeSims csproj;
// translated for completeness). Uses TuningEntry (tuning.dat), which is a STUB(3ds) returning 0.
#pragma once
#include <cstdint>
#include <array>
#include "simantics/model/VMMotive.h"
#include "content/ContentManager.h"
namespace FSO { namespace SimAntics {
class VMAvatar; class VMContext;
namespace Engine {
class VMMotiveDecay {
public:
    static const char* const LotMotiveNames[7];
    static const Model::VMMotive DecrementMotives[7];
    std::array<int16_t, 7> MotiveFractions{}; //in 1/1000ths
    int32_t LastMinute = 0;
    void Tick(VMAvatar* avatar, VMContext* context);
private:
    FSO::Content::TuningEntry LotMotives; //Content.Get().GlobalTuning.EntriesByName["lotmotives"];
    FSO::Content::TuningEntry SimMotives; //Content.Get().GlobalTuning.EntriesByName["simmotives"];
};
}}}
