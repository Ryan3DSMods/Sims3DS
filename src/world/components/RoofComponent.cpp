/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.LotView.Components.RoofComponent (World/components/RoofComponent.cs) — see RoofComponent.h
#include "world/components/RoofComponent.h"
#include "world/model/Blueprint.h"
#include "world/WorldState.h"
#include "world/Framebuffer.h"
#include "world/utils/_2DWorldBatch.h"
#include "content/ContentManager.h"
#include "vitaboy/Texture.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace FSO { namespace LotView { namespace Components {

using FSO::LotView::Model::LotTilePos;
using namespace FSO::LotView::Utils;

void RoofComponent::SetStylePitch(uint32_t style, float pitch) {
    Style = style;
    Pitch = pitch;
    if (blueprint != nullptr) blueprint->Damage.push_back(FSO::LotView::Model::BlueprintDamage(FSO::LotView::Model::BlueprintDamageType::ROOF_STYLE_CHANGED, 0, 0, 1));
}

// ---------------------------------------------------------------- RoofRect

RoofComponent::RoofRect::RoofRect(int32_t x1_, int32_t y1_, int32_t x2_, int32_t y2_) {
    if (x1_ > x2_) std::swap(x1_, x2_);
    if (y1_ > y2_) std::swap(y1_, y2_);
    x1 = x1_; y1 = y1_; x2 = x2_; y2 = y2_;
}

int32_t RoofComponent::RoofRect::GetByDir(int dir) const {
    switch (dir) {
        case 0: return x2;
        case 1: return y2;
        case 2: return x1;
        case 3: return y1;
    }
    return 0;
}

void RoofComponent::RoofRect::SetByDir(int dir, int32_t value) {
    switch (dir) {
        case 0: x2 = value; break;
        case 1: y2 = value; break;
        case 2: x1 = value; break;
        case 3: y1 = value; break;
    }
}

size_t RoofComponent::RectCount() const {
    size_t n = 0;
    for (auto& l : RoofRects) n += l.size();
    return n;
}

// ---------------------------------------------------------------- generation

void RoofComponent::RegenRoof() {
    if (blueprint == nullptr) return;
    PrepTexture();
    RoofRects.assign((size_t)std::max(0, blueprint->Stories - 1), std::vector<RoofRect>());
    Meshes.assign(RoofRects.size(), std::vector<RoofQuad>());
    for (int i = 1; i < blueprint->Stories; i++) RegenRoof((int8_t)(i + 1));
}

void RoofComponent::RemeshRoof() {
    if (blueprint == nullptr) return;
    PrepTexture();
    for (int i = 1; i < blueprint->Stories; i++) MeshRects(i + 1);
}

void RoofComponent::RegenRoof(int8_t level) {
    //algorithm overview:
    // 1. divide each tile into 4.
    // 2. find tile that IsRoofable
    // 3. place a starting rectangle on the tile
    // 4. Expand the rectangle along x and y axis one by one
    //    (for current axis)
    //    4a. if the expansion would be entirely contained by an existing rectangle (other axis), expand to the end of that rectangle.
    //    4b. if any tiles we're expanding into are not roofable, stop expanding this rectangle and add new ones that start from the same baseline (current axis), but split into the regions (other axis)
    //    4c. if all tiles are not roofable, rectangle is complete.
    auto width = blueprint->Width * 2;
    auto height = blueprint->Height * 2;
    std::vector<bool> evaluated((size_t)(width * height), false);
    std::vector<RoofRect> result;
    for (int y = 2; y < height; y++) {
        for (int x = 2; x < width; x++) {
            auto off = x + y * width;
            if (!evaluated[(size_t)off]) {
                evaluated[(size_t)off] = true;
                LotTilePos tilePos((int16_t)(x * 8), (int16_t)(y * 8), level);
                if (IsRoofable(tilePos)) {
                    //bingo. try expand a rectangle here.
                    RoofSpread(tilePos, evaluated, width, height, level, result);
                }
            }
        }
    }
    RoofRects[(size_t)(level - 2)] = result;
    MeshRects(level);
}

static const st::Point advanceByDir[4] = { st::Point(8, 0), st::Point(0, 8), st::Point(-8, 0), st::Point(0, -8) };
static const int ExpandOrder[4] = { 2, 3, 1, -1 };

