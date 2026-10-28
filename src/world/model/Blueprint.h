/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.LotView.Model.Blueprint (World/model/Blueprint.cs)
// Holds all the objects that exist in the world for rendering and SimAntics.
// The tile arrays are not copied: they point at VMArchitecture's vectors (VMArchitecture::SyncBlueprint).
#pragma once
#include <cstdint>
#include <vector>
#include <memory>
#include "core/math.h"
#include "world/model/WallTile.h"
#include "world/model/FloorTile.h"
#include "world/model/Room.h"
#include "world/model/RoomLighting.h"
#include "world/model/LotTilePos.h"
#include "world/components/WorldComponents.h"

namespace FSO { namespace LotView { namespace Model {

enum class BlueprintDamageType {
    OBJECT_MOVE, OBJECT_GRAPHIC_CHANGE, OBJECT_RETURN_TO_STATIC, FLOOR_CHANGED, WALL_CHANGED,
    SCROLL, ROTATE, ZOOM, PRECISE_ZOOM, WALL_CUT_CHANGED, LEVEL_CHANGED, LIGHTING_CHANGED,
    ROOM_CHANGED, ROOF_STYLE_CHANGED
};

class BlueprintDamage {
public:
    BlueprintDamageType Type;
    int16_t TileX = 0;
    int16_t TileY = 0;
    int8_t Level = 0;
    FSO::LotView::WorldComponent* Component = nullptr;
    BlueprintDamage(BlueprintDamageType type) : Type(type) {}
    BlueprintDamage(BlueprintDamageType type, int16_t tileX, int16_t tileY, int8_t level, FSO::LotView::WorldComponent* component)
        : Type(type), TileX(tileX), TileY(tileY), Level(level), Component(component) {}
    BlueprintDamage(BlueprintDamageType type, int16_t tileX, int16_t tileY, int8_t level) : Type(type), TileX(tileX), TileY(tileY), Level(level) {}
};

class Blueprint {
public:
    std::vector<BlueprintDamage> Damage;
    int32_t Width;
    int32_t Height;
    int8_t Stories = 5;

    /// Only read these arrays, do not modify them!  (shared with VMArchitecture, which owns them)
    std::vector<std::vector<WallTile>>* Walls = nullptr;
    std::vector<std::vector<int32_t>>* WallsAt = nullptr;
    std::unique_ptr<Components::WallComponent> WallComp;

    std::vector<std::vector<FloorTile>>* Floors = nullptr;
    std::unique_ptr<Components::FloorComponent> FloorComp;

    Components::RoofComponent RoofComp;

    std::vector<std::vector<bool>>* Supported = nullptr; //directly the VM's copy at all times. DO NOT MODIFY.

    std::vector<Components::ObjectComponent*> Objects;
    std::vector<Components::AvatarComponent*> Avatars;
    std::unique_ptr<Components::TerrainComponent> Terrain;

    /// Walls Cutaway sections. Remember to manage these correctly - i.e remove when you're finished with them!
    std::vector<bool> Cutaway;

    st::Color OutsideColor = st::Color::White();
    std::vector<RoomLighting> Light;
    std::vector<const std::vector<uint32_t>*> RoomMap;
    std::vector<Room> Rooms;

    std::vector<st::Color> RoomColors;
    st::Rectangle BuildableArea;
    st::Rectangle TargetBuildableArea;

    // TS1(sims): FreeSO terrain altitude (VMArchitectureTerrain.Heights / Centers, shared with the VM like the
    // tile arrays). Per vertex, FreeSO units (TS1 altitude x 10); TerrainFactor turns them into tile heights.
    const std::vector<int16_t>* Altitude = nullptr;
    const std::vector<int16_t>* AltitudeCenters = nullptr;
    const std::vector<bool>* FineArea = nullptr;     // user buildable tiles (VMArchitecture.FineBuildableArea)
    float TerrainFactor = 3 / 160.0f;
    int32_t BaseAlt = 0;
    /// Altitude of a tile's centre (FreeSO GetAltitude), in tile heights (one story = 2.95).
    float GetAltitude(int x, int y) const;
    float GetAltPoint(int x, int y) const;
    /// Bilinear altitude at a tile-space position (FreeSO InterpAltitude).
    float InterpAltitude(const st::Vector3& position) const;

    Blueprint(int32_t width, int32_t height);
    ~Blueprint();

    void GenerateRoomLights();

    void AddAvatar(Components::AvatarComponent* avatar);
    void RemoveAvatar(Components::AvatarComponent* avatar) {
        for (size_t i = 0; i < Avatars.size(); i++) if (Avatars[i] == avatar) { Avatars.erase(Avatars.begin() + i); return; }
    }
    void SignalWallChange() {
        Damage.push_back(BlueprintDamage(BlueprintDamageType::WALL_CHANGED, 0, 0, 1));
        //todo: should this even have a position? we're rerendering the whole thing atm
    }
    void SignalRoomChange() { Damage.push_back(BlueprintDamage(BlueprintDamageType::ROOM_CHANGED, 0, 0, 1)); }
    void SignalFloorChange() { Damage.push_back(BlueprintDamage(BlueprintDamageType::FLOOR_CHANGED, 0, 0, 1)); }

    WallTile GetWall(int16_t tileX, int16_t tileY, int8_t level) const {
        return (*Walls)[(size_t)(level - 1)][GetOffset(tileX, tileY)];
    }
    FloorTile GetFloor(int16_t tileX, int16_t tileY, int8_t level) const {
        return (*Floors)[(size_t)(level - 1)][GetOffset(tileX, tileY)];
    }

    void ChangeObjectLocation(Components::ObjectComponent* component, const LotTilePos& pos);
    void AddObject(Components::ObjectComponent* component);
    void RemoveObject(Components::ObjectComponent* component);

private:
    uint16_t GetOffset(int32_t tileX, int32_t tileY) const { return (uint16_t)((tileY * Width) + tileX); }
};

}}}
