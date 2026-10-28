/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Entities.VMAvatarMotiveDecay
#include "simantics/entities/VMAvatarMotiveDecay.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/model/TSOPlatform/VMTSOLotState.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"
#include "simantics/VMClock.h"
#include "content/ContentManager.h"
namespace FSO { namespace SimAntics { namespace Entities {
using namespace FSO::SimAntics::Model;

const char* const VMAvatarMotiveDecay::LotMotiveNames[7] = { "Hunger", "Comfort", "Hygiene", "Bladder", "Energy", "Fun", "Social" };
const char* const VMAvatarMotiveDecay::CategoryNames[11] = { "None", "Money", "Offbeat", "Romance", "Services", "Shopping", "Skills", "Welcome", "Games", "Entertain", "Residence" };
const VMMotive VMAvatarMotiveDecay::DecrementMotives[7] = { VMMotive::Hunger, VMMotive::Comfort, VMMotive::Hygiene, VMMotive::Bladder, VMMotive::Energy, VMMotive::Fun, VMMotive::Social };

VMAvatarMotiveDecay::VMAvatarMotiveDecay() {
    auto& tuning = FSO::Content::Content::Get()->GlobalTuning->EntriesByName;
    LotMotives = &tuning["lotmotives"];
    SimMotives = &tuning["simmotives"];
    FlatSimMotives = {
        ToFixed1000(SimMotives->GetNum("HungerDecrementRatio")), //0
        ToFixed1000(SimMotives->GetNum("ComfortDecrementActive")), //1
        ToFixed1000(SimMotives->GetNum("HygieneDecrementAsleep")), //2
        ToFixed1000(SimMotives->GetNum("HygieneDecrementAwake")), //3
        ToFixed1000(SimMotives->GetNum("BladderDecrementAsleep")), //4
        ToFixed1000(SimMotives->GetNum("BladderDecrementAwake")), //5
        ToFixed1000(SimMotives->GetNum("HungerToBladderMultiplier")), //6
        ToFixed1000(SimMotives->GetNum("EnergySpan")), //7
        (int32_t)SimMotives->GetNum("WakeHours"), //8
        ToFixed1000(SimMotives->GetNum("EntDecrementAwake")), //9
        ToFixed1000(SimMotives->GetNum("SocialDecrementBase")), //10
        ToFixed1000(SimMotives->GetNum("SocialDecrementMultiplier")), //11
    };
}

void VMAvatarMotiveDecay::UpdateCategory(VMContext* context) {
    auto cat = context->VM->TSOState()->PropertyCategory;
    if (cat != LastCategory) {
        LotMuls.assign(7, 0);
        std::string category = CategoryNames[(cat == 255 || cat > 10) ? 0 : cat];
        for (int i = 0; i < 7; i++) {
            LotMuls[i] = ToFixed1000(LotMotives->GetNum(category + "_" + LotMotiveNames[i] + "Weight"));
        }
        LastCategory = cat;
    }
}

void VMAvatarMotiveDecay::Tick(VMAvatar* avatar, VMContext* context) {
    auto roomScore = context->GetRoomScore(context->GetRoomAt(avatar->Position()));
    avatar->SetMotiveData(VMMotive::Room, roomScore);
    if (context->Clock->Minutes == LastMinute) return;
    if (avatar->GetPersonData(VMPersonDataVariable::Cheats) > 0) return;
    LastMinute = context->Clock->Minutes;

    UpdateCategory(context);
    int sleepState = (avatar->GetMotiveData(VMMotive::SleepState) == 0) ? 1 : 0;

    int moodSum = 0;
    for (int i = 0; i < 7; i++) {
        int lotMul = LotMuls[i];
        int frac = 0;
        auto motive = avatar->GetMotiveData(DecrementMotives[i]);
        auto r_Hunger = FracMul(FlatSimMotives[0] * (100 + avatar->GetMotiveData(VMMotive::Hunger)), LotMuls[0]);
        switch (i) {
            case 0:
                frac = r_Hunger; break;
            case 1:
                frac = FracMul(FlatSimMotives[1], lotMul); break;
            case 2:
                frac = FracMul(FlatSimMotives[2 + sleepState], lotMul); break;
            case 3:
                frac = FracMul(FlatSimMotives[4 + sleepState], lotMul) + FracMul(r_Hunger, FlatSimMotives[6]); break;
            case 4:
                frac = (FlatSimMotives[8] == 0) ? 0 : (FlatSimMotives[7] / (60 * FlatSimMotives[8]));
                // TODO: wrong but appears to be close? need one which uses energy weight, which is about 2.4 on skills
                break;
            case 5:
                frac = (sleepState == 0) ? 0 : FracMul(FlatSimMotives[9], lotMul);
                break;
            case 6:
                frac = FlatSimMotives[10] +
                    FracMul((FlatSimMotives[11] * (100 + motive)), lotMul);
                frac /= 2; //make this less harsh right now, til I can work out how multiplayer bonus is meant to work
                break;
        }

        MotiveFractions[i] += (int16_t)frac;
        if (MotiveFractions[i] >= 1000) {
            motive -= (int16_t)(MotiveFractions[i] / 1000);
            MotiveFractions[i] %= 1000;
            if (motive < -100) motive = -100;
            avatar->SetMotiveData(DecrementMotives[i], motive);
        }
        moodSum += motive;
    }
    moodSum += roomScore;

    avatar->SetMotiveData(VMMotive::Mood, (int16_t)(moodSum / 8));
}
}}}
