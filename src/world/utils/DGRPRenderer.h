/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.LotView.Utils.DGRPRenderer (World/utils/DGRPRenderer.cs)
// Used for rendering DGRPs in the world.
#pragma once
#include <cstdint>
#include <vector>
#include "core/math.h"
#include "world/model/Direction.h"
#include "world/utils/_2DSprite.h"

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks { class DGRP; class DGRPSprite; } } } } }

namespace FSO { namespace LotView {
class WorldState;
namespace Utils {

class DGRPRendererItem {
public:
    _2DSprite Sprite;
    FSO::Files::Formats::IFF::Chunks::DGRPSprite* DGRPSprite = nullptr;
};

class DGRPRenderer {
public:
    explicit DGRPRenderer(FSO::Files::Formats::IFF::Chunks::DGRP* group) : DrawGroup(group) {}

    st::Rectangle Bounding;
    uint64_t DynamicSpriteFlags = 0x00000000;
    uint64_t DynamicSpriteFlags2 = 0x00000000;
    uint16_t DynamicSpriteBaseID = 0;
    uint16_t NumDynamicSprites = 0;
    uint16_t Room = 0;

    /// Gets or sets the DGRP used by this DGRPRenderer instance.
    FSO::Files::Formats::IFF::Chunks::DGRP* DGRP() const { return DrawGroup; }
    void DGRP(FSO::Files::Formats::IFF::Chunks::DGRP* value) { DrawGroup = value; _TextureDirty = true; _Dirty = true; }

    /// Gets or sets the direction that the DGRP is facing.
    FSO::LotView::Model::Direction Direction() const { return _Direction; }
    void Direction(FSO::LotView::Model::Direction value) { _Direction = value; _TextureDirty = true; _Dirty = true; }

    void InvalidateRotation() { _TextureDirty = true; _Dirty = true; }
    void InvalidateZoom() { _TextureDirty = true; _Dirty = true; }
    void InvalidateScroll() { /*_Dirty = true;*/ }
    // FSO_CPP: the C# property setters for the dynamic sprite flags / room only change state; sprites are
    // rebuilt from them in ValidateSprite, so callers that change them must invalidate.
    void InvalidateTextures() { _TextureDirty = true; _Dirty = true; }

    void ValidateSprite(WorldState& world);
    void Draw(WorldState& world);

private:
    FSO::Files::Formats::IFF::Chunks::DGRP* DrawGroup;
    std::vector<DGRPRendererItem> Items;
    bool _Dirty = true;
    bool _TextureDirty = true;
    FSO::LotView::Model::Direction _Direction = FSO::LotView::Model::Direction::NORTH;

    float RadianDirection() const;
};

}}}