static st::Point StartLocation(const RoofComponent::RoofRect& rect, int dir) {
    switch (dir) {
        case 0: return st::Point(rect.x2, rect.y1);
        case 1: return st::Point(rect.x2 - 8, rect.y2);
        case 2: return st::Point(rect.x1 - 8, rect.y2 - 8);
        case 3: return st::Point(rect.x1, rect.y1 - 8);
    }
    return st::Point();
}

static bool RangeCheck(const RoofComponent::RoofRect& me, const RoofComponent::RoofRect& into, int dir) {
    switch (dir % 2) {
        case 0: return (me.y1 > into.y1 && me.y2 < into.y2);
        case 1: return (me.x1 > into.x1 && me.x2 < into.x2);
    }
    return false;
}

void RoofComponent::RoofSpread(const LotTilePos& start, std::vector<bool>& evaluated, int width, int height, int8_t level,
                               std::vector<RoofRect>& result) {
    (void)height;
    RoofRect rect(start.x, start.y, start.x + 8, start.y + 8);
    st::Point toCtr(4, 4);

    while (rect.ExpandDir != -1) {
        //still have to expand in a direction
        //order: 0, 2, 1, 3, (xpos,xneg,ypos,yneg)
        auto dir = rect.ExpandDir;
        auto startPt = StartLocation(rect, dir);
        auto testPt = startPt;
        auto inc = advanceByDir[(dir + 1) % 4];
        auto count = std::abs(rect.GetByDir((dir + 1) % 4) - rect.GetByDir((dir + 3) % 4)) / 8;

        bool canExpand = true;
        for (int i = 0; i < count; i++) {
            LotTilePos tile((int16_t)testPt.X, (int16_t)testPt.Y, level);
            if (!IsRoofable(tile)) {
                canExpand = false;
                break;
            }
            testPt += inc;
        }

        if (!canExpand) rect.ExpandDir = ExpandOrder[rect.ExpandDir];
        else {
            //mark as complete - new roof rects cannot START on these tiles..
            testPt = startPt;
            for (int i = 0; i < count; i++) {
                size_t idx = (size_t)((testPt.X / 8) + (testPt.Y / 8) * width);
                if (idx < evaluated.size()) evaluated[idx] = true;
                testPt += inc;
            }

            //SPEEDUP: if expansion is within an existing roof rectangle skip to the other end of it
            auto midPt = startPt + st::Point(inc.X * count / 2, inc.Y * count / 2) + toCtr;
            const RoofRect* expandInto = nullptr;
            for (auto& x : result) if (x.Contains(midPt) && RangeCheck(rect, x, dir)) { expandInto = &x; break; }
            if (expandInto != nullptr) {
                rect.SetByDir(dir, expandInto->GetByDir(dir));
            } else {
                //on no detection, expand by 1
                rect.SetByDir(dir, rect.GetByDir(dir) + ((dir > 1) ? -8 : 8));
            }
        }
    }
    result.push_back(rect);
}

bool RoofComponent::TileIndoors(int x, int y, int level) const {
    if (level < 1 || (size_t)(level - 1) >= blueprint->RoomMap.size()) return false;
    auto map = blueprint->RoomMap[(size_t)(level - 1)];
    if (map == nullptr || x < 0 || y < 0 || x >= blueprint->Width || y >= blueprint->Height) return false;
    auto idx = (size_t)(x + y * blueprint->Width);
    if (idx >= map->size()) return false;
    auto room = (*map)[idx];
    auto room1 = room & 0xFFFF;
    auto room2 = room >> 16;
    auto& rooms = blueprint->Rooms;
    if (room1 < rooms.size() && !rooms[room1].IsOutside) return true;
    if (room2 > 0 && room2 < rooms.size() && !rooms[room2].IsOutside) return true;
    return false;
}

bool RoofComponent::IndoorsOrFloor(int x, int y, int level) const {
    if (TileIndoors(x, y, level)) return true;
    if (blueprint->Floors == nullptr || (size_t)(level - 1) >= blueprint->Floors->size()) return false;
    if (x < 0 || y < 0 || x >= blueprint->Width || y >= blueprint->Height) return false;
    return blueprint->GetFloor((int16_t)x, (int16_t)y, (int8_t)level).Pattern != 0;
}

