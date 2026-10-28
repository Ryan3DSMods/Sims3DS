/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Engine.Scopes.VMSuitScope / VMPersonSuits
#pragma once
namespace FSO { namespace SimAntics { namespace Engine { namespace Scopes {
enum class VMSuitScope : int { Global = 0, Person = 1, Object = 2 };
enum class VMPersonSuits : int {
    DefaultDaywear = 0, Naked = 1, DefaultSwimwear = 2, JobOutfit = 3, DefaultSleepwear = 5, SkeletonPlus = 6,
    SkeletonMinus = 7, DecorationHead = 8, DecorationBack = 9, DecorationShoes = 10, DecorationTail = 11,
    TeleporterMishap = 20, CockroachHead = 21, DynamicDaywear = 22, DynamicSwimwear = 23, DynamicSleepwear = 24,
    DynamicCostume = 25, signnotepad = 26
};
}}}}
