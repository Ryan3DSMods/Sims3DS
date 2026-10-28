/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.LotView.WorldZoom (World/model/WorldZoom.cs)
#pragma once
namespace FSO { namespace LotView {
    /// Zoom level for worlds.
    enum class WorldZoom : int {
        Near = 3,
        Medium = 2,
        Far = 1
    };
}}
