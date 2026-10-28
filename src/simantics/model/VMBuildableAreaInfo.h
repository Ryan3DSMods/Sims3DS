/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Model.VMBuildableAreaInfo — manages how buildable area costs and scaling works.
#pragma once
#include <cstdint>
#include <vector>
#include "content/ContentManager.h"
namespace FSO { namespace SimAntics {
class VM;
namespace Model {
class VMBuildableAreaInfo {
public:
    /// Possible sizes in tiles of the buildable area.
    static const int32_t BuildableSizes[11];
    /// The price to increase size by 1 or add a floor. Having 64x64 5 floors is meant to be expensive!
    static const int32_t UpgradePrices[14];
    static const int32_t ObjectLimitPerPerson[14];
    /// Note: levels above 8 do not require more than 8 roomies to have no penalty, as you can only have 8 roomies
    static const int32_t LackRoomies[8];

    static int32_t CalculateBaseCost(int32_t initialLevel, int32_t targetLevel) { return UpgradePrices[targetLevel] - UpgradePrices[initialLevel]; }
    static int32_t CalculateRoomieCost(int32_t roomies, int32_t initialLevel, int32_t targetLevel);
    static int32_t GetObjectLimit(VM* vm);
    static int32_t GetDiscountFor(FSO::Content::WorldObjectCatalog::ObjectCatalogItem* item, VM* vm);
    static void UpdateOverbudgetObjects(VM* vm);
};
}}}
