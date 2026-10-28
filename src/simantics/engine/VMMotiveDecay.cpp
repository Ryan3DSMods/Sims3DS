/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// tso.simantics.Engine.VMMotiveDecay
#include "simantics/engine/VMMotiveDecay.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/VMContext.h"
#include "simantics/VMClock.h"
namespace FSO { namespace SimAntics { namespace Engine {
using namespace FSO::SimAntics::Model;
const char* const VMMotiveDecay::LotMotiveNames[7] = { "Hunger", "Comfort", "Hygiene", "Bladder", "Energy", "Fun", "Social" };
const VMMotive VMMotiveDecay::DecrementMotives[7] = { VMMotive::Hunger, VMMotive::Comfort, VMMotive::Hygiene, VMMotive::Bladder, VMMotive::Energy, VMMotive::Fun, VMMotive::Social };

void VMMotiveDecay::Tick(VMAvatar* avatar, VMContext* context) {
    if (context->Clock->Minutes == LastMinute) return;
    LastMinute = context->Clock->Minutes;

    std::string category = "Skills";
    std::string sleepState = (avatar->GetMotiveData(VMMotive::SleepState) == 0) ? "Awake" : "Asleep";

    int moodSum = 0;
    for (int i = 0; i < 7; i++) {
        float lotMul = LotMotives.GetNum(category + "_" + LotMotiveNames[i] + "Weight");
        float frac = 0;
        auto motive = avatar->GetMotiveData(DecrementMotives[i]);
        auto r_Hunger = (SimMotives.GetNum("HungerDecrementRatio") * (100 + avatar->GetMotiveData(VMMotive::Hunger))) * LotMotives.GetNum(category + "_HungerWeight");
        switch (i) {
            case 0:
                frac = r_Hunger; break;
            case 1:
                frac = (SimMotives.GetNum("ComfortDecrementActive") * lotMul); break;
            case 2:
                frac = (SimMotives.GetNum("HygieneDecrement" + sleepState) * lotMul); break;
            case 3:
                frac = (SimMotives.GetNum("BladderDecrement" + sleepState) * lotMul) + (SimMotives.GetNum("HungerToBladderMultiplier") * r_Hunger); break;
            case 4: {
                float wake = SimMotives.GetNum("WakeHours");
                frac = (wake == 0) ? 0 : (SimMotives.GetNum("EnergySpan") / (60 * wake));
                // TODO: wrong but appears to be close? need one which uses energy weight, which is about 2.4 on skills
                break;
            }
            case 5:
                frac = (sleepState == "Asleep") ? 0 : (SimMotives.GetNum("EntDecrementAwake") * lotMul);
                break;
            case 6:
                frac = (SimMotives.GetNum("SocialDecrementBase") + (SimMotives.GetNum("SocialDecrementMultiplier") * (100 + motive))) * lotMul;
                frac /= 2; //make this less harsh right now, til I can work out how multiplayer bonus is meant to work
                break;
        }

        MotiveFractions[i] += (int16_t)(frac * 1000);
        if (MotiveFractions[i] >= 1000) {
            motive -= (int16_t)(MotiveFractions[i] / 1000);
            MotiveFractions[i] %= 1000;
            if (motive < -100) motive = -100;
            avatar->SetMotiveData(DecrementMotives[i], motive);
        }
        moodSum += motive;
    }

    avatar->SetMotiveData(VMMotive::Mood, (int16_t)(moodSum / 7));
}
}}}
