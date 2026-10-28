/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.LotView.Model.Room
#pragma once
#include <cstdint>
#include "core/math.h"
namespace FSO { namespace LotView { namespace Model {
    struct Room {
        uint16_t RoomID = 0;
        bool IsOutside = false;
        uint16_t Area = 0;
        bool IsPool = false;
        st::Rectangle Bounds;
    };
}}}