bool RoofComponent::IsRoofable(const LotTilePos& pos) const {
    if (pos.Level == 1) return false;
    auto tileX = pos.TileX();
    auto tileY = pos.TileY();
    auto level = pos.Level;
    if (tileX <= 0 || tileX >= blueprint->Width - 1 || tileY <= 0 || tileY >= blueprint->Height - 1) return false;
    //must be over indoors
    auto halftile = false;
    if (!TileIndoors(tileX, tileY, level - 1)) {
        //are a half tile away from indoors?
        bool found = false;
        if (pos.x % 16 == 8) {
            if (TileIndoors(tileX + 1, tileY, level - 1)) found = true;
        } else {
            if (TileIndoors(tileX - 1, tileY, level - 1)) found = true;
        }

        if (pos.y % 16 == 8) {
            if (TileIndoors(tileX, tileY + 1, level - 1)) found = true;
        } else {
            if (TileIndoors(tileX, tileY - 1, level - 1)) found = true;
        }

        if (TileIndoors(tileX + ((pos.x % 16 == 8) ? 1 : -1), tileY + ((pos.y % 16 == 8) ? 1 : -1), level - 1)) found = true;
        if (!found) return false;
        halftile = true;
    }
    //on our level, the tile must not be indoors or floored
    if (IndoorsOrFloor(tileX, tileY, level)) return false;

    if (halftile) {
        if (pos.x % 16 == 8) {
            if (IndoorsOrFloor(tileX + 1, tileY, level)) return false;
        } else {
            if (IndoorsOrFloor(tileX - 1, tileY, level)) return false;
        }

        if (pos.y % 16 == 8) {
            if (IndoorsOrFloor(tileX, tileY + 1, level)) return false;
        } else {
            if (IndoorsOrFloor(tileX, tileY - 1, level)) return false;
        }

        if (IndoorsOrFloor(tileX + ((pos.x % 16 == 8) ? 1 : -1), tileY + ((pos.y % 16 == 8) ? 1 : -1), level)) return false;
    }
    return true;
}

// C# ToWorldPos returns world units ((x/16)*3, (z*pitch/16)*3 + (level-1)*2.95*3, (y/16)*3); we keep tile units.
st::Vector3 RoofComponent::ToTilePos(int x, int y, int z, int level, float pitch) {
    return st::Vector3(x / 16.0f, y / 16.0f, (z * pitch / 16.0f) + ((level - 1) * 2.95f) + ((blueprint != nullptr) ? blueprint->GetAltitude(x / 16, y / 16) : 0.0f));
}

