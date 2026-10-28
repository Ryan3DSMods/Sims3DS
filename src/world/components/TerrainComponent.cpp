/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.LotView.Components.TerrainComponent — software terrain mesh, see TerrainComponent.h
#include "world/components/TerrainComponent.h"
#include "world/model/Blueprint.h"
#include "world/WorldState.h"
#include "world/Framebuffer.h"
#include "world/utils/_2DWorldBatch.h"
#include <cmath>
#include <algorithm>

namespace FSO { namespace LotView { namespace Components {

using namespace FSO::LotView::Utils;

// FSO.LotView.Model.LotTypeGrassInfo (FreeSO)
namespace LotTypeGrassInfo {
static const st::Color LightGreen[8] = {
    st::Color(80, 116, 59), st::Color(181, 171, 149), st::Color(126, 96, 70), st::Color(240, 245, 250), st::Color(0, 0, 255),
    st::Color(74, 89, 66), //TS1 Dark Grass
    st::Color(140, 113, 49), //TS1 Autumn Grass
    st::Color(240, 245, 250), //TS1 Cloud
};
static const st::Color LightBrown[8] = {
    st::Color(157, 117, 65), st::Color(196, 185, 162), st::Color(126, 96, 70), st::Color(240, 245, 250), st::Color(0, 0, 255),
    st::Color(90, 69, 41), st::Color(115, 73, 33), st::Color(15, 20, 140),
};
static const st::Color DarkGreen[8] = {
    st::Color(8, 52, 8), st::Color(115, 109, 95), st::Color(107, 77, 57), st::Color(180, 180, 190), st::Color(0, 0, 255),
    st::Color(21, 30, 13), st::Color(109, 63, 35), st::Color(180, 180, 190),
};
static const st::Color DarkBrown[8] = {
    st::Color(81, 60, 18), st::Color(121, 114, 100), st::Color(107, 77, 57), st::Color(180, 180, 190), st::Color(0, 0, 255),
    st::Color(64, 69, 14), st::Color(56, 35, 17), st::Color(15, 20, 140),
};
}

TerrainComponent::TerrainComponent(const st::Rectangle& size) : Area(size) {
    GenerateGrassStates();
    UpdateLotType();
}

void TerrainComponent::SetTS1HouseNumber(int32_t houseNumber) {
    // FreeSO VMTS1Activator.HouseNumToType
    TerrainType ttype = TerrainType::GRASS;
    switch (houseNumber) {
        case 28: case 29: case 46: case 47: case 48: ttype = TerrainType::SAND; break;
        case 40: case 41: case 42: ttype = TerrainType::SNOW; break;
        case 90: case 91: case 92: case 93: case 94: ttype = TerrainType::TS1DarkGrass; break;
        case 95: case 96: ttype = TerrainType::TS1AutumnGrass; break;
        case 99: ttype = TerrainType::TS1Cloud; break;
        default: break;
    }
    LightType = (ttype == TerrainType::SAND) ? TerrainType::GRASS : ttype;
    DarkType = ttype;
    UpdateLotType();
    TerrainDirty = true;
}

void TerrainComponent::UpdateLotType() {
    int index = (int)LightType;
    LightGreen = LotTypeGrassInfo::LightGreen[index];
    DarkGreen = LotTypeGrassInfo::DarkGreen[index];
    if (LightType != DarkType) {
        auto dindex = (int)DarkType;
        LightBrown = LotTypeGrassInfo::LightGreen[dindex];
        DarkBrown = LotTypeGrassInfo::DarkGreen[dindex];
    } else {
        LightBrown = LotTypeGrassInfo::LightBrown[index];
        DarkBrown = LotTypeGrassInfo::DarkBrown[index];
    }
}

void TerrainComponent::SetGrassState(const std::vector<uint8_t>& grass, int32_t width) {
    GrassState = grass;
    GrassWidth = width;
    TerrainDirty = true;
}

void TerrainComponent::SetHeights(const std::vector<float>& heights, int32_t width) {
    Heights = heights;
    HeightWidth = width;
    TerrainDirty = true;
}

float TerrainComponent::GetElevationPoint(int x, int y) const {
    if (Heights.empty() || HeightWidth <= 0) return 0.0f;
    x = std::max<int32_t>(0, std::min<int32_t>(HeightWidth - 1, x));
    y = std::max<int32_t>(0, std::min<int32_t>(HeightWidth - 1, y));
    size_t i = (size_t)(y * HeightWidth + x);
    return (i < Heights.size()) ? Heights[i] : 0.0f;
}

void TerrainComponent::GenerateGrassStates() { //generates a set of grass states for a lot.
    //right now only works for square lots, but that's all tso has!
    // FSO_CPP: C# uses an unseeded System.Random; a fixed-seed LCG keeps renders reproducible. Only used when the
    // house file has no lawn state (TS1 houses always do).
    uint32_t seed = 0x5EED1234u;
    auto next = [&seed]() { seed = seed * 1664525u + 1013904223u; return (seed >> 8) / 16777216.0; };
    int width = std::max<int32_t>(1, Area.Width + 2);
    std::vector<float> result((size_t)(width * width), 0.0f);
    int initial = width / 4; //divide by more for less noisyness!
    if (initial >= 2) {
        float factor = 0.5f / ((int)std::log2((double)initial));
        while (initial > 0) {
            auto squared = initial * initial;
            std::vector<float> noise((size_t)squared);
            for (int i = 0; i < squared; i++) noise[(size_t)i] = (float)next() * factor;
            int offset = 0;
            for (int x = 0; x < width; x++) {
                double xInt = (x / (double)(width - 1)) * (initial - 1);
                for (int y = 0; y < width; y++) {
                    double yInt = (y / (double)(width - 1)) * (initial - 1);
                    float tl = noise[(size_t)(std::floor(yInt) * initial + std::floor(xInt))];
                    float tr = noise[(size_t)(std::floor(yInt) * initial + std::ceil(xInt))];
                    float bl = noise[(size_t)(std::ceil(yInt) * initial + std::floor(xInt))];
                    float br = noise[(size_t)(std::ceil(yInt) * initial + std::ceil(xInt))];
                    float p = (float)std::fmod(xInt, 1.0);
                    float q = (float)std::fmod(yInt, 1.0);
                    result[(size_t)offset++] += (tl * (1 - p) + tr * (p)) * (1 - q) + (bl * (1 - p) + br * (p)) * q; //don't you love 2 dimensional linear interpolation?? ;)
                }
            }
            initial /= 2;
        }
    }
    GrassState.resize(result.size());
    for (size_t i = 0; i < result.size(); i++) GrassState[i] = (uint8_t)std::min(255.0f, result[i] * 255.0f);
    GrassWidth = width;
}

// Tileable grass grain (FreeSO draws blades with GrassShader; this is the flat-texture equivalent): value noise
// in two octaves plus per-texel speckles, mostly darker blades. 200 = unmodulated.
void TerrainComponent::GenerateBlades() {
    if (Blades.Valid()) return;
    const int N = 128;
    Blades.Width = Blades.Height = N;
    Blades.Pixels.resize((size_t)(N * N));
    auto hash = [](uint32_t x, uint32_t y, uint32_t s) {
        uint32_t h = x * 374761393u + y * 668265263u + s * 2246822519u;
        h = (h ^ (h >> 13)) * 1274126177u;
        return ((h ^ (h >> 16)) & 0xFFFF) / 65535.0f;
    };
    auto value = [&](float x, float y, int cell, uint32_t s) {
        int c = N / cell;
        float fx = x / cell, fy = y / cell;
        int x0 = (int)std::floor(fx), y0 = (int)std::floor(fy);
        float tx = fx - x0, ty = fy - y0;
        tx = tx * tx * (3 - 2 * tx); ty = ty * ty * (3 - 2 * ty);
        auto h = [&](int a, int b) { return hash((uint32_t)(((a % c) + c) % c), (uint32_t)(((b % c) + c) % c), s); };
        float a = h(x0, y0) * (1 - tx) + h(x0 + 1, y0) * tx;
        float b = h(x0, y0 + 1) * (1 - tx) + h(x0 + 1, y0 + 1) * tx;
        return a * (1 - ty) + b * ty;
    };
    for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
        float l = 1.0f + 0.16f * (value((float)x, (float)y, 32, 1) - 0.5f) + 0.10f * (value((float)x, (float)y, 8, 2) - 0.5f);
        float r = hash((uint32_t)x, (uint32_t)y, 3);
        if (r < 0.22f) l *= 0.78f + r;          // dark blade
        else if (r > 0.94f) l *= 1.12f;          // lit blade tip
        else l *= 0.96f + 0.08f * hash((uint32_t)x, (uint32_t)y, 4);
        int g = std::max(0, std::min(255, (int)(l * 200)));
        Blades.Pixels[(size_t)(y * N + x)] = Framebuffer::Pack((uint8_t)g, (uint8_t)g, (uint8_t)g, 255);
    }
}

void TerrainComponent::RegenTerrain(WorldState& world, FSO::LotView::Model::Blueprint* blueprint) {
    (void)world;
    Blueprint = blueprint;
    TerrainDirty = false;
}

float TerrainComponent::StateAt(int x, int y) const {
    if (GrassState.empty() || GrassWidth <= 0) return 0.0f;
    x = std::max<int32_t>(0, std::min<int32_t>(GrassWidth - 1, x));
    y = std::max<int32_t>(0, std::min<int32_t>(GrassWidth - 1, y));
    size_t i = (size_t)(y * GrassWidth + x);
    return (i < GrassState.size()) ? GrassState[i] / 255.0f : 0.0f;
}

void TerrainComponent::DrawInto(WorldState& world, Framebuffer& target) {
    LastStats = RasterStats();
    if (Blueprint == nullptr) return;
    GenerateBlades();
    auto& ws = world.WorldSpace;
    auto K = TileProjectionOrigin(world);
    auto cam = world.Camera();

    st::Color out = world.OutsideColor;
    if (out.A == 0 || !target.Room.empty()) out = st::Color::White(); // deferred: lit when composited
    // blade texture: 200 = 1.0
    float lr = out.R / (255.0f * 200.0f), lg = out.G / (255.0f * 200.0f), lb = out.B / (255.0f * 200.0f);
    // texels per tile: ~2 screen pixels per texel at every zoom
    float tpt = ws.TilePxWidth / 2.0f;

    // vertex grid over the terrain area (one more vertex than tiles on each axis)
    int x0 = Area.Left(), y0 = Area.Top(), x1 = std::min(Area.Right(), Blueprint->Width), y1 = std::min(Area.Bottom(), Blueprint->Height);
    if (x1 <= x0 || y1 <= y0) return;
    int vw = x1 - x0 + 1;
    std::vector<RasterVertex> verts((size_t)(vw * (y1 - y0 + 1)));
    for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
        // the colour of a vertex blends the lawn state of the tiles around it (C# GrassState per vertex)
        float state = (StateAt(x - 1, y - 1) + StateAt(x, y - 1) + StateAt(x - 1, y) + StateAt(x, y)) * 0.25f;
        st::Vector3 p((float)x, (float)y, GetElevationPoint(x, y));
        auto s = ws.GetScreenFromTile(p);
        auto& v = verts[(size_t)((y - y0) * vw + (x - x0))];
        v.x = s.X + K.X; v.y = s.Y + K.Y;
        v.z = cam->Depth(WorldSpace::GetWorldFromTile(p));
        v.u = x * tpt; v.v = y * tpt;
        auto c = st::Color::Lerp(LightGreen, LightBrown, state);
        auto d = st::Color::Lerp(DarkGreen, DarkBrown, state);
        // FreeSO mixes in the dark colour at the blade roots; a quarter of it on the surface
        float r = c.R * 0.8f + d.R * 0.2f, g = c.G * 0.8f + d.G * 0.2f, b = c.B * 0.8f + d.B * 0.2f;
        // TS1(sims): hills are shaded (FreeSO's grass shader lights the mesh normal): a fixed sun from the lot's
        // top-left, slopes facing it brighter, the others darker (heights in tile units)
        float shade = 1.0f;
        if (!Heights.empty()) {
            float dx = GetElevationPoint(x + 1, y) - GetElevationPoint(x - 1, y);
            float dy = GetElevationPoint(x, y + 1) - GetElevationPoint(x, y - 1);
            shade = std::max(0.55f, std::min(1.35f, 1.0f + 0.9f * (dx + dy)));
        }
        v.r = r * lr * shade; v.g = g * lg * shade; v.b = b * lb * shade;
    }
    // FSO_CPP: behind the floor plane, so floor sprites always cover the grass: the generated floor z-buffers step
    // 1.525 per row (exact would be 1.594) and are quantized to bytes, so floors are up to ~0.2 world units behind
    // the exact plane.
    const float bias = 0.4f;
    float cx0 = (float)std::max<int32_t>(0, target.ClipX0), cy0 = (float)std::max<int32_t>(0, target.ClipY0);
    float cx1 = (float)std::min(target.Width, target.ClipX1), cy1 = (float)std::min(target.Height, target.ClipY1);
    for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) {
        auto& a = verts[(size_t)((y - y0) * vw + (x - x0))];
        auto& b = verts[(size_t)((y - y0) * vw + (x + 1 - x0))];
        auto& c = verts[(size_t)((y + 1 - y0) * vw + (x + 1 - x0))];
        auto& d = verts[(size_t)((y + 1 - y0) * vw + (x - x0))];
        float minX = std::min(std::min(a.x, b.x), std::min(c.x, d.x)), maxX = std::max(std::max(a.x, b.x), std::max(c.x, d.x));
        float minY = std::min(std::min(a.y, b.y), std::min(c.y, d.y)), maxY = std::max(std::max(a.y, b.y), std::max(c.y, d.y));
        if (maxX < cx0 || maxY < cy0 || minX >= cx1 || minY >= cy1) continue;
        DrawTriangle(target, a, b, c, &Blades, 0, bias, &LastStats);
        DrawTriangle(target, a, c, d, &Blades, 0, bias, &LastStats);
    }
}

}}}
