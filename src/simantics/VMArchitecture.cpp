/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.VMArchitecture
#include "simantics/VMArchitecture.h"
#include "simantics/VMContext.h"
#include "simantics/VM.h"
#include "simantics/VMClock.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/entities/VMGameObject.h"
#include "simantics/utils/VMArchitectureTools.h"
#include "content/ContentManager.h"
#include <cmath>
#include <algorithm>
namespace FSO { namespace SimAntics {
using namespace FSO::LotView::Model;
using namespace FSO::SimAntics::Model;
using FSO::SimAntics::Utils::VMArchitectureTools;

const st::Color VMArchitecture::m_TimeColors[12] = {
    st::Color(50, 70, 122) * 1.5f,
    st::Color(50, 70, 122) * 1.5f,
    st::Color(60, 80, 132) * 1.5f,
    st::Color(60, 80, 132) * 1.5f,
    st::Color(217, 109, 0),
    st::Color(255, 255, 255),
    st::Color(255, 255, 255),
    st::Color(255, 255, 255),
    st::Color(255, 255, 255),
    st::Color(217, 109, 0),
    st::Color(60, 80, 80) * 1.5f,
    st::Color(60, 80, 132) * 1.5f,
};

const st::Point VMArchitecture::SupportSpread[4] = { st::Point(0, 1), st::Point(1, 0), st::Point(0, -1), st::Point(-1, 0) };

void VMArchitecture::SyncBlueprint() {
    if (WorldUI == nullptr) return;
    WorldUI->Walls = &Walls;
    WorldUI->WallsAt = &WallsAt;
    WorldUI->Floors = &Floors;
    WorldUI->Supported = &Supported;
}

void VMArchitecture::SetRoof(float pitch, uint32_t style) {
    RoofPitch = pitch;
    RoofStyle = style;
    if (VM::UseWorld() && WorldUI != nullptr) {
        WorldUI->RoofComp.SetStylePitch(style, pitch);
    }
}

VMArchitecture::VMArchitecture(int32_t width, int32_t height, Blueprint* blueprint, VMContext* context) {
    // TS1(sims): FreeSO (Content.TS1): DisableClip, TerrainLimit = the lot minus its edge
    DisableClip = true;
    TerrainLimit = st::Rectangle(1, 1, width - 2, height - 2);
    Terrain.reset(new VMArchitectureTerrain(width, height));
    this->Context = context;
    this->Width = width;
    this->Height = height;

    auto numTiles = (size_t)(width * height);
    this->WallsAt.resize(Stories);
    this->Walls.resize(Stories);
    this->VisWalls.resize(Stories);

    this->Floors.resize(Stories);
    this->VisFloors.resize(Stories);

    this->ObjectSupport.resize(Stories); //true if there's an object support in the specified position
    this->Supported.resize(Stories - 1); //no supported array for bottom floor. true if this tile is supported.

    this->Rooms.resize(Stories);

    for (int i = 0; i < Stories; i++) {
        this->Walls[i].assign(numTiles, WallTile());
        this->VisWalls[i].assign(numTiles, WallTile());

        this->Floors[i].assign(numTiles, FloorTile());
        this->VisFloors[i].assign(numTiles, FloorTile());
        this->ObjectSupport[i].assign(numTiles, false);

        if (i < Stories - 1) this->Supported[i].assign(numTiles, false);
    }

    this->WorldUI = blueprint;
    SyncBlueprint();

    WallsDirty = true;
    FloorsDirty = true;
    RealMode = true;
    Redraw = true;
}

void VMArchitecture::SetTimeOfDay(double time) {
    int idx = (int)std::floor(time * (12 - 1));
    idx = std::max(0, std::min(10, idx));
    st::Color col1 = m_TimeColors[idx]; //first colour
    st::Color col2 = m_TimeColors[idx + 1]; //second colour
    double Progress = std::fmod(time * (12 - 1), 1.0); //interpolation progress (mod 1)

    if (WorldUI != nullptr) WorldUI->OutsideColor = st::Color::Lerp(col1, col2, (float)Progress); //linearly interpolate between the two colours for this specific time.
}

void VMArchitecture::SetObjectSupported(int16_t x, int16_t y, int8_t level, bool support) {
    ObjectSupport[level - 1][y * Width + x] = support;
    RegenerateSupported(level + 1);
}

void VMArchitecture::RegenerateSupported(int32_t level) {
    if (level < 2 || level > Stories) return;
    auto& objSup = ObjectSupport[level - 2];
    auto& sup = Supported[level - 2];
    auto& floors = Floors[level - 1];
    auto& rooms = Rooms[level - 2];
    if (rooms.Map.empty()) return;

    int offset = 0;
    for (int y = 0; y < Height; y++) {
        for (int x = 0; x < Width; x++) {
            //if we are an object support or are above a room that is not outside, we're supported.
            if (objSup[offset] || !RoomData[(uint16_t)(rooms.Map[offset])].IsOutside) sup[offset] = true;
            else {
                //if we are a floor tile or are next to the floor tile, do the full 5x5 check.
                bool step1 = false;
                for (int i = 0; i < 4; i++) {
                    int newX = x + SupportSpread[i].X;
                    int newY = y + SupportSpread[i].Y;
                    if (newX < 0 || newX >= Width || newY < 0 || newY >= Height) continue;
                    int newOff = newY * Width + newX;
                    if (floors[newOff].Pattern != 0) {
                        step1 = true;
                        break;
                    }
                }

                if (step1) {
                    bool step2 = false;
                    for (int y2 = -2; y2 < 3; y2++) {
                        for (int x2 = -2; x2 < 3; x2++) {
                            int newX = x + x2;
                            int newY = y + y2;
                            if (newX < 0 || newX >= Width || newY < 0 || newY >= Height) continue;
                            int newOff = newY * Width + newX;
                            if (!RoomData[(uint16_t)rooms.Map[newOff]].IsOutside || (objSup[newOff] && (std::abs(x2) < 2 && std::abs(y2) < 2))) {
                                step2 = true;
                                break;
                            }
                        }
                        if (step2) break;
                    }
                    sup[offset] = step2;
                }
                else sup[offset] = false;
            }
            offset++;
        }
    }
}

void VMArchitecture::SignalRedraw() {
    Redraw = true;
}

void VMArchitecture::RegenRoomMap() {
    RoomData.clear();
    RoomData.push_back(VMRoom()); //dummy at index 0
    for (int i = 0; i < Stories; i++) {
        Rooms[i].GenerateMap(Walls[i], Floors[i], Width, Height, RoomData);
        if (VM::UseWorld() && WorldUI != nullptr) WorldUI->RoomMap[i] = &Rooms[i].Map;
        RegenerateSupported(i + 1);
    }
    if (VM::UseWorld() && WorldUI != nullptr) {
        // HOOK(world): FreeSO VMArchitecture.RegenRoomMap shares the room list with the blueprint; the roof generator
        // (RoofComponent.TileIndoors) needs to know which rooms are outside.
        WorldUI->Rooms.resize(RoomData.size());
        for (size_t i = 0; i < RoomData.size(); i++) {
            auto& r = WorldUI->Rooms[i];
            r.RoomID = RoomData[i].RoomID; r.IsOutside = RoomData[i].IsOutside; r.Area = RoomData[i].Area;
            r.IsPool = RoomData[i].IsPool; r.Bounds = RoomData[i].Bounds;
        }
        WorldUI->RoofComp.ShapeDirty = true;
        WorldUI->SignalRoomChange();
    }
}

void VMArchitecture::Tick() {
    if (WallsDirty || FloorsDirty) {
        RegenRoomMap();
        if (WallsChanged) WallsChanged(this);
        if (WorldUI != nullptr) WorldUI->RoofComp.SetStylePitch(RoofStyle, RoofPitch);
    }

    if (FloorsDirty) {
        for (int i = 1; i < Stories; i++)
            RegenerateSupported(i + 1);
    }
    if (TerrainDirty) {
        // TS1(sims): FreeSO VMArchitecture.Tick
        Terrain->RegenerateCenters();
        SyncTerrain(false);
        TerrainDirty = false;
    }
    if (VM::UseWorld() && Redraw) {
        LastFailReason = 0;
        LastTestCost = SimulateCommands(Commands, true);
        if (WorldUI != nullptr) {
            WorldUI->SignalWallChange();
            WorldUI->SignalFloorChange();
        }
        if (TerrainDirty) {
            SyncTerrain(true); // the preview heights go to the terrain mesh only (FreeSO UpdateTerrain(VisHeights))
            TerrainDirty = false;
        }
    }

    auto clock = Context->Clock.get();
    SetTimeOfDay(clock->Hours / 24.0 + clock->Minutes / (24.0 * 60) + clock->Seconds() / (24.0 * 60 * 60));

    FloorsDirty = false;
    Redraw = false;
    WallsDirty = false;
}

int32_t VMArchitecture::SimulateCommands(std::vector<VMArchitectureCommand>& commands, bool visualChange) {
    int cost;
    if (commands.size() == 0) {
        if (visualChange) {
            //direct copy, no changes to make
            SyncBlueprint();
            if (TerrainPreviewShown) { TerrainPreviewShown = false; Terrain->VisHeights = Terrain->Heights; Terrain->VisGrass = Terrain->GrassState; TerrainDirty = true; }
        }
        return 0;
    } else {
        RealMode = false;
        //C# points Walls/Floors at the Vis* copies, runs the commands, then restores the originals.
        auto oldWalls = Walls;
        auto oldWallsAt = WallsAt;
        auto oldFloors = Floors;
        bool terrainWasDirty = TerrainDirty;
        TerrainDirty = false;
        Terrain->EnterVis();

        cost = RunCommands(commands, visualChange);
        bool terrainChanged = TerrainDirty;
        Terrain->ExitVis();
        // TS1(sims): FreeSO keeps TerrainDirty set by a visual simulation so Tick hands VisHeights to the mesh;
        // a simulation that changed nothing must still drop an older preview (TerrainDirty from before).
        TerrainDirty = visualChange ? (terrainChanged || terrainWasDirty || TerrainPreviewShown) : terrainWasDirty;
        TerrainPreviewShown = visualChange && terrainChanged;

        VisFloors = Floors;
        VisWalls = Walls;
        VisWallsAt = WallsAt;

        Floors = std::move(oldFloors);
        Walls = std::move(oldWalls);
        WallsAt = std::move(oldWallsAt);
        RealMode = true;
        if (visualChange && WorldUI != nullptr) {
            //upload modified walls to blueprint (C#: WorldUI.Walls = Walls; WorldUI.WallsAt = WallsAt; WorldUI.Floors = Floors)
            WorldUI->Walls = &VisWalls;
            WorldUI->WallsAt = &VisWallsAt;
            WorldUI->Floors = &VisFloors;
        } else SyncBlueprint();
    }
    return cost;
}

int32_t VMArchitecture::RunCommands(std::vector<VMArchitectureCommand>& commands, bool transient) {
    int cost = 0; //negative for sellback;
    int pdCount = 0;
    uint16_t pdVal = 0;
    VMAvatar* lastAvatar = nullptr;
    for (size_t i = 0; i < commands.size(); i++) {
        auto& com = commands[i];
        VMEntity* avaEnt = nullptr;
        for (auto x : Context->VM->Entities) if (x->PersistID == com.CallerUID) { avaEnt = x; break; }
        if ((avaEnt == nullptr || dynamic_cast<VMGameObject*>(avaEnt) != nullptr) && !transient) return 0; //we need an avatar to run a command from net
        auto avatar = (transient) ? nullptr : (VMAvatar*)avaEnt;
        lastAvatar = avatar;
        auto walls = FSO::Content::Content::Get()->WorldWalls.get();
        int styleInd = -1;
        auto si = walls->WallStyleToIndex.find(com.style);
        if (si != walls->WallStyleToIndex.end()) styleInd = si->second;
        // STUB(3ds): chat events (VMChatEvent "placed N walls" etc) are not emitted.
        //if patterns are invalid, don't do anything.
        switch (com.Type) {
            case VMArchitectureCommandType::WALL_LINE: {
                if (styleInd == -1) break; //MUST be purchasable style
                auto lstyle = walls->GetWallStyle(com.style);
                auto nwCount = VMArchitectureTools::DrawWall(this, st::Point(com.x, com.y), com.x2, com.y2, com.pattern, com.style, com.level, false);
                if (nwCount > 0) {
                    cost += nwCount * ((lstyle == nullptr) ? 0 : lstyle->Price);
                }
                break;
            }
            case VMArchitectureCommandType::WALL_DELETE: {
                auto dwCount = VMArchitectureTools::EraseWall(this, st::Point(com.x, com.y), com.x2, com.y2, com.pattern, com.style, com.level);
                if (dwCount > 0) {
                    cost -= 7 * dwCount;
                }
                break;
            }
            case VMArchitectureCommandType::WALL_RECT: {
                if (styleInd == -1) break; //MUST be purchasable style
                auto rstyle = walls->GetWallStyle(com.style);
                auto rwCount = VMArchitectureTools::DrawWallRect(this, st::Rectangle(com.x, com.y, com.x2, com.y2), com.pattern, com.style, com.level);
                if (rwCount > 0) {
                    cost += rwCount * ((rstyle == nullptr) ? 0 : rstyle->Price);
                }
                break;
            }
            case VMArchitectureCommandType::PATTERN_FILL: {
                auto pattern = GetPatternRef(com.pattern);
                if (pattern == nullptr && com.pattern != 0) break;
                auto pfCount = VMArchitectureTools::WallPatternFill(this, st::Point(com.x, com.y), com.pattern, com.level);
                if (pfCount.Total > 0) {
                    cost -= pfCount.Cost - pfCount.Cost / 5;
                    cost += (pattern == nullptr) ? 0 : pattern->Price * pfCount.Total;
                }
                break;
            }
            case VMArchitectureCommandType::PATTERN_DOT: {
                auto pdpattern = GetPatternRef(com.pattern);
                if (pdpattern == nullptr && com.pattern != 0) break;
                auto dot = VMArchitectureTools::WallPatternDot(this, st::Point(com.x, com.y), com.pattern, com.x2, com.y2, com.level);
                pdVal = com.pattern;
                if (dot.Total > -1) {
                    cost -= dot.Cost - dot.Cost / 5;
                    cost += (pdpattern == nullptr) ? 0 : pdpattern->Price;
                    pdCount++;
                }
                break;
            }
            case VMArchitectureCommandType::FLOOR_FILL: {
                auto ffpattern = GetFloorRef(com.pattern);
                if (ffpattern == nullptr && com.pattern != 0) break;
                auto ffCount = VMArchitectureTools::FloorPatternFill(this, st::Point(com.x, com.y), com.pattern, com.level);
                if (ffCount.Total > 0) {
                    cost -= (ffCount.Cost - ffCount.Cost / 5) / 2;
                    cost += (ffpattern == nullptr) ? 0 : ffpattern->Price * ffCount.Total / 2;
                }
                break;
            }
            case VMArchitectureCommandType::FLOOR_RECT: {
                auto frpattern = GetFloorRef(com.pattern);
                if (frpattern == nullptr && com.pattern != 0) break;
                auto frCount = VMArchitectureTools::FloorPatternRect(this, st::Rectangle(com.x, com.y, com.x2, com.y2), com.style, com.pattern, com.level);
                if (frCount.Total > 0) {
                    cost -= (frCount.Cost - frCount.Cost / 5) / 2;
                    cost += (frpattern == nullptr) ? 0 : frpattern->Price * frCount.Total / 2;
                }
                break;
            }
            // TS1(sims): FreeSO terrain commands
            case VMArchitectureCommandType::TERRAIN_RAISE:
            case VMArchitectureCommandType::TERRAIN_FLATTEN: {
                auto height = (int16_t)com.style;
                st::Rectangle rect;
                if (com.Type == VMArchitectureCommandType::TERRAIN_FLATTEN) rect = st::Rectangle(com.x, com.y, com.x2, com.y2);
                else rect = st::Rectangle(com.x, com.y, 0, 0);

                auto terrainCount = VMArchitectureTools::RaiseTerrain(this, rect, height, com.pattern > 0);
                if (terrainCount > 0) {
                    cost += terrainCount;
                }
                break;
            }
            case VMArchitectureCommandType::GRASS_DOT: {
                auto dotCount = VMArchitectureTools::DotTerrain(this, st::Point(com.x, com.y), (int16_t)com.pattern);
                cost += dotCount;
                break;
            }
        }
    }
    (void)lastAvatar; (void)pdCount; (void)pdVal; // used only by the (stubbed) chat event
    return cost;
}

void VMArchitecture::UpdateBuildableArea(const st::Rectangle& area, int32_t floors) {
    //notify the lotview this has changed too, so it can be drawn.
    BuildableArea = area;
    BuildableFloors = floors;
    if (VM::UseWorld() && WorldUI != nullptr) {
        WorldUI->BuildableArea = BuildableArea;
        // WorldUI.Terrain.TerrainDirty = true;
    }
}

FSO::Content::WallReference* VMArchitecture::GetPatternRef(uint16_t id) {
    auto& wallEntries = FSO::Content::Content::Get()->WorldWalls->Entries;
    auto it = wallEntries.find(id);
    return (it == wallEntries.end()) ? nullptr : &it->second;
}

FSO::Content::FloorReference* VMArchitecture::GetFloorRef(uint16_t id) {
    auto& e = FSO::Content::Content::Get()->WorldFloors->Entries;
    auto it = e.find(id);
    return (it == e.end()) ? nullptr : &it->second;
}

/// Checks if there is a wall between two (Full Tile) points.
// FIX(sims) QA4: when the line passes exactly through a tile corner (a 45-degree step: error + errorprev == ddx/ddy)
// FreeSO/FreeSims took the "y before x" detour in one direction and the "x before y" detour in the other, so A->B
// could be clear while B->A was blocked by a fence/wall touching the corner. The Unleashed rabbit snaps one tile
// diagonally, creates its Reserve Tile there and snaps back with the reverse slot: the reverse ray met the other
// side of the corner, no goal was found and Maxis' breakpoint reset it (QA3 Q6: Burb lot 63). Exactly through a
// corner is now blocked only when both detours are, the same both ways.
bool VMArchitecture::RaycastWall(const st::Point& p1, const st::Point& p2, int8_t level) {
    //Bresenham's line algorithm, modified to check walls
    //http://lifc.univ-fcomte.fr/home/~ededu/projects/bresenham/

    int i, ystep, xstep, error, errorprev, ddy, ddx,
        y = p1.Y,
        x = p1.X,
        dx = p2.X - x,
        dy = p2.Y - y;

    if (dy < 0) {
        ystep = -1;
        dy = -dy;
    }
    else
        ystep = 1;

    if (dx < 0) {
        xstep = -1;
        dx = -dx;
    }
    else
        xstep = 1;

    ddy = dy * 2;
    ddx = dx * 2;

    int xAOff = (xstep > 0) ? 1 : 0;
    int yAOff = (ystep > 0) ? 1 : 0;

    if (ddx >= ddy) {
        errorprev = error = dx;
        for (i = 0; i < dx; i++) {
            int oldx = x;
            int oldy = y;
            x += xstep;
            error += ddy;
            if (error > ddx) {
                y += ystep;
                error -= ddx;

                //extra steps
                bool xFirst = GetWall((int16_t)(oldx + xAOff), (int16_t)(oldy), level).TopLeftSolid()
                    || GetWall((int16_t)(x), (int16_t)(oldy + yAOff), level).TopRightSolid();
                bool yFirst = GetWall((int16_t)(oldx), (int16_t)(oldy + yAOff), level).TopRightSolid()
                    || GetWall((int16_t)(oldx + xAOff), (int16_t)(y), level).TopLeftSolid();
                if (error + errorprev < ddx) {
                    //moved into x before y
                    if (xFirst) return true;
                } else if (error + errorprev > ddx) {
                    //moved into y before x
                    if (yFirst) return true;
                } else if (xFirst && yFirst) return true; // FIX(sims) QA4: see below
            } else {
                //only move into x
                if (GetWall((int16_t)(oldx + xAOff), (int16_t)(oldy), level).TopLeftSolid()) return true;
            }
            errorprev = error;
        }
    } else {
        errorprev = error = dy;
        for (i = 0; i < dy; i++) {
            int oldx = x;
            int oldy = y;
            y += ystep;
            error += ddx;
            if (error > ddy) {
                x += xstep;
                error -= ddy;

                //extra steps
                bool xFirst = GetWall((int16_t)(oldx + xAOff), (int16_t)(oldy), level).TopLeftSolid()
                    || GetWall((int16_t)(x), (int16_t)(oldy + yAOff), level).TopRightSolid();
                bool yFirst = GetWall((int16_t)(oldx), (int16_t)(oldy + yAOff), level).TopRightSolid()
                    || GetWall((int16_t)(oldx + xAOff), (int16_t)(y), level).TopLeftSolid();
                if (error + errorprev < ddy) {
                    //moved into y before x
                    if (yFirst) return true;
                } else if (error + errorprev > ddy) {
                    //moved into x before y
                    if (xFirst) return true;
                } else if (xFirst && yFirst) return true; // FIX(sims) QA4: see below
            } else {
                //only move into y
                if (GetWall((int16_t)(oldx), (int16_t)(oldy + yAOff), level).TopRightSolid()) return true;
            }

            errorprev = error;
        }
    }
    return false;
}

void VMArchitecture::SetWall(int16_t tileX, int16_t tileY, int8_t level, const WallTile& wall) {
    int32_t off = GetOffset(tileX, tileY);

    auto& wa = WallsAt[level - 1];
    auto it = std::find(wa.begin(), wa.end(), off);
    if (it != wa.end()) wa.erase(it);
    if (wall.Segments > 0) {
        Walls[level - 1][off] = wall;
        wa.push_back(off);
    } else {
        Walls[level - 1][off] = WallTile();
    }

    if (RealMode) WallsDirty = true;
    Redraw = true;
}

WallTile VMArchitecture::GetWall(int16_t tileX, int16_t tileY, int8_t level) {
    size_t off = GetOffset(tileX, tileY);
    auto& w = Walls[level - 1];
    if (off >= w.size()) return WallTile(); // C# would throw IndexOutOfRange
    return w[off];
}

FloorTile VMArchitecture::GetFloor(int16_t tileX, int16_t tileY, int8_t level) {
    size_t offset = GetOffset(tileX, tileY);
    auto& f = Floors[level - 1];
    if (offset >= f.size()) return FloorTile(); // C# would throw IndexOutOfRange
    return f[offset];
}

bool VMArchitecture::SetFloor(int16_t tileX, int16_t tileY, int8_t level, const FloorTile& floor, bool force) {
    //returns false on failure
    auto offset = GetOffset(tileX, tileY);

    if (!force) {
        //first check if we're supported
        if (floor.Pattern > 65533 && level > 1 && RoomData[(int)Rooms[level - 2].Map[offset] & 0xFFFF].IsOutside) return false;
        if (level > 1 && !Supported[level - 2][offset]) return false;
        //check if objects need/don't need floors
        if (!Context->CheckFloorValid(LotTilePos::FromBigTile((int16_t)tileX, (int16_t)tileY, level), floor)) return false;
    }

    Floors[level - 1][offset] = floor;

    if (RealMode) FloorsDirty = true;
    Redraw = true;
    return true;
}

// ---- TS1(sims): FreeSO terrain accessors / OutsideClip
bool VMArchitecture::GetTerrainSloped(int16_t tileX, int16_t tileY) {
    auto off = (size_t)GetOffset(tileX, tileY);
    return off < Terrain->Sloped.size() && Terrain->Sloped[off];
}

void VMArchitecture::SetTerrainHeight(int16_t tileX, int16_t tileY, int16_t height) {
    auto off = (size_t)GetOffset(tileX, tileY);
    Terrain->Heights[off] = height;
    TerrainDirty = true;
    Redraw = true;
}

int16_t VMArchitecture::GetTerrainHeight(int16_t tileX, int16_t tileY) {
    return Terrain->Heights[(size_t)GetOffset(tileX, tileY)];
}

void VMArchitecture::SetTerrainGrass(int16_t tileX, int16_t tileY, uint8_t grass) {
    Terrain->GrassState[(size_t)GetOffset(tileX, tileY)] = grass;
    TerrainDirty = true;
    Redraw = true;
}

uint8_t VMArchitecture::GetTerrainGrass(int16_t tileX, int16_t tileY) {
    return Terrain->GrassState[(size_t)GetOffset(tileX, tileY)];
}

bool VMArchitecture::OutsideClip(int16_t tileX, int16_t tileY, int8_t level) {
    // TS1(sims): map_edit cheat: only the lot's own bounds limit the build tools
    if (MapEdit) return (tileX < 0 || tileY < 0 || level < 1 || tileX >= Width || tileY >= Height || level > Stories);
    auto& fine = FineBuildableArea;
    if (!fine.empty()) {
        if (tileX < 0 || tileX >= Width) return true;
        else if (tileY < 0 || tileY >= Height) return true;
        else if (level < 1) return true;
        else return !fine[(size_t)(tileX + tileY * Width)];
    }
    auto area = BuildableArea;
    if (DisableClip)
        return (tileX < 0 || tileY < 0 || level < 1 || tileX >= Width || tileY >= Height || level > Stories);
    else
        return (tileX < area.X || tileY < area.Y || level < 1 || tileX >= area.Right() || tileY >= area.Bottom() || level > BuildableFloors);
}

void VMArchitecture::SyncTerrain(bool vis) {
    if (WorldUI == nullptr) return;
    if (!vis) {
        WorldUI->Altitude = &Terrain->Heights;
        WorldUI->AltitudeCenters = &Terrain->Centers;
        WorldUI->FineArea = FineBuildableArea.empty() ? nullptr : &FineBuildableArea;
    }
    if (WorldUI->Terrain) {
        // FreeSO TerrainComponent.UpdateTerrain(light, dark, heights, grass): heights in tile units (GetAltPoint rules)
        auto& h = vis ? Terrain->VisHeights : Terrain->Heights;
        auto& g = vis ? Terrain->VisGrass : Terrain->GrassState;
        std::vector<float> fh(h.size());
        for (int y = 0; y < Height; y++) for (int x = 0; x < Width; x++) {
            size_t i = (size_t)(y * Width + x);
            fh[i] = (x <= 0 || y <= 0) ? 0.0f : (h[i] - WorldUI->BaseAlt) * WorldUI->TerrainFactor;
        }
        WorldUI->Terrain->SetHeights(fh, Width);
        if (!g.empty()) WorldUI->Terrain->SetGrassState(g, Width);
    }
    // the floor / wall / object layers move with the terrain
    WorldUI->SignalFloorChange();
    WorldUI->SignalWallChange();
}

void VMArchitecture::RegenWallsAt() {
    WallsAt.assign(Stories, std::vector<int32_t>());
    for (int i = 0; i < Stories; i++) {
        auto& list = WallsAt[i];
        auto& wIt = Walls[i];
        for (size_t j = 0; j < wIt.size(); j++) {
            if (wIt[j].Segments > 0) list.push_back((int32_t)j);
        }
    }
}
}}
