/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
#include <cstdio>
// FSO.SimAntics.Model.VMObjectQueries / VMSetToNextCache
#include "simantics/model/VMObjectQueries.h"
#include "simantics/model/VMSetToNextCache.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/entities/VMMultitileGroup.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"
#include "simantics/VMArchitecture.h"
#include <stdexcept>
#include <algorithm>
namespace FSO { namespace SimAntics { namespace Model {
using FSO::LotView::Model::LotTilePos;

static void ListRemove(std::vector<VMEntity*>& list, VMEntity* ent) {
    auto it = std::find(list.begin(), list.end(), ent);
    if (it != list.end()) list.erase(it);
}

// ---------------- VMObjectQueries

int32_t VMObjectQueries::GetOffest(const LotTilePos& pos) {
    if (pos == LotTilePos::OUT_OF_WORLD) return -1;
    return pos.TileX() + pos.TileY() * Context->Architecture()->Width + (pos.Level - 1) * Context->Architecture()->Width * Context->Architecture()->Height;
}

void VMObjectQueries::RegisterObjectPos(VMEntity* ent) {
    auto off = GetOffest(ent->Position());
    auto& tile = TileToObjects[off];
    VM::AddToObjList(tile, ent); //if it's already on this tile, this will do nothing
}

void VMObjectQueries::UnregisterObjectPos(VMEntity* ent) {
    auto off = GetOffest(ent->Position());
    auto it = TileToObjects.find(off);
    if (it == TileToObjects.end()) return; //???
    ListRemove(it->second, ent);
    if (it->second.size() == 0) TileToObjects.erase(it);
}

void VMObjectQueries::VerifyPositions() {
    for (auto& objs : TileToObjects) {
        auto off = objs.first;
        auto tileX = off % Context->Architecture()->Width;
        auto tileY = (off / Context->Architecture()->Width) % (Context->Architecture()->Height);
        auto level = (off / (Context->Architecture()->Width * Context->Architecture()->Height)) + 1;
        for (auto obj : objs.second) {
            if (off == -1) {
                if (obj->Position() != LotTilePos::OUT_OF_WORLD) throw std::runtime_error("Should be out of World!");
            }
            else if (obj->Position().TileX() != tileX || obj->Position().TileY() != tileY || obj->Position().Level != level)
                throw std::runtime_error("Invalid Position Assignment!!");
            if (obj->Dead) throw std::runtime_error("but it's dead!");
        }
    }
}

void VMObjectQueries::RegisterCategory(VMEntity* obj, int16_t category) {
    auto& tile = ObjectsByCategory[category];
    //debug check: use if things are going weird
    //if (!tile.Contains(obj))
    VM::AddToObjList(tile, obj);
}

void VMObjectQueries::RemoveCategory(VMEntity* obj, int16_t category) {
    auto it = ObjectsByCategory.find(category);
    if (it == ObjectsByCategory.end()) return; //???
    VM::DeleteFromObjList(it->second, obj);
    if (it->second.size() == 0) ObjectsByCategory.erase(it);
}

void VMObjectQueries::NewObject(VMEntity* obj) {
    auto guid = obj->Object->OBJ->GUID;
    auto& list = ObjectsByGUID[guid];
    VM::AddToObjList(list, obj);
    RegisterCategory(obj, obj->GetValue(VMStackObjectVariable::Category));
    auto ava = dynamic_cast<VMAvatar*>(obj);
    if (ava != nullptr) {
        VM::AddToObjList(Avatars, obj);
        if (obj->PersistID != 0) AvatarsByPersist[obj->PersistID] = ava;
    }
}

void VMObjectQueries::RemoveObject(VMEntity* obj) {
    auto guid = obj->Object->OBJ->GUID;
    auto it = ObjectsByGUID.find(guid);
    if (it != ObjectsByGUID.end()) {
        VM::DeleteFromObjList(it->second, obj);
        if (it->second.size() == 0) ObjectsByGUID.erase(it);
    }
    RemoveCategory(obj, obj->GetValue(VMStackObjectVariable::Category));
    if (dynamic_cast<VMAvatar*>(obj) != nullptr) {
        ListRemove(Avatars, obj);
        AvatarsByPersist.erase(obj->PersistID);
    } else if (obj->PersistID > 0 && obj->MultitileGroup->Objects.size() == 1) {
        MultitileByPersist.erase(obj->PersistID);
        if (obj->Thread != nullptr) {
            // if (vm.PlatformState.LimitExceeded) VMBuildableAreaInfo.UpdateOverbudgetObjects(vm);
        }
    }
}

std::vector<VMEntity*>* VMObjectQueries::GetObjectsAt(const LotTilePos& pos) {
    auto it = TileToObjects.find(GetOffest(pos));
    return (it == TileToObjects.end()) ? nullptr : &it->second;
}
std::vector<VMEntity*>* VMObjectQueries::GetObjectsByGUID(uint32_t guid) {
    auto it = ObjectsByGUID.find(guid);
    return (it == ObjectsByGUID.end()) ? nullptr : &it->second;
}
std::vector<VMEntity*>* VMObjectQueries::GetObjectsByCategory(int16_t category) {
    auto it = ObjectsByCategory.find(category);
    return (it == ObjectsByCategory.end()) ? nullptr : &it->second;
}

// ---------------- VMSetToNextCache

int32_t VMSetToNextCache::GetOffest(const LotTilePos& pos) {
    if (pos == LotTilePos::OUT_OF_WORLD) return -1;
    return pos.TileX() + pos.TileY() * Context->Architecture()->Width + (pos.Level - 1) * Context->Architecture()->Width * Context->Architecture()->Height;
}

void VMSetToNextCache::RegisterCategory(VMEntity* obj, int16_t category) {
    auto& tile = ObjectsByCategory[category];
    //debug check: use if things are going weird
    //if (!tile.Contains(obj))
    VM::AddToObjList(tile, obj);
}

void VMSetToNextCache::RemoveCategory(VMEntity* obj, int16_t category) {
    auto it = ObjectsByCategory.find(category);
    if (it == ObjectsByCategory.end()) return; //???
    VM::DeleteFromObjList(it->second, obj);
    if (it->second.size() == 0) ObjectsByCategory.erase(it);
}

void VMSetToNextCache::RegisterObjectPos(VMEntity* ent) {
    auto off = GetOffest(ent->Position());
    auto& tile = TileToObjects[off];
    if (std::find(tile.begin(), tile.end(), ent) == tile.end()) VM::AddToObjList(tile, ent); //shouldn't be a problem any more, but just in case check first.
    else { }
}

void VMSetToNextCache::UnregisterObjectPos(VMEntity* ent) {
    auto off = GetOffest(ent->Position());
    auto it = TileToObjects.find(off);
    if (it == TileToObjects.end()) return; //???
    ListRemove(it->second, ent);
    if (it->second.size() == 0) TileToObjects.erase(it);
}

void VMSetToNextCache::VerifyPositions() {
    for (auto& objs : TileToObjects) {
        auto off = objs.first;
        auto tileX = off % Context->Architecture()->Width;
        auto tileY = (off / Context->Architecture()->Width) % (Context->Architecture()->Height);
        auto level = (off / (Context->Architecture()->Width * Context->Architecture()->Height)) + 1;
        for (auto obj : objs.second) {
            if (off == -1) {
                if (obj->Position() != LotTilePos::OUT_OF_WORLD) throw std::runtime_error("Should be out of World!");
            }
            else if (obj->Position().TileX() != tileX || obj->Position().TileY() != tileY || obj->Position().Level != level)
                throw std::runtime_error("Invalid Position Assignment!!");
            if (obj->Dead) throw std::runtime_error("but it's dead!");
        }
    }
}

void VMSetToNextCache::NewObject(VMEntity* obj) {
    auto guid = obj->Object->OBJ->GUID;
    auto& list = ObjectsByGUID[guid];
    VM::AddToObjList(list, obj);
    if (dynamic_cast<VMAvatar*>(obj) != nullptr) VM::AddToObjList(Avatars, obj);
}

void VMSetToNextCache::RemoveObject(VMEntity* obj) {
    auto guid = obj->Object->OBJ->GUID;
    auto it = ObjectsByGUID.find(guid);
    if (it != ObjectsByGUID.end()) {
        ListRemove(it->second, obj);
        if (it->second.size() == 0) ObjectsByGUID.erase(it);
    }
    // FIX(sims) QA4: FreeSO's RemoveObject also takes the object out of its category list
    // (RemoveCategory(obj, obj.GetValue(VMStackObjectVariable.Category))). FreeSims did not, so every deleted object
    // with a category (meals: Set To Next 'object with category = SP0' in CounterGlobals 'food slot test') stayed in
    // the list as a dead pointer: the busboy's check tree then read 'so func' of a deleted object and reset
    // (36 resets in one Downtown 24 run with the generated downtownies), and the dead entity was dereferenced.
    {
        auto cat = obj->GetValue(VMStackObjectVariable::Category);
        auto cit = ObjectsByCategory.find(cat);
        if (cit != ObjectsByCategory.end()) {
            ListRemove(cit->second, obj);
            if (cit->second.size() == 0) ObjectsByCategory.erase(cit);
        }
    }
    if (dynamic_cast<VMAvatar*>(obj) != nullptr) ListRemove(Avatars, obj);
}

std::vector<VMEntity*>* VMSetToNextCache::GetObjectsAt(const LotTilePos& pos) {
    auto it = TileToObjects.find(GetOffest(pos));
    return (it == TileToObjects.end()) ? nullptr : &it->second;
}
std::vector<VMEntity*>* VMSetToNextCache::GetObjectsByGUID(uint32_t guid) {
    auto it = ObjectsByGUID.find(guid);
    return (it == ObjectsByGUID.end()) ? nullptr : &it->second;
}
std::vector<VMEntity*>* VMSetToNextCache::GetObjectsByCategory(int16_t category) {
    auto it = ObjectsByCategory.find(category);
    return (it == ObjectsByCategory.end()) ? nullptr : &it->second;
}
}}}
