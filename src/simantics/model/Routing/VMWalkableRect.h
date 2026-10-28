/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Model.Routing.VMWalkableRect
#pragma once
#include <vector>
#include <array>
#include "core/math.h"
#include "simantics/model/Routing/VMObstacle.h"
#include "simantics/model/Routing/VMFreeList.h"
namespace FSO { namespace SimAntics { namespace Model { namespace Routing {
class VMWalkableRect : public VMObstacle {
public:
    std::array<VMFreeList, 4> Free;
    std::vector<VMWalkableRect*> Adj;
    //used directly for routing
    VMWalkableRect* Parent = nullptr;
    st::Point ParentSource;
    int32_t OriginalG = 0;
    int32_t FScore = 0;
    int32_t GScore = 0;
    bool Start = false;
    uint8_t State = 0; //0 = untouched, 1 = open, 2 = closed;
    VMWalkableRect(int32_t x1, int32_t y1, int32_t x2, int32_t y2) : VMObstacle(x1, y1, x2, y2) {}
};
}}}}
