/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.LotView.Components.WallComponent (World/components/WallComponent.cs)
// "a mega fun component that draws all walls for you!!"
#pragma once
#include <cstdint>
#include <vector>
#include <unordered_map>
#include "core/math.h"
#include "world/components/WorldComponent.h"
#include "world/model/WallTile.h"
#include "world/model/WorldRotation.h"
#include "world/utils/_2DSprite.h"

namespace FSO { namespace Content { class Wall; class WallStyle; class Floor; } }
namespace FSO { namespace LotView { namespace Model { class Blueprint; } } }

namespace FSO { namespace LotView { namespace Components {

enum class WallCut : uint8_t {
    Up = 0,
    DownLeftUpRight = 1,
    DownRightUpLeft = 2,
    Down = 3
};

struct WallCuts {
    WallCut TLCut = WallCut::Up;
    WallCut TRCut = WallCut::Up;
};

enum class CutawayEdges : uint8_t {
    PositiveY = 1,
    PositiveX = 2,
    NegativeY = 4,
    NegativeX = 8,
    SpecialPositiveY = 16,
    SpecialPositiveX = 32,
    SpecialNegativeY = 64,
    SpecialNegativeX = 128
};

enum class JunctionFlags : uint8_t {
    TopLeft = 1,
    TopRight = 2,
    BottomRight = 4,
    BottomLeft = 8,
    DiagLeft = 16,
    DiagTop = 32,
    DiagRight = 64,
    DiagBottom = 128,

    AllNonDiag = 15,
    AllDiag = 240
};

class WallComponent : public WorldComponent {
public:
    float PreferredDrawOrder() override { return 800.0f; }

    FSO::LotView::Model::Blueprint* blueprint = nullptr;

    std::unordered_map<uint16_t, FSO::Content::Wall*> WallCache;
    std::unordered_map<uint16_t, FSO::Content::WallStyle*> WallStyleCache;

    void Draw(WorldState& world) override;

private:
    std::vector<uint8_t> UpJunctions;   // JunctionFlags
    std::vector<uint8_t> DownJunctions; // JunctionFlags
    std::vector<WallCuts> Cuts;
    std::vector<Utils::Texture2D>* WallZBuffers = nullptr;
    uint32_t TileRoom = 0;

    static const st::Rectangle DESTINATION_NEAR[4], DESTINATION_MED[4], DESTINATION_FAR[4];
    static const st::Rectangle JUNCDEST_NEAR, JUNCDEST_MED, JUNCDEST_FAR;
    static const st::Rectangle FLRDEST_NEAR, FLRDEST_MED, FLRDEST_FAR;
    static int8_t JunctionMap[256];
    static void InitJunctionMap();

    bool OffsetValid(const st::Vector3& off) const;
    static st::Vector3 RotateOffset(WorldRotation rot, const st::Vector3& off);
    void GenerateWallData(const std::vector<FSO::LotView::Model::WallTile>& walls, const std::vector<int32_t>& wallsAt, bool notTop);
    bool WallsDownAt(int x, int y) const;
    uint8_t GetCutEdges(int x, int y) const;
    FSO::Content::Wall* GetPattern(uint16_t id);
    FSO::Content::WallStyle* GetStyle(uint16_t id);
    Utils::_2DSprite* GetWallSprite(FSO::Content::Wall* pattern, FSO::Content::WallStyle* style, int rotation, bool down, WorldState& world);
    Utils::_2DSprite* GetFloorSprite(FSO::Content::Floor* pattern, int rotation, WorldState& world, uint8_t cut);
    static JunctionFlags RotateJunction(WorldRotation rot, JunctionFlags input);
    WallCuts RotateCuts(WorldRotation rot, WallCuts input, int16_t x, int16_t y);
    FSO::LotView::Model::WallTile RotateWall(WorldRotation rot, const FSO::LotView::Model::WallTile& input, int16_t x, int16_t y, int8_t level);
    Utils::_2DSprite* CopySprite(WorldState& world, Utils::_2DSprite* _Sprite);
};

}}}
