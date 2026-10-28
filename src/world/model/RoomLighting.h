/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.LotView.Model.RoomLighting
#pragma once
#include <cstdint>
namespace FSO { namespace LotView { namespace Model {
    class RoomLighting {
    public:
        //TODO: point lights
        uint16_t OutsideLight = 0;
        uint16_t AmbientLight = 0;
        int16_t RoomScore = 0;
    };
}}}
