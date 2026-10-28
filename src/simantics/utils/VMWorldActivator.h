/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// VMWorldActivator (utils/VMWorldActivator.cs, global namespace in C#) — loads a lot into the VM:
// from a Sims 1 house IFF (SIMI/HOUS/ARRY/OBJM/OBJT/WALm/FLRm) or from FreeSims' XML house model.
#pragma once
#include <cstdint>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include "simantics/_files_contract.h"
#include "world/World.h"
#include "world/model/Blueprint.h"
#include "world/model/XmlHouse.h"
namespace FSO { namespace SimAntics { class VM; class VMAvatar; class XmlCharacter; } }

class VMWorldActivator {
public:
    using Blueprint = FSO::LotView::Model::Blueprint;
    using FloorTile = FSO::LotView::Model::FloorTile;
    using WallTile = FSO::LotView::Model::WallTile;

    static const std::unordered_set<uint32_t> ControllerObjects;
    /// TS1(sims): value for global 33 "Machine Level" (0 low .. 2 high: how much traffic, how many walk-bys
    /// and NPCs the controllers spawn). FreeSO uses 2; a front-end may lower it for the 3DS.
    static int16_t MachineLevel;
    static bool IsLateController(uint32_t guid); // FIX(sims)
    static bool IsCommunityOnlyController(uint32_t guid); // TS1(sims)
    static const std::unordered_set<uint16_t> ValidWallStyles;

    VMWorldActivator(FSO::SimAntics::VM* vm, FSO::LotView::World* world);

    /// Both loaders create the Blueprint/VMArchitecture on vm->Context (the context owns them) and return the blueprint.
    Blueprint* LoadFromXML(FSO::LotView::Model::XmlHouseData& model);
    Blueprint* LoadFromIff(FSO::Files::Formats::IFF::IffFile* iff);

    std::vector<FloorTile> DecodeFloors(const std::vector<uint8_t>& data);
    std::vector<FloorTile> DecodeAdvFloors(const std::vector<uint8_t>& data);
    /// data == nullptr -> returns false (C# null)
    bool DecodeObjID(const std::vector<uint8_t>* data, std::vector<uint16_t>& result);
    std::vector<WallTile> DecodeWalls(const std::vector<uint8_t>& data);
    std::vector<WallTile> DecodeAdvWalls(const std::vector<uint8_t>& data);

    FSO::SimAntics::VMAvatar* CreateAvatar();
    FSO::SimAntics::VMAvatar* CreateAvatar(uint32_t guid, FSO::SimAntics::XmlCharacter* xml, bool visitor, int16_t id);
    void CreateObject(const FSO::LotView::Model::XmlHouseDataObject& obj);

    /// TS1(sims): statistics of the last RestoreSavedState (for tools/logs).
    struct RestoreStats { int Objects = 0, Persons = 0, PersonsPlaced = 0, Contained = 0, Missing = 0, Skipped = 0; } LastRestore;

private:
    FSO::SimAntics::VM* VM;
    FSO::LotView::World* World;
    Blueprint* BlueprintPtr = nullptr;
    bool FlipRoad = false;

    std::vector<bool> ResizeFlags(const std::vector<uint8_t>& flags, int32_t size);
    std::vector<FloorTile> ResizeFloors(const std::vector<FloorTile>& floors, int32_t size);
    std::vector<WallTile> ResizeWalls(const std::vector<WallTile>& walls, int32_t size);
    std::vector<FloorTile> RemapFloors(std::vector<FloorTile> floors, const std::unordered_map<uint8_t, uint16_t>& dict, const std::vector<uint8_t>& flags);
    std::vector<WallTile> RemapWalls(std::vector<WallTile> walls, const std::unordered_map<uint8_t, uint16_t>& dict, const std::unordered_map<uint8_t, uint16_t>& floorDict);
    std::unordered_map<uint8_t, uint16_t> BuildFloorDict(const std::vector<FSO::Files::Formats::IFF::Chunks::WALmEntry>& entries);
    std::unordered_map<uint8_t, uint16_t> BuildWallDict(const std::vector<FSO::Files::Formats::IFF::Chunks::WALmEntry>& entries);
    std::unique_ptr<FSO::LotView::Components::TerrainComponent> CreateTerrain(int16_t Size);
    void InitWorldComponent(FSO::LotView::Components::WorldComponent* component);
    /// TS1(sims): restore the saved per-object state decoded from OBJM (FreeSO OBJMInstance): attributes, object
    /// data, dynamic sprite flags, relationships, containment, and for persons person data, motives and position.
    void RestoreSavedState(FSO::Files::Formats::IFF::Chunks::OBJM* objm);
};
