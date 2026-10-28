/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.LotView.Model.Direction
#pragma once
namespace FSO { namespace LotView { namespace Model {
    /// Current direction of world/camera/renderer
    enum class Direction : int {
        SOUTH = 0x10,
        WEST = 0x40,
        EAST = 0x04,
        NORTH = 0x01
    };
}}}
