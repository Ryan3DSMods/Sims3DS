/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Model.VMMotiveChange
#include "simantics/model/VMMotiveChange.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"
#include "simantics/engine/VMThread.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/entities/VMAvatarMotiveDecay.h"
#include "simantics/engine/VMTS1MotiveDecay.h"
#include "simantics/model/VMTuningCache.h"
#include "content/ContentManager.h"
#include "simantics/model/TSOPlatform/VMTSOLotState.h"
#include <cmath>
#include <climits>

namespace FSO { namespace SimAntics { namespace Model {

void VMMotiveChange::Clear() {
    PerHourChange = 0;
    MaxValue = SHRT_MAX;
}

void VMMotiveChange::Tick(VMAvatar* avatar) {
    Ticked = true;
    if (PerHourChange != 0) {
        double rate = (PerHourChange / 60.0) / (30.0); //timed for 5 second minutes
        // TS1(sims): FreeSO — TS1 runs 30 ticks per game minute (see VMClock); TSO divides by 5 more.
        if (avatar->Thread == nullptr || !avatar->Thread->Context->VM->TS1) rate /= 5;
        fractional += rate;
        if (std::fabs(fractional) >= 1) {
            auto motive = avatar->GetMotiveData(Motive);
            if (((rate > 0) && (motive > MaxValue)) || ((rate < 0) && (motive < MaxValue))) { return; } //we're already over, do nothing. (do NOT clamp)
            motive += (int16_t)(fractional);
            fractional = std::fmod(fractional, 1.0);
            // TS1(sims): no clamp for The Sims 1. Its trees test "past the max": the bed sets energy max = tuning 90
            // and 'tweak sleep finished' wakes the Sim when energy > 90, which a clamp to 90 can never satisfy (Sims slept
            // until hunger/bladder failure). The change stops on the first step past the max (check above).
            if (!(avatar->Thread != nullptr && avatar->Thread->Context->VM->TS1))
            if (((rate > 0) && (motive > MaxValue)) || ((rate < 0) && (motive < MaxValue))) { motive = MaxValue; }
            //DO NOT CLEAR MOTIVE WHEN IT HITS MAX VALUE! fixes pet, maybe shower.
            avatar->SetMotiveData(Motive, motive);
        }
    }
}

int32_t VMMotiveChange::ScaleRate(VM* vm, int32_t rate, VMMotive type) {
    if (vm->TS1) {
        if (type == VMMotive::Energy && rate > 0) {
            rate *= (int32_t)Entities::VMTS1MotiveDecay::Constants[0] / (24 - (int32_t)Entities::VMTS1MotiveDecay::Constants[1]);
        }
        return rate;
    } else {
        if (rate < 0) return rate;
        auto& LotMotives = FSO::Content::Content::Get()->GlobalTuning->EntriesByName["lotmotives"];
        if (vm->TSOState()->PropertyCategory == 4 && (int)type > 0) rate = (rate * 3) / 2; //1.5x gain multiplier on services lots
        if (VMMotive::Comfort == type) return rate;
        int ind = -1;
        for (int i = 0; i < 7; i++) if (Entities::VMAvatarMotiveDecay::DecrementMotives[i] == type) { ind = i; break; }
        auto cat = vm->TSOState()->PropertyCategory;
        if (cat > 10) cat = 0;
        std::string category = Entities::VMAvatarMotiveDecay::CategoryNames[cat];
        auto weight = ToFixed1000(LotMotives.GetNum(category + "_" + ((ind < 0) ? std::string("") : Entities::VMAvatarMotiveDecay::LotMotiveNames[ind]) + "Weight"));
        if (weight == 0) return rate; // STUB(3ds): tuning.dat not loaded; avoid divide by zero
        return (rate * 1000) / weight;
    }
}

int16_t VMMotiveChange::ScaleMax(VM* vm, int16_t oldMax, VMMotive type) {
    return (int16_t)((oldMax - 100) + vm->TuningCache->GetLimit(type));
}

}}}
