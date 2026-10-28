/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.VMArchitecture — the lot architecture model: walls, floors, rooms, support.
#pragma once
#include <cstdint>
#include <vector>
#include <functional>
#include "core/math.h"
#include "world/model/WallTile.h"
#include "world/model/FloorTile.h"
#include "world/model/Blueprint.h"
#include "simantics/model/VMRoomMap.h"
#include "simantics/model/VMRoomInfo.h"
#include "simantics/model/VMArchitectureCommand.h"
#include "simantics/model/VMArchitectureTerrain.h"
#include <memory>
#include "content/ContentManager.h"
namespace FSO { namespace SimAntics {
class VMContext;
class VMArchitecture {
public:
    int32_t Width;
    int32_t Height;
    int32_t Stories = 5;

    int32_t LastTestCost = 0;

    //public for quick access and iteration.
    //Make sure that on modifications you signal so that the render updates.
    std::vector<std::vector<FSO::LotView::Model::WallTile>> Walls;
    std::vector<std::vector<FSO::LotView::Model::WallTile>> VisWalls;
    std::vector<std::vector<int32_t>> WallsAt;
    // ADDED(sims): C# allocated a fresh WallsAt list for the simulated (preview) walls and handed it to the
    // blueprint with VisWalls/VisFloors; kept here so build-mode previews draw (see SimulateCommands).
    std::vector<std::vector<int32_t>> VisWallsAt;

    std::vector<std::vector<FSO::LotView::Model::FloorTile>> Floors;
    std::vector<std::vector<FSO::LotView::Model::FloorTile>> VisFloors;

    std::vector<std::vector<bool>> ObjectSupport;
    std::vector<std::vector<bool>> Supported;

    std::vector<Model::VMArchitectureCommand> Commands;

    st::Rectangle BuildableArea;
    std::vector<bool> FineBuildableArea; // empty == C# null
    int32_t BuildableFloors = 0;

    std::vector<Model::VMRoomMap> Rooms;
    std::vector<Model::VMRoom> RoomData;

    using ArchitectureEvent = std::function<void(VMArchitecture*)>;
    ArchitectureEvent WallsChanged; // C# event (single subscriber: VMContext)

    uint32_t RoofStyle = 16;
    float RoofPitch = 0.66f;
    bool RoofUserSet = false; // TS1(sims): the roof tool changed style/pitch (the saver rewrites HOUS)

    // TS1(sims): FreeSO terrain (VMArchitectureTerrain) and user build limits.
    std::unique_ptr<Model::VMArchitectureTerrain> Terrain;
    st::Rectangle TerrainLimit; //in tso mode, this is set to the maximum buildable area.
    bool DisableClip = false;
    bool MapEdit = false; // TS1(sims): the map_edit cheat: build/buy outside the buildable area (whole lot, street too)
    int32_t LastFailReason = 0;
    bool TerrainDirty = false;
    bool TerrainPreviewShown = false; // ADDED(sims): the mesh shows VisHeights
    bool GetTerrainSloped(int16_t tileX, int16_t tileY);
    void SetTerrainHeight(int16_t tileX, int16_t tileY, int16_t height);
    int16_t GetTerrainHeight(int16_t tileX, int16_t tileY);
    void SetTerrainGrass(int16_t tileX, int16_t tileY, uint8_t grass);
    uint8_t GetTerrainGrass(int16_t tileX, int16_t tileY);
    void SignalTerrainRedraw() { TerrainDirty = true; Redraw = true; }
    /// FreeSO: true when a user build at this tile is outside the lot's buildable area (TS1: ARRY 8 flag 32).
    bool OutsideClip(int16_t tileX, int16_t tileY, int8_t level);
    /// TS1(sims): hands the (real) heights to the renderer's blueprint and terrain mesh.
    void SyncTerrain(bool vis);

    VMContext* Context; //used for access to objects

    void SetRoof(float pitch, uint32_t style);
    VMArchitecture(int32_t width, int32_t height, FSO::LotView::Model::Blueprint* blueprint, VMContext* context);
    void SetTimeOfDay(double time);
    void SetObjectSupported(int16_t x, int16_t y, int8_t level, bool support);
    void RegenerateSupported(int32_t level);
    void SignalRedraw();
    void RegenRoomMap();
    void Tick();
    int32_t SimulateCommands(std::vector<Model::VMArchitectureCommand>& commands, bool visualChange);
    int32_t RunCommands(std::vector<Model::VMArchitectureCommand>& commands, bool transient);
    void UpdateBuildableArea(const st::Rectangle& area, int32_t floors);

    /// Checks if there is a wall between two (Full Tile) points.
    bool RaycastWall(const st::Point& p1, const st::Point& p2, int8_t level);
    void SetWall(int16_t tileX, int16_t tileY, int8_t level, const FSO::LotView::Model::WallTile& wall);
    FSO::LotView::Model::WallTile GetWall(int16_t tileX, int16_t tileY, int8_t level);
    FSO::LotView::Model::FloorTile GetFloor(int16_t tileX, int16_t tileY, int8_t level);
    bool SetFloor(int16_t tileX, int16_t tileY, int8_t level, const FSO::LotView::Model::FloorTile& floor, bool force);
    uint16_t GetOffset(int32_t tileX, int32_t tileY) { return (uint16_t)((tileY * Width) + tileX); }
    void RegenWallsAt();

private:
    FSO::LotView::Model::Blueprint* WorldUI;
    bool RealMode;
    bool WallsDirty;
    bool FloorsDirty;
    bool Redraw;
    static const st::Color m_TimeColors[12];
    static const st::Point SupportSpread[4];
    FSO::Content::WallReference* GetPatternRef(uint16_t id);
    FSO::Content::FloorReference* GetFloorRef(uint16_t id);
    void SyncBlueprint();
};
}}
