/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims app — screen -> object picking for the software world renderer.
//
// FreeSims: World.GetObjectIDAtScreenPos(x, y, gd) renders the lot once more in "OBJID mode" (every sprite's
// pixels write its ObjectID instead of its colour, depth tested) into a 1x1 target and reads the pixel back;
// avatars are drawn in the same pass with their ObjectID. The C++ renderer stubbed that out (src/world/STUBS.md),
// so this is the CPU equivalent, built on the world's own rasterizer without touching src/world:
//   * for every object component on a visible level, its DGRP sprites are re-rasterized (same _2DWorldBatch,
//     same depth function) into a tiny (2r+1)^2 target centred on the tap point;
//   * every avatar is re-rasterized by the Vitaboy renderer into the same small window (camera shifted);
//   * the hit is the entity nearest to the camera at the tap pixel; it must not be hidden behind what the last
//     frame drew there (walls, other objects), i.e. its depth is <= the frame's depth (+ epsilon). If nothing
//     covers the exact pixel, the nearest covered pixel within r wins (fat-finger tolerance for the touch view).
// Cost: only on a tap. ~100-200 objects x their sprites clipped to a 7x7 window.
#pragma once
#include <cstdint>
#include "core/math.h"
#include "vitaboy/AvatarRenderer.h"

namespace FSO { namespace LotView { class World; class Framebuffer; } }

namespace simapp {

struct PickResult {
    int16_t ObjectID = 0;     // 0 = nothing
    bool IsAvatar = false;
    float Depth = 0;          // world depth (smaller = nearer)
    int Distance = 0;         // pixels from the tap point to the nearest covered pixel
};

/// The Vitaboy camera World::DrawAvatarComponents uses for a view of the world's current size.
FSO::Vitaboy::WorldCamera MakeAvatarCamera(FSO::LotView::World& world);

/// Screen pixel of a tile-space point (x, y in tiles, z = height in tiles), matching what World::Draw paints.
st::Vector2 TileToScreen(FSO::LotView::World& world, const st::Vector3& tile);

/// Picks the entity at view pixel (x, y). `lastFrame` (optional) is the framebuffer World::Draw last painted
/// (its depth buffer is used for occlusion by walls/floors/other objects).
PickResult PickObjectAt(FSO::LotView::World& world, const FSO::LotView::Framebuffer* lastFrame, int x, int y, int radius = 3);

}
