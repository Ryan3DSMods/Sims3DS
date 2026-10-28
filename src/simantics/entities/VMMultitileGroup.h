/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Entities.VMMultitileGroup — ties multiple entities together with a common name and set of repositioning functions.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "core/math.h"
#include "world/model/LotTilePos.h"
#include "world/model/Direction.h"
#include "simantics/VMPlacementResult.h"
namespace FSO { namespace SimAntics {
class VMEntity; class VMContext;
namespace Entities {
class VMMultitileGroup {
public:
    bool MultiTile = false;
    std::string Name = "";
    int32_t Price = 0;
    int32_t SalePrice = -1;
    std::vector<VMEntity*> Objects;
    std::vector<FSO::LotView::Model::LotTilePos> Offsets;

    VMEntity* BaseObject();

    VMMultitileGroup() {}
    void AddObject(VMEntity* obj);
    void AddDynamicObject(VMEntity* obj, const FSO::LotView::Model::LotTilePos& offset);
    void RemoveObject(VMEntity* obj);
    std::vector<st::Vector3> GetBasePositions();
    VMEntity* GetInteractionGroupLeader(VMEntity* obj);
    VMPlacementResult ChangePosition(const FSO::LotView::Model::LotTilePos& pos, FSO::LotView::Model::Direction direction, VMContext* context);
    void SetVisualPosition(const st::Vector3& pos, FSO::LotView::Model::Direction direction, VMContext* context);
    void Combine(VMMultitileGroup* other);
    void ExecuteEntryPoint(int32_t num, VMContext* context);
    void Delete(VMContext* context);
    void Init(VMContext* context);
};
}}}
