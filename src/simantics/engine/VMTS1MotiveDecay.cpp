/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Entities.VMTS1MotiveDecay
#include "simantics/engine/VMTS1MotiveDecay.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/VMContext.h"
#include "simantics/VMClock.h"
namespace FSO { namespace SimAntics { namespace Entities {
using namespace FSO::SimAntics::Model;

const float VMTS1MotiveDecay::Constants[17] = {
    180, //energy span 0
    16, //wake hours 1
    7, //wake hour 2
    0.01f, //energy drift 3
    0.3f, //hunger to bladder 4
    0.0021f, //hunger decrement ratio 5
    0.055f, //social decrement base 6
    0.000125f, //social decrement multiplier 7
    0.25f, //ent dec awake 8
    1.0f, //ent mult asleep 9
    0.17f, //hyg decrement awake 10
    0.08f, //hyg decrement asleep 11
    0.3f, //blad decrement awake 12
    0.15f, //blad decrement asleep 13
    0.4f, //comfort decrement active 14
    0.6f, //comfort decrement lazy 15
    0.5f, //comfort decrement 16
};

const VMMotive VMTS1MotiveDecay::DecrementMotives[7] = {
    VMMotive::Hunger, VMMotive::Comfort, VMMotive::Hygiene, VMMotive::Bladder, VMMotive::Energy, VMMotive::Fun, VMMotive::Social
};

void VMTS1MotiveDecay::Tick(VMAvatar* avatar, VMContext* context) {
    auto roomScore = context->GetRoomScore(context->GetRoomAt(avatar->Position()));
    avatar->SetMotiveData(VMMotive::Room, roomScore);
    if (context->Clock->Minutes / 2 == LastMinute || avatar->GetValue(VMStackObjectVariable::Hidden) > 0) return;
    auto hours = context->Clock->Hours;
    LastMinute = context->Clock->Minutes / 2;
    auto sleeping = (avatar->GetMotiveData(VMMotive::SleepState) != 0);

    int moodSum = 0;

    for (int i = 0; i < 7; i++) {
        int frac = 0;
        auto dm = DecrementMotives[i];
        if (avatar->HasMotiveChange(dm) && dm != VMMotive::Energy) continue;
        auto motive = avatar->GetMotiveData(dm);
        auto r_Hunger = ToFixed1000(Constants[5]) * (100 + avatar->GetMotiveData(VMMotive::Hunger));
        switch (i) {
            case 0:
                frac = r_Hunger; break;
            case 1: {
                auto active = avatar->GetPersonData(VMPersonDataVariable::ActivePersonality);
                if (active > 666)
                    frac = ToFixed1000(Constants[14]);
                else if (active < 666)
                    frac = ToFixed1000(Constants[15]);
                else
                    frac = ToFixed1000(Constants[16]);
                break;
            }
            case 2:
                frac = ToFixed1000(Constants[sleeping ? 11 : 10]); break;
            case 3:
                frac = ToFixed1000(Constants[sleeping ? 13 : 12]) + FracMul(r_Hunger, ToFixed1000(Constants[4])); break;
            case 4:
                // TS1(sims): FreeSO — try to wake the sim up if they're asleep on the wake hour, and they're well rested.
                if (sleeping && hours == Constants[2] && motive >= Constants[0] - 100) {
                    avatar->SetMotiveData(VMMotive::SleepState, 0);
                }
                if (sleeping) {
                    // TS1(sims): FreeSO restores energy (-643*2/1000 per 2 minutes, ~39/hour) for sleep state -1. The
                    // global 'failure sleep' (passing out on the floor or in a chair) sets sleep state -50 and loops until
                    // energy > 0 with no motive change of its own, so with only -1 handled a Sim who passed out in a chair
                    // never woke up. Inference: any negative sleep state is that kind of sleep.
                    if (avatar->GetMotiveData(VMMotive::SleepState) < 0) frac = -643 * 2;
                    else frac = (context->Clock->Hours >= Constants[2]) ? ToFixed1000(Constants[3]) : 0;
                } else {
                    frac = (ToFixed1000(Constants[0]) / (30 * (int)Constants[1]));
                }
                //energy span over wake hours. small energy drift applied if asleep during the day.
                break;
            case 5:
                frac = (sleeping) ? 0 : ToFixed1000(Constants[8]);
                break;
            case 6:
                frac = ToFixed1000(Constants[6]) +
                    ToFixed1000(Constants[7]) * avatar->GetPersonData(VMPersonDataVariable::OutgoingPersonality);
                break;
        }

        MotiveFractions[i] += (int16_t)frac;
        if (MotiveFractions[i] >= 1000) {
            motive -= (int16_t)(MotiveFractions[i] / 1000);
            MotiveFractions[i] %= 1000;
            if (motive < -100) motive = -100;
            avatar->SetMotiveData(DecrementMotives[i], motive);
        }
        if (MotiveFractions[i] < 0) { // TS1(sims): FreeSO — negative decrements (energy while asleep on the floor) raise the motive
            motive += (int16_t)(1 - MotiveFractions[i] / 1000);
            MotiveFractions[i] += (int16_t)((1 - MotiveFractions[i] / 1000) * 1000);
            avatar->SetMotiveData(DecrementMotives[i], motive);
        }
        moodSum += motive;
    }
    moodSum += roomScore;

    avatar->SetMotiveData(VMMotive::Mood, (int16_t)(moodSum / 8));
}
}}}
