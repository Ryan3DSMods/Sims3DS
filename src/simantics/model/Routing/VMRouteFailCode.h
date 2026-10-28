/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Model.Routing.VMRouteFailCode
#pragma once
#include <cstdint>
namespace FSO { namespace SimAntics { namespace Model { namespace Routing {
enum class VMRouteFailCode : int16_t {
    Success = 0, Unknown = 1, NoRoomRoute = 2,
    NoPath = 3, //pathfind failed
    Interrupted = 4, CantSit = 5,
    CantStand = 6, //with blocking object
    NoValidGoals = 7, DestTileOccupied = 8,
    DestChairOccupied = 9, //with blocking object
    NoChair = 10, WallInWay = 11, AltsDontMatch = 12,
    DestTileOccupiedPerson = 13 //with blocking object
};
}}}}
