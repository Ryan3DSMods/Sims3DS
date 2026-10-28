/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.LotView.Components.FloorComponent (World/components/FloorComponent.cs)
#pragma once
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <memory>
#include "core/math.h"
#include "world/components/WorldComponent.h"
#include "world/utils/_2DSprite.h"

namespace FSO { namespace Content { class Floor; } }
namespace FSO { namespace LotView { namespace Model { class Blueprint; } } }

namespace FSO { namespace LotView { namespace Components {

enum class PoolSegments : int {
    TopRight = 1,
    Right = 2,
    BottomRight = 4,
    Bottom = 8,
    BottomLeft = 16,
    Left = 32,
    TopLeft = 64,
    Top = 128
};

class FloorComponent : public WorldComponent {
public:
    FSO::LotView::Model::Blueprint* blueprint = nullptr;
    bool HasDrawBound = false;          // C# Rectangle? DrawBound
    st::Rectangle DrawBound;

    float PreferredDrawOrder() override { return 801.0f; }
    void Draw(WorldState& world) override;

    static PoolSegments RotatePoolSegs(PoolSegments ps, int rotate);

private:
    std::vector<Utils::Texture2D>* ArchZBuffers = nullptr;
    /// TS1 water tiles (floors.iff SPR2 0x800-0x80F) mark the area outside the water diamond with the palette's
    /// transparent colour but draw it with the "solid" pixel command; keyed copies with that colour cleared.
    struct KeyedTexture { std::vector<st::Color> Data; Utils::Texture2D Tex; };
    std::unordered_map<const void*, std::unique_ptr<KeyedTexture>> WaterTextures;
    Utils::Texture2D* GetWaterTexture(void* frame);
    static const st::Rectangle FLOORDEST_NEAR, FLOORDEST_MED, FLOORDEST_FAR;
    static const st::Point PoolDirections[8];

    Utils::_2DSprite* GetAirSprite(WorldState& world);
    Utils::_2DSprite* GetFloorSprite(FSO::Content::Floor* pattern, int rotation, WorldState& world);
};

}}}
