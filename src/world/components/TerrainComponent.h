/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.LotView.Components.TerrainComponent (World/components/TerrainComponent.cs, lot types from FreeSO)
//
// FreeSims/FreeSO draw the lot terrain as a 3D mesh with GrassShader.fx: per-vertex colour blended from the lot
// type's light (healthy) and dark (dead) colours by the TS1 lawn state (house ARRY 6), plus darker grass blades.
// Here the same mesh (two triangles per tile, per-vertex colour and altitude) is drawn with the software
// TriangleRaster into the cached floor layer, modulated by a tileable blade/speckle texture in tile space so the
// grain is continuous across tiles and rotations, and lit by the outside light (C# DiffuseColor = OutsideColor).
// Lot types (FreeSO VMTS1Activator.HouseNumToType + LotTypeGrassInfo): grass, sand (beach lots 28/29/46-48: grass
// fading to sand), snow (Vacation 40-42), Magic Town dark grass (90-94) and autumn grass (95/96), cloud (99).
// Depth: the terrain is written slightly behind the floor plane so floor sprites always win on equal depth.
#pragma once
#include <cstdint>
#include <vector>
#include "core/math.h"
#include "world/components/WorldComponent.h"
#include "world/utils/TriangleRaster.h"

namespace FSO { namespace LotView {
class Framebuffer;
namespace Model { class Blueprint; }
namespace Components {

/// FSO.Content.Model.TerrainType (values as in FreeSO).
enum class TerrainType : int { GRASS = 0, SAND = 1, ROCK = 2, SNOW = 3, WATER = 4, TS1DarkGrass = 5, TS1AutumnGrass = 6, TS1Cloud = 7 };

class TerrainComponent : public WorldComponent {
public:
    explicit TerrainComponent(const st::Rectangle& size);

    st::Rectangle Area;       // C# private Size
    bool TerrainDirty = false;
    bool DepthMode = false;
    bool DrawGrid = false;

    TerrainType LightType = TerrainType::GRASS;
    TerrainType DarkType = TerrainType::GRASS;

    float PreferredDrawOrder() override { return 0.0f; }

    /// FreeSO VMTS1Activator: lot type from the TS1 house number.
    void SetTS1HouseNumber(int32_t houseNumber);
    /// FreeSO UpdateTerrain: light/dark types (UpdateLotType picks the colours).
    void UpdateLotType();
    /// Lawn state per tile, row major blueprint-sized (0 = green, 255 = brown; TS1 ARRY 6 as (byte)(127 - x)).
    void SetGrassState(const std::vector<uint8_t>& grass, int32_t width);
    /// Vertex altitude in tiles (width x width vertices, same layout as GrassState). Empty = flat.
    void SetHeights(const std::vector<float>& heights, int32_t width);
    float GetElevationPoint(int x, int y) const;

    void RegenTerrain(WorldState& world, FSO::LotView::Model::Blueprint* blueprint);
    /// Draws the terrain mesh into target (the floor layer; scroll = world._2D->GetScroll()).
    void DrawInto(WorldState& world, Framebuffer& target);
    void Draw(WorldState& world) override { (void)world; }

    Utils::RasterStats LastStats;

private:
    st::Color LightGreen = st::Color(80, 116, 59);
    st::Color LightBrown = st::Color(157, 117, 65);
    st::Color DarkGreen = st::Color(8, 52, 8);
    st::Color DarkBrown = st::Color(81, 60, 18);
    std::vector<uint8_t> GrassState; //0 = green, 255 = brown. to start with, should be randomly distriuted in range 0-128.
    int32_t GrassWidth = 0;
    std::vector<float> Heights;
    int32_t HeightWidth = 0;
    FSO::LotView::Model::Blueprint* Blueprint = nullptr;
    Utils::RasterTexture Blades; // 128x128 tileable luminance (x200 = 1.0)

    void GenerateGrassStates();
    void GenerateBlades();
    float StateAt(int x, int y) const;
};

}}}
