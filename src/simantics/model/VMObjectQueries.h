/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Model.VMObjectQueries
#pragma once
#include <cstdint>
#include <vector>
#include <unordered_map>
#include "world/model/LotTilePos.h"
namespace FSO { namespace SimAntics {
class VM; class VMEntity; class VMAvatar; class VMContext;
namespace Entities { class VMMultitileGroup; }
namespace Model {
class VMObjectQueries {
public:
    std::vector<VMEntity*> Avatars;
    std::unordered_map<uint32_t, VMAvatar*> AvatarsByPersist;
    std::unordered_map<uint32_t, Entities::VMMultitileGroup*> MultitileByPersist;

    int32_t NumUserObjects() const {
        //if we're not a community lot, we can short ciruit this.
        return (int32_t)MultitileByPersist.size();
    }
    int32_t NumDonatedObjects() const { return 0; }

    explicit VMObjectQueries(VMContext* context) : Context(context) {}

    void RegisterObjectPos(VMEntity* ent);
    void UnregisterObjectPos(VMEntity* ent);
    /// Debug function. Call to make sure positions are correctly registered.
    void VerifyPositions();
    void RegisterAvatarPersist(VMAvatar* ava, uint32_t persistID) { if (persistID != 0) AvatarsByPersist[persistID] = ava; }
    void RemoveAvatarPersist(uint32_t persistID) { AvatarsByPersist.erase(persistID); }
    void RegisterMultitilePersist(Entities::VMMultitileGroup* mul, uint32_t persistID) { if (persistID != 0) MultitileByPersist[persistID] = mul; }
    void RemoveMultitilePersist(VM* vm, uint32_t persistID) { (void)vm; MultitileByPersist.erase(persistID); }
    void RegisterCategory(VMEntity* obj, int16_t category);
    void RemoveCategory(VMEntity* obj, int16_t category);
    void NewObject(VMEntity* obj);
    void RemoveObject(VMEntity* obj);
    std::vector<VMEntity*>* GetObjectsAt(const FSO::LotView::Model::LotTilePos& pos);
    std::vector<VMEntity*>* GetObjectsByGUID(uint32_t guid);
    std::vector<VMEntity*>* GetObjectsByCategory(int16_t category);
private:
    VMContext* Context;
    std::unordered_map<int32_t, std::vector<VMEntity*>> TileToObjects;
    std::unordered_map<uint32_t, std::vector<VMEntity*>> ObjectsByGUID;
    std::unordered_map<int16_t, std::vector<VMEntity*>> ObjectsByCategory;
    int32_t GetOffest(const FSO::LotView::Model::LotTilePos& pos);
};
}}}