void RoofComponent::MeshRects(int level) {
    if ((size_t)(level - 2) >= RoofRects.size()) return;
    auto& rects = RoofRects[(size_t)(level - 2)];
    auto& mesh = Meshes[(size_t)(level - 2)];
    mesh.clear();
    mesh.reserve(rects.size() * 8);
    // texture coordinates: C# texScale (2/3, 1) on world units (3 per tile), times the 32 texel texture
    const float su = (2.0f / 3.0f) * 3.0f * 32.0f, sv = 1.0f * 3.0f * 32.0f;
    for (auto& rect : rects) {
        //determine roof height of the smallest edge. This height will be used on all edges
        auto height = std::min(rect.x2 - rect.x1, rect.y2 - rect.y1) / 2;
        //    /    \
        //   /      \
        //  /________\  Draw 4 segments like this. two segments will have the top middle section so short it will not appear.
        auto heightMod = height / 400.0f / 3.0f;
        auto pitch = Pitch;
        auto up = st::Vector3(0, 0, heightMod);
        auto tl = ToTilePos(rect.x1, rect.y1, 0, level, pitch) + up;
        auto tr = ToTilePos(rect.x2, rect.y1, 0, level, pitch) + up;
        auto bl = ToTilePos(rect.x1, rect.y2, 0, level, pitch) + up;
        auto br = ToTilePos(rect.x2, rect.y2, 0, level, pitch) + up;
        auto m_tl = ToTilePos(rect.x1 + height, rect.y1 + height, height, level, pitch) + up;
        auto m_tr = ToTilePos(rect.x2 - height, rect.y1 + height, height, level, pitch) + up;
        auto m_bl = ToTilePos(rect.x1 + height, rect.y2 - height, height, level, pitch) + up;
        auto m_br = ToTilePos(rect.x2 - height, rect.y2 - height, height, level, pitch) + up;

        // C#: eave vertices * darken(0.8), ridge vertices full colour
        const float eave = 0.8f;
        auto V = [&](const st::Vector3& p, float u, float v, float shade) { RoofVertex r; r.Tile = p; r.U = u; r.V = v; r.Shade = shade; return r; };
        RoofQuad q;
        q.Fascia = false;
        // top (-y): uv (X, -Z)
        q.Face = 0;
        q.V[0] = V(tl, tl.X * su, -tl.Y * sv, eave); q.V[1] = V(tr, tr.X * su, -tr.Y * sv, eave);
        q.V[2] = V(m_tr, m_tr.X * su, -m_tr.Y * sv, 1); q.V[3] = V(m_tl, m_tl.X * su, -m_tl.Y * sv, 1);
        mesh.push_back(q);
        // right (+x): uv (Z, X)
        q.Face = 1;
        q.V[0] = V(tr, tr.Y * su, tr.X * sv, eave); q.V[1] = V(br, br.Y * su, br.X * sv, eave);
        q.V[2] = V(m_br, m_br.Y * su, m_br.X * sv, 1); q.V[3] = V(m_tr, m_tr.Y * su, m_tr.X * sv, 1);
        mesh.push_back(q);
        // bottom (+y): uv (X, Z)
        q.Face = 2;
        q.V[0] = V(br, br.X * su, br.Y * sv, eave); q.V[1] = V(bl, bl.X * su, bl.Y * sv, eave);
        q.V[2] = V(m_bl, m_bl.X * su, m_bl.Y * sv, 1); q.V[3] = V(m_br, m_br.X * su, m_br.Y * sv, 1);
        mesh.push_back(q);
        // left (-x): uv (Z, -X)
        q.Face = 3;
        q.V[0] = V(bl, bl.Y * su, -bl.X * sv, eave); q.V[1] = V(tl, tl.Y * su, -tl.X * sv, eave);
        q.V[2] = V(m_tl, m_tl.Y * su, -m_tl.X * sv, 1); q.V[3] = V(m_bl, m_bl.Y * su, -m_bl.X * sv, 1);
        mesh.push_back(q);

        // FSO_CPP (FreeSO advanced roof "thickness"): a fascia board under each eave, 0.5 world units deep.
        auto th = st::Vector3(0, 0, -0.5f / 3.0f);
        const st::Vector3* edges[4][2] = { { &tl, &tr }, { &tr, &br }, { &br, &bl }, { &bl, &tl } };
        for (int f = 0; f < 4; f++) {
            RoofQuad e;
            e.Fascia = true;
            e.Face = f;
            e.V[0] = V(*edges[f][0], 0, 0, 1); e.V[1] = V(*edges[f][1], 0, 0, 1);
            e.V[2] = V(*edges[f][1] + th, 0, 0, 0.8f); e.V[3] = V(*edges[f][0] + th, 0, 0, 0.8f);
            mesh.push_back(e);
        }
    }
}

void RoofComponent::PrepTexture() {
    auto roofs = FSO::Content::Content::Get()->WorldRoofs.get();
    std::string path = (roofs != nullptr) ? roofs->GetPath((int32_t)Style) : std::string();
    if (path == LoadedTexture && Mips[0].Valid()) return;
    LoadedTexture = path;
    for (auto& m : Mips) m = RasterTexture();
    std::vector<uint8_t> data;
    FSO::Vitaboy::Texture bmp;
    if (!path.empty() && FSO::Content::ReadAllBytes(path, data) && bmp.LoadBMP(data) && bmp.Width > 0 && bmp.Height > 0) {
        // resample to 32x32 (all TS1 roofs are 32x32 already)
        RasterTexture t;
        t.Width = 32; t.Height = 32;
        t.Pixels.resize(32 * 32);
        uint64_t sr = 0, sg = 0, sb = 0;
        for (int y = 0; y < 32; y++) for (int x = 0; x < 32; x++) {
            uint32_t c = bmp.Fetch(x * bmp.Width / 32, y * bmp.Height / 32) | 0xFF000000u;
            t.Pixels[(size_t)(y * 32 + x)] = c;
            sr += c & 0xFF; sg += (c >> 8) & 0xFF; sb += (c >> 16) & 0xFF;
        }
        t.Average = Framebuffer::Pack((uint8_t)(sr / 1024), (uint8_t)(sg / 1024), (uint8_t)(sb / 1024), 255);
        Mips[0] = t;
        Mips[1] = Mips[0].Half();
        Mips[2] = Mips[1].Half();
    } else {
        // no roof textures installed: plain grey-brown shingles
        RasterTexture t;
        t.Width = 2; t.Height = 2;
        t.Pixels.assign(4, Framebuffer::Pack(110, 90, 80, 255));
        t.Average = t.Pixels[0];
        Mips[0] = Mips[1] = Mips[2] = t;
    }
}

