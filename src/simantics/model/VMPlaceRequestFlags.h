/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Model.VMPlaceRequestFlags
#pragma once
namespace FSO { namespace SimAntics { namespace Model {
enum class VMPlaceRequestFlags : int {
    Default = 0, // no intersection, does not place in slots, ignores user placement rules
    AcceptSlots = 1, //places in non-hand slots, tries all available on specified tile.
    UserBuildableLimit = 2, //respect the buildable area
    AllowIntersection = 4, //TODO: used by some primitives
    AllAvatarsSolid = 8,
    UserPlacement = AcceptSlots | UserBuildableLimit
};
inline VMPlaceRequestFlags operator|(VMPlaceRequestFlags a, VMPlaceRequestFlags b) { return (VMPlaceRequestFlags)((int)a | (int)b); }
inline VMPlaceRequestFlags operator&(VMPlaceRequestFlags a, VMPlaceRequestFlags b) { return (VMPlaceRequestFlags)((int)a & (int)b); }
inline bool operator>(VMPlaceRequestFlags a, int b) { return (int)a > b; }
}}}
