/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.LotView.Components.RoofComponent (World/components/RoofComponent.cs)
//
// TS1 roofs are generated, not built: every level-N tile that is above an indoor room of level N-1 (plus the
// half-tile eave around it) and has no room/floor of its own is "roofable"; the roofable area is split into
// rectangles (RoofSpread) and each rectangle becomes a hip roof (4 slopes meeting at a ridge) of the lot's pitch,
// textured with GameData/Roofs/<HOUS.RoofName>.bmp. This is a translation of the FreeSims algorithm (RegenRoof /
// RoofSpread / IsRoofable / MeshRects) with the 3D draw (GrassEffect "DrawBase" on the GPU) replaced by the
// software TriangleRaster writing world depth into the static wall layer (World2D), like the original roofs that
// are part of the cached house image.
// Shown like TS1: only in the "Roof" view (WorldState::DrawRoofs, control panel button above "Second Story") and in
// lot thumbnails; never with walls down/cutaway or while a story is selected.
// Differences (marked FSO_CPP): face shading is relative to the camera rotation (TS1's sprites are lit from the
// viewer's side in every rotation; FreeSims lit fixed world sides), eaves get a thin fascia (FreeSO "advanced roof"
// rim) in the roof's average colour, and the texture is mip-mapped per zoom level.
#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include "core/math.h"
#include "world/components/WorldComponent.h"
#include "world/model/LotTilePos.h"
#include "world/utils/TriangleRaster.h"

namespace FSO { namespace LotView {
class Framebuffer;
namespace Model { class Blueprint; }
namespace Components {

class RoofComponent : public WorldComponent {
public:
    FSO::LotView::Model::Blueprint* blueprint = nullptr;

    uint32_t Style = 16;   // C# RoofStyle (WorldRoofProvider id)
    float Pitch = 0.66f;   // C# RoofPitch (TS1 HOUS: 0.4 / 0.6 / 1.0, 0 = flat)
    bool StyleDirty = false;
    bool ShapeDirty = true;

    void SetStylePitch(uint32_t style, float pitch);

    class RoofRect {
    public:
        int32_t ExpandDir = 0;
        int32_t x1, x2, y1, y2;
        RoofRect(int32_t x1_, int32_t y1_, int32_t x2_, int32_t y2_);
        int32_t GetByDir(int dir) const;
        void SetByDir(int dir, int32_t value);
        bool Contains(const st::Point& pt) const { return (pt.X >= x1 && pt.X <= x2) && (pt.Y >= y1 && pt.Y <= y2); }
    };

    /// Regenerates the rectangles of every roof level (C# RegenRoof(device)).
    void RegenRoof();
    void RegenRoof(int8_t level);
    /// Rebuilds the meshes (C# RemeshRoof / MeshRects) and reloads the texture.
    void RemeshRoof();

    bool TileIndoors(int x, int y, int level) const;
    bool IndoorsOrFloor(int x, int y, int level) const;
    bool IsRoofable(const FSO::LotView::Model::LotTilePos& pos) const;

    /// Draws the roofs of the levels up to world.Level()+1 into target (with its scroll = world._2D->GetScroll()).
    void DrawInto(WorldState& world, Framebuffer& target);
    void Draw(WorldState& world) override { (void)world; } // drawn by World2D through DrawInto

    float PreferredDrawOrder() override { return 0.0f; }
    /// Statistics of the last DrawInto.
    Utils::RasterStats LastStats;
    size_t RectCount() const;

private:
    struct RoofVertex { st::Vector3 Tile; float U, V; float Shade; };
    struct RoofQuad { RoofVertex V[4]; int Face; bool Fascia; };
    std::vector<std::vector<RoofRect>> RoofRects; // index level-2
    std::vector<std::vector<RoofQuad>> Meshes;    // index level-2
    Utils::RasterTexture Mips[3];                 // 32x32, 16x16, 8x8
    std::string LoadedTexture;

    void MeshRects(int level);
    void PrepTexture();
    void RoofSpread(const FSO::LotView::Model::LotTilePos& start, std::vector<bool>& evaluated, int width, int height,
                    int8_t level, std::vector<RoofRect>& result);
    st::Vector3 ToTilePos(int x, int y, int z, int level, float pitch); // TS1(sims): + altitude (FreeSO), no longer static
};

}}}
