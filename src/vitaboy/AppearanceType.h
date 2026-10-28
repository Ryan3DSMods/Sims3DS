/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.Vitaboy.AppearanceType (sims.common/rendering/vitaboy/AppearanceType.cs)
#pragma once

namespace FSO { namespace Vitaboy {

/// <summary>
/// A sim can be of light, medium or dark skin.
/// </summary>
enum class AppearanceType : int {
    Light = 0,
    Medium = 1,
    Dark = 2
};

}}
