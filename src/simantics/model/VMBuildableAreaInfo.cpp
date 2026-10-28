/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Model.VMBuildableAreaInfo
#include "simantics/model/VMBuildableAreaInfo.h"
#include "simantics/model/VMObjectQueries.h"
#include "simantics/model/TSOPlatform/VMTSOLotState.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMGameObject.h"
#include "simantics/entities/VMMultitileGroup.h"
#include "simantics/engine/VMThread.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"
#include <algorithm>
#include <unordered_set>
namespace FSO { namespace SimAntics { namespace Model {

const int32_t VMBuildableAreaInfo::BuildableSizes[11] = { 12, 20, 26, 31, 35,
    40, //original tso 39
    45, //43
    50, //46
    55, //new
    60, 64 };
const int32_t VMBuildableAreaInfo::UpgradePrices[14] = { 0, 200, 550, 1200, 2200, 4000, 7000, 13000,
    //new: 3 size upgrades + 3 floor upgrades
    20000, 28000, 37000, 47000, 58000, 70000 };
const int32_t VMBuildableAreaInfo::ObjectLimitPerPerson[14] = { 75, 92, 109, 126, 143, 160, 177, 194, 211, 228, 245, 262, 279, 300 };
const int32_t VMBuildableAreaInfo::LackRoomies[8] = { 0, 200, 650, 1400, 2900, 5100, 9100, 16100 };

int32_t VMBuildableAreaInfo::CalculateRoomieCost(int32_t roomies, int32_t initialLevel, int32_t targetLevel) {
    roomies = std::min<int32_t>(8, std::max<int32_t>(roomies, 1)) - 1; //can't have 0 roomies, or more than 8. Start at 1.
    auto lackAtCurrent = LackRoomies[std::max<int32_t>(0, std::min<int32_t>(7, initialLevel) - roomies)];
    auto lackAtTarget = LackRoomies[std::max<int32_t>(0, std::min<int32_t>(7, targetLevel) - roomies)];
    return lackAtTarget - lackAtCurrent;
}

int32_t VMBuildableAreaInfo::GetObjectLimit(VM* vm) {
    if (vm->TSOState()->PropertyCategory == 255) return 10000;
    // else if (vm.TSOState.CommunityLot) return 100;
    auto lotInfo = vm->TSOState();
    auto lotSize = lotInfo->Size & 255;
    auto lotFloors = (lotInfo->Size >> 8) & 255;
    auto sizeMode = ObjectLimitPerPerson[std::min<int32_t>(13, lotSize + lotFloors)];
    return sizeMode * std::max<int32_t>(1, (int32_t)lotInfo->Roommates.size());
}

int32_t VMBuildableAreaInfo::GetDiscountFor(FSO::Content::WorldObjectCatalog::ObjectCatalogItem* item, VM* vm) {
    if (vm == nullptr || vm->TS1 || vm->TSOState()->PropertyCategory != 5) return 0;
    auto guid = item->GUID;
    auto catalog = FSO::Content::Content::Get()->WorldCatalog.get();
    int32_t sameItem = 0, catItems = 0;
    for (auto x : vm->Entities) {
        if (x != x->MultitileGroup->BaseObject()) continue;
        if (x->Object->OBJ->GUID == guid || (x->MasterDefinition != nullptr && x->MasterDefinition->GUID == guid)) sameItem++;
        auto catItem = catalog->GetItemByGUID((x->MasterDefinition == nullptr) ? x->Object->OBJ->GUID : x->MasterDefinition->GUID);
        if (catItem != nullptr && item->Category == catItem->Category) catItems++;
    }
    sameItem = std::min<int32_t>(3, sameItem);
    catItems = std::min<int32_t>(10, catItems);
    return sameItem * 10 + catItems * 3;
}

void VMBuildableAreaInfo::UpdateOverbudgetObjects(VM* vm) {
    auto limit = GetObjectLimit(vm);
    vm->TSOState()->ObjectLimit = limit;
    //community lots cannot go overbudget, as the object limit does not change!
    if (vm->TSOState()->LotID != 0) {
        std::unordered_set<Entities::VMMultitileGroup*> multInUse;
        for (auto ava : vm->Context->ObjectQueries->Avatars) {
            if (ava->Thread == nullptr) continue;
            for (auto& frame : ava->Thread->Stack) {
                if (frame->Callee != nullptr && dynamic_cast<VMGameObject*>(frame->Callee) != nullptr) multInUse.insert(frame->Callee->MultitileGroup);
            }
        }
        int i = 0;
        std::vector<Entities::VMMultitileGroup*> ordered;
        for (auto& kv : vm->Context->ObjectQueries->MultitileByPersist) ordered.push_back(kv.second);
        std::sort(ordered.begin(), ordered.end(), [](Entities::VMMultitileGroup* a, Entities::VMMultitileGroup* b) { return a->BaseObject()->ObjectID < b->BaseObject()->ObjectID; });
        for (auto obj : ordered) {
            auto baseGO = (obj->Objects.size() > 0) ? dynamic_cast<VMGameObject*>(obj->BaseObject()) : nullptr;
            auto isPortal = (baseGO != nullptr) && baseGO->PartOfPortal();
            for (auto o : obj->Objects) {
                auto go = dynamic_cast<VMGameObject*>(o);
                if (go != nullptr) {
                    if (i >= limit) {
                        go->Disabled |= VMGameObjectDisableFlags::ObjectLimitExceeded;
                        go->Disabled &= ~VMGameObjectDisableFlags::ObjectLimitThreadDisable;
                        if (multInUse.find(obj) == multInUse.end() && !o->GetFlag(VMEntityFlags::Occupied) && !isPortal) go->Disabled |= VMGameObjectDisableFlags::ObjectLimitThreadDisable;
                    }
                    else go->Disabled &= ~(VMGameObjectDisableFlags::ObjectLimitExceeded | VMGameObjectDisableFlags::ObjectLimitThreadDisable);
                }
            }
            i++;
        }
    }
    vm->TSOState()->LimitExceeded(vm->Context->ObjectQueries->NumUserObjects() > limit);
}
}}}
