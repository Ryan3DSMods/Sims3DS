/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.LotView.Utils.DGRPRenderer (World/utils/DGRPRenderer.cs)
#include "world/utils/DGRPRenderer.h"
#include "world/utils/_2DWorldBatch.h"
#include "world/WorldState.h"
#include "files/formats/iff/IffFile.h"
#include "files/formats/iff/chunks/DGRP.h"
#include "files/formats/iff/chunks/SPR2.h"
#include <climits>
#include <cmath>

namespace FSO { namespace LotView { namespace Utils {

using namespace FSO::Files::Formats::IFF::Chunks;
using FSO::LotView::Model::Direction;

float DGRPRenderer::RadianDirection() const {
    switch (_Direction) {
        case Direction::NORTH: return 0;
        case Direction::EAST: return (float)M_PI / 2;
        case Direction::SOUTH: return (float)M_PI;
        case Direction::WEST: return (float)M_PI * 1.5f;
        default: return 0;
    }
}

void DGRPRenderer::ValidateSprite(WorldState& world) {
    if (DrawGroup == nullptr) return;
    if (_Dirty) {
        if (_TextureDirty) {
            Items.clear();
            auto direction = (uint32_t)_Direction;

            /** Compute the direction **/
            auto image = DrawGroup->GetImage(direction, (uint32_t)world.Zoom(), (uint32_t)world.Rotation());
            if (image != nullptr) {
                for (auto& sprite : image->Sprites) {
                    // C#: world._2D.GetWorldTexture(sprite) -> DGRPSprite.GetWorldTexture, which looks the SPR2 up in
                    // the DGRP's own iff (an SPR# yields no z-buffer and is skipped below, as in C#).
                    SPR2* spr2 = (DrawGroup->ChunkParent != nullptr) ? DrawGroup->ChunkParent->Get<SPR2>((uint16_t)sprite.SpriteID) : nullptr;
                    if (spr2 == nullptr || sprite.SpriteFrameIndex >= spr2->Frames.size()) continue;
                    auto texture = world._2D->GetWorldTexture(spr2->Frames[sprite.SpriteFrameIndex].get());
                    if (texture.Pixel == nullptr || texture.ZBuffer == nullptr) { continue; }

                    auto isDynamic = sprite.SpriteID >= DynamicSpriteBaseID && sprite.SpriteID < (uint32_t)(DynamicSpriteBaseID + NumDynamicSprites);
                    if (isDynamic) {
                        auto dynamicIndex = (uint16_t)(sprite.SpriteID - DynamicSpriteBaseID);

                        auto isVisible = (dynamicIndex > 63) ? ((DynamicSpriteFlags2 & ((uint64_t)0x1 << (dynamicIndex - 64))) > 0) :
                            ((DynamicSpriteFlags & ((uint64_t)0x1 << dynamicIndex)) > 0);
                        if (!isVisible) {
                            continue;
                        }
                    }
                    DGRPRendererItem entry; //do not use sprite pool for DGRP, since we can reliably remember our own sprites.
                    auto& item = entry.Sprite;
                    item.Pixel = texture.Pixel;
                    item.Depth = texture.ZBuffer;
                    if (texture.ZBuffer != nullptr) {
                        item.RenderMode = _2DBatchRenderMode::Z_BUFFER;
                        item.WorldPosition = st::Vector3(sprite.ObjectOffset.X, sprite.ObjectOffset.Y, sprite.ObjectOffset.Z);
                    } else {
                        item.RenderMode = _2DBatchRenderMode::NO_DEPTH;
                    }

                    item.SrcRect = st::Rectangle(0, 0, item.Pixel->Width, item.Pixel->Height);
                    item.DestRect = st::Rectangle(0, 0, item.Pixel->Width, item.Pixel->Height);
                    item.FlipHorizontally = sprite.Flip();

                    entry.DGRPSprite = &sprite;
                    Items.push_back(entry);
                }
            }

            _TextureDirty = false;
        }

        int maxX = INT_MIN, maxY = INT_MIN;
        int minX = INT_MAX, minY = INT_MAX;
        float rad = RadianDirection();
        float c = std::cos(rad), s = std::sin(rad);
        for (auto& entry : Items) {
            auto& sprite = entry.Sprite;
            auto dgrpSprite = entry.DGRPSprite;

            auto pxX = (world.WorldSpace.CadgeWidth / 2.0f) + dgrpSprite->SpriteOffset.X;
            auto pxY = (world.WorldSpace.CadgeBaseLine - sprite.Pixel->Height) + dgrpSprite->SpriteOffset.Y;

            st::Vector3 centerRelative(dgrpSprite->ObjectOffset.X * (1.0f / 16.0f), dgrpSprite->ObjectOffset.Y * (1.0f / 16.0f), dgrpSprite->ObjectOffset.Z * (1.0f / 5.0f));
            // Vector3.Transform(centerRelative, Matrix.CreateRotationZ(RadianDirection)) (row vector)
            centerRelative = st::Vector3(centerRelative.X * c - centerRelative.Y * s, centerRelative.X * s + centerRelative.Y * c, centerRelative.Z);

            auto pxOff = world.WorldSpace.GetScreenFromTile(centerRelative);

            sprite.DestRect.X = (int32_t)(pxX + pxOff.X);
            sprite.DestRect.Y = (int32_t)(pxY + pxOff.Y);

            if (sprite.DestRect.X < minX) minX = sprite.DestRect.X;
            if (sprite.DestRect.Y < minY) minY = sprite.DestRect.Y;
            if (sprite.DestRect.X + sprite.Pixel->Width > maxX) maxX = sprite.DestRect.X + sprite.Pixel->Width;
            if (sprite.DestRect.Y + sprite.Pixel->Height > maxY) maxY = sprite.DestRect.Y + sprite.Pixel->Height;

            // NOTE: faithful to C#: tile-space (x, y, z) * 3 is used directly as a world position, i.e. the tile y
            // offset lands in world Y (height) and z in world Z (depth axis) - unlike WorldSpace.GetWorldFromTile.
            sprite.WorldPosition = centerRelative * 3.0f;
            sprite.Room = ((dgrpSprite->Flags & DGRPSpriteFlags::Flip) > 0 && Room != 65534 && Room != 65533) ? (uint16_t)65535 : Room;
        }
        if (Items.empty()) Bounding = st::Rectangle();
        else Bounding = st::Rectangle(minX, minY, maxX - minX, maxY - minY);

        _Dirty = false;
    }
}

void DGRPRenderer::Draw(WorldState& world) {
    ValidateSprite(world);
    for (auto& item : Items) {
        world._2D->Draw(&item.Sprite);
    }
}

}}}
