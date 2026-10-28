/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.VMContext
#pragma once
#include <cstdint>
#include <array>
#include <vector>
#include <memory>
#include "core/math.h"
#include "content/ContentManager.h"
#include "world/World.h"
#include "world/model/Blueprint.h"
#include "world/model/LotTilePos.h"
#include "world/model/Direction.h"
#include "simantics/VMPlacementResult.h"
#include "simantics/VMClock.h"
#include "simantics/VMArchitecture.h"
#include "simantics/model/VMRoomInfo.h"
#include "simantics/model/VMObjectQueries.h"
#include "simantics/model/VMSetToNextCache.h"
#include "simantics/engine/VMPrimitiveRegistration.h"

namespace FSO { namespace SimAntics {
class VM;
class VMEntity;
namespace Engine { class VMAmbientSound; }
namespace Entities { class VMMultitileGroup; }

class VMContext {
public:
    static bool UseWorld;
    FSO::LotView::Model::Blueprint* Blueprint = nullptr;         // owned by OwnedBlueprint when the activator created it
    std::unique_ptr<FSO::LotView::Model::Blueprint> OwnedBlueprint;
    std::unique_ptr<Model::VMObjectQueries> ObjectQueries;
    std::unique_ptr<FSO::SimAntics::VMClock> Clock;

    FSO::SimAntics::VMArchitecture* Architecture() const { return _Arch.get(); }
    /// Takes ownership; hooks WallsChanged like the C# property setter.
    void Architecture(std::unique_ptr<FSO::SimAntics::VMArchitecture> value);
    bool Ready() const { return (_Arch != nullptr); }

    FSO::LotView::World* World = nullptr;
    /// Indexed by opcode (0-255). Register with AddPrimitive.
    std::array<std::unique_ptr<Engine::VMPrimitiveRegistration>, 256> Primitives;
    std::shared_ptr<Engine::VMAmbientSound> Ambience;
    uint64_t RandomSeed = 0;
    static uint64_t DebugSeed; // FIX(sims) QA: when non-zero, new VMs get repeatable seeds (host tools only)

    FSO::Content::GameGlobal* Globals = nullptr;
    std::unique_ptr<Model::VMSetToNextCache> SetToNextCache;
    std::vector<Model::VMRoomInfo> RoomInfo;

    FSO::SimAntics::VM* VM = nullptr;

    explicit VMContext(FSO::LotView::World* world);
    VMContext(FSO::LotView::World* world, VMContext* oldContext);
    ~VMContext();

    /// Returns a random number between 0 and less than the specified maximum.
    uint64_t NextRandom(uint64_t max);

    FSO::LotView::Components::ObjectComponent* MakeObjectComponent(FSO::Content::GameObject* obj);
    bool IsUserOutOfBounds(const FSO::LotView::Model::LotTilePos& pos);
    void UpdateTSOBuildableArea();
    st::Rectangle GetTSOBuildableArea(int32_t lotSInfo);
    void RegeneratePortalInfo();
    void RefreshLighting(uint16_t room, bool commit);
    void RefreshRoomScore(uint16_t room); // TS1(sims): FreeSO
    void AddRoomPortal(VMEntity* obj, uint16_t room);
    void RemoveRoomPortal(VMEntity* obj, uint16_t room);
    void RegisterObjectPos(VMEntity* obj);
    void UnregisterObjectPos(VMEntity* obj);
    bool SlopeVertexCheck(int32_t x, int32_t y); // TS1(sims): FreeSO
    bool CheckSlopeValid(const FSO::LotView::Model::LotTilePos& pos);
    bool CheckWallValid(const FSO::LotView::Model::LotTilePos& pos, const FSO::LotView::Model::WallTile& wall);
    bool CheckFloorValid(const FSO::LotView::Model::LotTilePos& pos, const FSO::LotView::Model::FloorTile& floor);
    VMSolidResult SolidToAvatars(const FSO::LotView::Model::LotTilePos& pos);
    bool IsOutOfBounds(const FSO::LotView::Model::LotTilePos& pos);
    VMPlacementResult GetAvatarPlace(VMEntity* target, const FSO::LotView::Model::LotTilePos& pos, FSO::LotView::Model::Direction dir);
    VMPlacementResult GetObjPlace(VMEntity* target, const FSO::LotView::Model::LotTilePos& pos, FSO::LotView::Model::Direction dir);
    uint16_t GetObjectRoom(VMEntity* obj);
    uint16_t GetRoomAt(const FSO::LotView::Model::LotTilePos& pos);
    int16_t GetRoomScore(uint16_t room);
    Entities::VMMultitileGroup* GhostCopyGroup(Entities::VMMultitileGroup* group);
    Entities::VMMultitileGroup* CreateObjectInstance(uint32_t GUID, const FSO::LotView::Model::LotTilePos& pos, FSO::LotView::Model::Direction direction, bool ghostImage, bool ts1);
    Entities::VMMultitileGroup* CreateObjectInstance(uint32_t GUID, const FSO::LotView::Model::LotTilePos& pos, FSO::LotView::Model::Direction direction, bool ts1);
    Entities::VMMultitileGroup* CreateObjectInstance(uint32_t GUID, const FSO::LotView::Model::LotTilePos& pos, FSO::LotView::Model::Direction direction, int16_t MainStackOBJ, int16_t MainParam, bool ghostImage, bool ts1);
    void RemoveObjectInstance(VMEntity* target);

    void AddPrimitive(std::unique_ptr<Engine::VMPrimitiveRegistration> primitive);
    void AddPrimitive(Engine::VMPrimitiveRegistration* primitive) { AddPrimitive(std::unique_ptr<Engine::VMPrimitiveRegistration>(primitive)); }

private:
    std::unique_ptr<FSO::SimAntics::VMArchitecture> _Arch;
    void WallsChanged(FSO::SimAntics::VMArchitecture* caller);
};

/// Registers the SimAntics primitive table (opcodes 0-67, see README.md) into a context.
/// Defined (strongly) by src/simantics/primitives/VMPrimitiveTable.cpp. A weak empty default lives in
/// VMContext.cpp so the core links on its own.
void RegisterDefaultPrimitives(VMContext* context);
void RegisterTS1ExpansionPrimitives(VMContext* context); // TS1(sims): primitives/VMTS1Primitives.cpp
}}
