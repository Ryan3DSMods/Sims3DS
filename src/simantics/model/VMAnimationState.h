/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Model.VMAnimationState
#pragma once
#include <cstdint>
#include <vector>
#include <algorithm>
#include "vitaboy/Animation.h"
#include "simantics/utils/TimePropertyListItemSorter.h"
namespace FSO { namespace SimAntics { namespace Model {
class VMAnimationState {
public:
    FSO::Vitaboy::Animation* Anim;
    float CurrentFrame = 0;
    std::vector<int16_t> EventQueue;
    uint8_t EventsRun = 0; //total # of xevts fired during anim. up to short if anything uses more than 255.
    bool EndReached = false;
    bool PlayingBackwards = false;
    float Speed = 1.0f;
    float Weight = 1.0f; //For animation blending. All active animations should add up to 1 but won't break if it doesn't.
    bool Loop = false;
    std::vector<FSO::Vitaboy::TimePropertyListItem> TimePropertyLists;

    VMAnimationState(FSO::Vitaboy::Animation* animation, bool backwards) : Anim(animation) {
        if (backwards) {
            PlayingBackwards = true;
            CurrentFrame = (float)Anim->NumFrames();
        }
        for (auto& motion : animation->Motions) {
            for (auto& tp : motion.TimeProperties) {
                for (auto& item : tp.Items) {
                    TimePropertyLists.push_back(item);
                }
            }
        }
        /** Sort time property lists by time **/
        std::stable_sort(TimePropertyLists.begin(), TimePropertyLists.end(), Utils::TimePropertyListItemSorter());
    }
};
}}}
