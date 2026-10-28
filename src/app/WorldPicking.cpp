/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims app — see WorldPicking.h
#include "app/WorldPicking.h"
#include "world/World.h"
#include "world/model/Blueprint.h"
#include "world/components/ObjectComponent.h"
#include "world/components/AvatarComponent.h"
#include "vitaboy/SimAvatar.h"
#include <cmath>
#include <limits>
#include <vector>
#include <algorithm>

namespace simapp {

using namespace FSO::LotView;

FSO::Vitaboy::WorldCamera MakeAvatarCamera(World& world) {
    // identical to World::DrawAvatarComponents (src/world/World.cpp)
    auto& State = world.State;
    auto& ws = State.WorldSpace;
    FSO::Vitaboy::WorldCamera cam;
    cam.Zoom = (FSO::Vitaboy::WorldZoom)(int)State.Zoom();
    cam.Rotation = State.Rotation();
    cam.CenterTile = State.CenterTile();
    cam.ViewDimensions = st::Vector2(ws.WorldPxWidth, ws.WorldPxHeight);
    cam.PreciseZoom = 1.0f;
    auto so = ws.GetScreenOffset();
    auto c = ws.GetScreenFromTile(State.CenterTile());
    st::Vector2 unrounded(-c.X + ws.WorldPxWidth / 2.0f - ws.CadgeWidth / 2.0f, -c.Y + ws.WorldPxHeight / 2.0f - ws.CadgeBaseLine);
    switch (State.Rotation()) {
        case WorldRotation::TopLeft: unrounded.Y += ws.TilePxHeightHalf; break;
        case WorldRotation::TopRight: unrounded.X -= ws.TilePxWidthHalf; break;
        case WorldRotation::BottomRight: unrounded.Y -= ws.TilePxHeightHalf; break;
        case WorldRotation::BottomLeft: unrounded.X += ws.TilePxWidthHalf; break;
    }
    cam.PixelOffset = st::Vector2(so.X - unrounded.X + 1.0f, so.Y - unrounded.Y);
    return cam;
}

st::Vector2 TileToScreen(World& world, const st::Vector3& tile) {
    auto cam = MakeAvatarCamera(world);
    auto p = cam.ProjectTile(tile);
    return st::Vector2(p.X, p.Y);
}

PickResult PickObjectAt(World& world, const Framebuffer* lastFrame, int x, int y, int radius) {
    PickResult best;
    auto bp = world.GetBlueprint();
    if (bp == nullptr) return best;
    auto& state = world.State;
    auto batch = state._2D;
    const int n = 2 * radius + 1;
    const float INF = std::numeric_limits<float>::infinity();
    const float EPS = 0.75f; // world units (a tile is 3)

    auto frameDepth = [&](int px, int py) -> float {
        if (lastFrame == nullptr || px < 0 || py < 0 || px >= lastFrame->Width || py >= lastFrame->Height) return INF;
        return lastFrame->Depth[(size_t)py * lastFrame->Width + px];
    };
    int bestDist = 1 << 30;
    float bestDepth = INF;
    auto consider = [&](int16_t id, bool avatar, const float* depth) {
        // depth: n*n world depths (INF = not covered)
        int nearDist = 1 << 30; float nearDepth = INF;
        for (int yy = 0; yy < n; yy++) for (int xx = 0; xx < n; xx++) {
            float d = depth[yy * n + xx];
            if (!(d < INF)) continue;
            int px = x - radius + xx, py = y - radius + yy;
            if (d > frameDepth(px, py) + EPS) continue; // hidden behind something drawn in the frame
            int dist = std::max(std::abs(xx - radius), std::abs(yy - radius));
            if (dist < nearDist || (dist == nearDist && d < nearDepth)) { nearDist = dist; nearDepth = d; }
        }
        if (nearDist == (1 << 30)) return;
        if (nearDist < bestDist || (nearDist == bestDist && nearDepth < bestDepth)) {
            bestDist = nearDist; bestDepth = nearDepth;
            best.ObjectID = id; best.IsAvatar = avatar; best.Depth = nearDepth; best.Distance = nearDist;
        }
    };

    // ---- objects: OBJID pass through the 2D batch ----
    Framebuffer pick(n, n);
    auto oldTarget = batch->Target;
    auto oldScroll = batch->GetScroll();
    auto pxOffset = state.WorldSpace.GetScreenOffset();
    pxOffset = st::Vector2(-pxOffset.X, -pxOffset.Y);
    batch->Target = &pick;
    batch->SetScroll(st::Vector2(pxOffset.X + (float)(x - radius), pxOffset.Y + (float)(y - radius)));
    for (auto obj : bp->Objects) {
        if (obj == nullptr || obj->Level > state.Level() || !obj->Visible || obj->DGRP == nullptr) continue;
        if (obj->ObjectID <= 0) continue;
        auto tilePosition = obj->DrawPosition();
        if (tilePosition.X < -2043 && tilePosition.Y < -2043) continue; // out of world
        auto oPx = state.WorldSpace.GetScreenFromTile(tilePosition);
        obj->ValidateSprite(state);
        auto b = obj->Bounding();
        // quick reject on the sprite bounds (world pixels)
        float wx = pxOffset.X + x, wy = pxOffset.Y + y;
        if (b.Width > 0 && b.Height > 0) {
            float bx0 = b.X + oPx.X - radius, by0 = b.Y + oPx.Y - radius;
            if (wx < bx0 || wy < by0 || wx > bx0 + b.Width + 2 * radius || wy > by0 + b.Height + 2 * radius) continue;
        }
        pick.Clear(0);
        batch->Begin(state.Camera());
        batch->OffsetPixel(oPx);
        batch->OffsetTile(tilePosition);
        batch->SetObjID(obj->ObjectID);
        obj->Draw(state);
        batch->End();
        consider(obj->ObjectID, false, pick.Depth.data());
    }
    batch->Target = oldTarget;
    batch->SetScroll(oldScroll);

    // ---- avatars: Vitaboy rasterizer into the same window ----
    if (!bp->Avatars.empty()) {
        auto cam = MakeAvatarCamera(world);
        cam.PixelOffset = st::Vector2(cam.PixelOffset.X - (float)(x - radius), cam.PixelOffset.Y - (float)(y - radius));
        // NDC depth <-> world depth (affine, as World::DrawAvatarComponents)
        st::Vector3 p0(0, 0, 0), p1(30, 7, 50);
        float d0 = state.Camera()->Depth(p0), d1 = state.Camera()->Depth(p1);
        float n0 = cam.DepthOfWorld(p0), n1 = cam.DepthOfWorld(p1);
        float a = (n1 - n0) / (d1 - d0), bb = n0 - a * d0;
        std::vector<uint8_t> color((size_t)n * n * 4, 0);
        std::vector<float> z((size_t)n * n, 1.0f), wd((size_t)n * n);
        for (auto ava : bp->Avatars) {
            if (ava == nullptr || ava->Avatar == nullptr || !ava->Visible) continue;
            auto pos = ava->DrawPosition();
            if (pos.X < -2043 && pos.Y < -2043) continue;
            if (ava->Level > state.Level()) continue;
            std::fill(z.begin(), z.end(), 1.0f);
            FSO::Vitaboy::Framebuffer fb(n, n, color.data());
            FSO::Vitaboy::ZBuffer zb(n, n, z.data());
            FSO::Vitaboy::AvatarDrawOptions opt;
            opt.Cull = FSO::Vitaboy::AvatarCullMode::None; // thin limbs: accept either side
            FSO::Vitaboy::DrawAvatar(fb, zb, cam, *ava->Avatar, pos, ava->RadianDirection, opt);
            for (int i = 0; i < n * n; i++) wd[(size_t)i] = (z[(size_t)i] >= 1.0f) ? INF : (z[(size_t)i] - bb) / a;
            consider(ava->ObjectID, true, wd.data());
        }
    }
    return best;
}

}
