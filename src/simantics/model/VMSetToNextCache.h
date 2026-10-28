/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Model.VMSetToNextCache
#pragma once
#include <cstdint>
#include <vector>
#include <unordered_map>
#include "world/model/LotTilePos.h"
namespace FSO { namespace SimAntics {
class VMEntity; class VMContext;
namespace Model {
class VMSetToNextCache {
public:
    std::vector<VMEntity*> Avatars;
    explicit VMSetToNextCache(VMContext* context) : Context(context) {}
    void RegisterCategory(VMEntity* obj, int16_t category);
    void RemoveCategory(VMEntity* obj, int16_t category);
    void RegisterObjectPos(VMEntity* ent);
    void UnregisterObjectPos(VMEntity* ent);
    /// Debug function. Call to make sure positions are correctly registered.
    void VerifyPositions();
    void NewObject(VMEntity* obj);
    void RemoveObject(VMEntity* obj);
    std::vector<VMEntity*>* GetObjectsAt(const FSO::LotView::Model::LotTilePos& pos);
    std::vector<VMEntity*>* GetObjectsByGUID(uint32_t guid);
    std::vector<VMEntity*>* GetObjectsByCategory(int16_t category);
private:
    VMContext* Context;
    std::unordered_map<int32_t, std::vector<VMEntity*>> TileToObjects;
    std::unordered_map<int16_t, std::vector<VMEntity*>> ObjectsByCategory;
    std::unordered_map<uint32_t, std::vector<VMEntity*>> ObjectsByGUID;
    int32_t GetOffest(const FSO::LotView::Model::LotTilePos& pos);
};
}}}