// ---------------------------------------------------------------- drawing

void RoofComponent::DrawInto(WorldState& world, Framebuffer& target) {
    LastStats = RasterStats();
    if (blueprint == nullptr) return;
    if (ShapeDirty) {
        RegenRoof();
        ShapeDirty = false;
        StyleDirty = false;
    } else if (StyleDirty) {
        RemeshRoof();
        StyleDirty = false;
    }

    auto& ws = world.WorldSpace;
    // A tile-space point lands where the 2D floor sprites put it: the top vertex of the floor diamond of tile (x,y)
    // is GetScreenFromTile(x,y) + (CadgeWidth/2, CadgeBaseLine - TilePxHeight/2); depending on the rotation that
    // vertex is the tile corner (x,y), (x,y+1), (x+1,y+1) or (x+1,y).
    auto K = TileProjectionOrigin(world);
    auto cam = world.Camera();

    int mip = (world.Zoom() == WorldZoom::Far) ? 1 : 0;
    float texDiv = (float)(1 << mip);
    const RasterTexture* tex = &Mips[mip];

    // TS1 roofs are lit by the outside light (C#: DiffuseColor = world.OutsideColor)
    st::Color out = world.OutsideColor;
    if (out.A == 0 || !target.Room.empty()) out = st::Color::White(); // deferred: lit when composited
    float lr = out.R / 255.0f, lg = out.G / 255.0f, lb = out.B / 255.0f;

    // FSO_CPP: shading relative to the view. Face normals in tile space -> screen direction.
    static const st::Vector3 normals[4] = { st::Vector3(0, -1, 0), st::Vector3(1, 0, 0), st::Vector3(0, 1, 0), st::Vector3(-1, 0, 0) };
    float faceShade[4];
    float slope = std::min(1.0f, Pitch / 0.66f);
    for (int f = 0; f < 4; f++) {
        auto s = ws.GetScreenFromTile(normals[f]);
        bool front = s.Y > 0, left = s.X < 0;
        float base = front ? (left ? 1.0f : 0.80f) : (left ? 0.90f : 0.70f);
        faceShade[f] = 1.0f - (1.0f - base) * (0.35f + 0.65f * slope);
    }

    for (size_t li = 0; li < Meshes.size(); li++) {
        int level = (int)li + 2;
        if (level - 1 > world.Level()) break; // roofs over the stories that are shown
        for (auto& q : Meshes[li]) {
            RasterVertex rv[4];
            for (int i = 0; i < 4; i++) {
                auto& v = q.V[i];
                auto s = ws.GetScreenFromTile(v.Tile);
                rv[i].x = s.X + K.X;
                rv[i].y = s.Y + K.Y;
                rv[i].z = cam->Depth(WorldSpace::GetWorldFromTile(v.Tile));
                rv[i].u = v.U / texDiv;
                rv[i].v = v.V / texDiv;
                float sh = v.Shade * (q.Fascia ? faceShade[q.Face] * 0.75f : faceShade[q.Face]);
                rv[i].r = sh * lr; rv[i].g = sh * lg; rv[i].b = sh * lb;
            }
            const RasterTexture* t = q.Fascia ? nullptr : tex;
            uint32_t flat = q.Fascia ? tex->Average : 0;
            // C# index order: (2,1,0) (0,3,2)
            DrawTriangle(target, rv[2], rv[1], rv[0], t, flat, -0.05f, &LastStats);
            DrawTriangle(target, rv[0], rv[3], rv[2], t, flat, -0.05f, &LastStats);
        }
    }
}

}}}
