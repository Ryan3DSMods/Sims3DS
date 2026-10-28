/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.LotView.WorldRotation (World/model/WorldRotation.cs)
#pragma once
namespace FSO { namespace LotView {
    /// The rotation names refer to the position of the tile 0,0 when projected
    /// onto the screen.
    enum class WorldRotation : int { TopLeft = 0, TopRight = 1, BottomRight = 2, BottomLeft = 3 };
}}
