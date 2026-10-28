/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.LotView.World (World/World.cs)
#include "world/World.h"
#include "vitaboy/AvatarRenderer.h"
#include "vitaboy/SimAvatar.h"
#include "world/utils/TriangleRaster.h"
#include <cmath>
#include <limits>
#include <cstdio>
#include <cstdlib>
#include <algorithm>

namespace FSO { namespace LotView {

using namespace FSO::LotView::Model;

World::World(int32_t width, int32_t height) : State((float)width, (float)height, this) {
    /**
     * Setup world state, this object acts as a facade
     * to world objects as well as providing various
     * state settings for the world and helper functions
     */
    State._2D = &_2DBatch;
    HasInit = HasInitGPU && HasInitBlueprint;
}

World::~World() {}

void World::InitBlueprint(FSO::LotView::Model::Blueprint* blueprint) {
    this->Blueprint = blueprint;
    State.AltitudeBlueprint = blueprint; // TS1(sims)
    _2DWorld.Init(blueprint);

    HasInitBlueprint = true;
    HasInit = HasInitGPU && HasInitBlueprint;
    if (blueprint == nullptr) return;
    State.InvalidateWorldSize();
    // C# gets these from the WorldState setters run in World.Initialize; make sure every layer and the room
    // lights are generated for the first frame.
    blueprint->Damage.push_back(BlueprintDamage(BlueprintDamageType::ZOOM));
    blueprint->Damage.push_back(BlueprintDamage(BlueprintDamageType::LIGHTING_CHANGED));
    blueprint->Damage.push_back(BlueprintDamage(BlueprintDamageType::SCROLL));
}

void World::InvalidateZoom() {
    if (Blueprint == nullptr) { return; }

    // C# clears the texture cache after notifying objects; our DGRP renderers hold texture pointers, so
    // clear first and let the invalidated renderers re-fetch them.
    State._2D->ClearTextureCache();
    for (auto item : Blueprint->Objects) {
        item->OnZoomChanged(State);
    }
    Blueprint->Damage.push_back(BlueprintDamage(BlueprintDamageType::ZOOM));
}

void World::InvalidatePreciseZoom() {
    if (Blueprint == nullptr) { return; }
    Blueprint->Damage.push_back(BlueprintDamage(BlueprintDamageType::PRECISE_ZOOM));
}

void World::InvalidateRotation() {
    if (Blueprint == nullptr) { return; }

    State._2D->ClearTextureCache();
    for (auto item : Blueprint->Objects) {
        item->OnRotationChanged(State);
    }
    Blueprint->Damage.push_back(BlueprintDamage(BlueprintDamageType::ROTATE));
}

void World::InvalidateScroll() {
    if (Blueprint == nullptr) { return; }

    for (auto item : Blueprint->Objects) {
        item->OnScrollChanged(State);
    }
    Blueprint->Damage.push_back(BlueprintDamage(BlueprintDamageType::SCROLL));
}

void World::InvalidateFloor() {
    if (Blueprint == nullptr) { return; }
    Blueprint->Damage.push_back(BlueprintDamage(BlueprintDamageType::LEVEL_CHANGED));
}

void World::Scroll(const st::Vector2& dir) {
    st::Vector2 basis[2];
    GetScrollBasis(basis);
    State.CenterTile(State.CenterTile() + basis[0] * dir.X + basis[1] * dir.Y);
}

void World::GetScrollBasis(st::Vector2 output[2]) {
    switch (State.Rotation()) {
        case WorldRotation::TopLeft:
            output[1] = st::Vector2(2, 2);
            output[0] = st::Vector2(1, -1);
            break;
        case WorldRotation::TopRight:
            output[1] = st::Vector2(2, -2);
            output[0] = st::Vector2(-1, -1);
            break;
        case WorldRotation::BottomRight:
            output[1] = st::Vector2(-2, -2);
            output[0] = st::Vector2(-1, 1);
            break;
        case WorldRotation::BottomLeft:
            output[1] = st::Vector2(-2, 2);
            output[0] = st::Vector2(1, 1);
            break;
    }
    int multiplier = ((1 << (3 - (int)State.Zoom())) * 3) / 2;
    output[0] = output[0] * (float)multiplier;
    output[1] = output[1] * (float)multiplier;
}

void World::Update() {
    // STUB(3ds): ScrollAnchor follows an avatar's pelvis bone (needs the Vitaboy skeleton); smooth zoom is
    // a GPU scale effect (PreciseZoom) and is not ported.
}

void World::PreDraw() {
    //bound the scroll so we can't see gray space.
    float boundfactor = 0.5f;
    switch (State.Zoom()) {
        case WorldZoom::Near: boundfactor = 1.20f; break;
        case WorldZoom::Medium: boundfactor = 1.05f; break;
        default: break;
    }
    boundfactor *= (Blueprint != nullptr) ? Blueprint->Width : 64;
    auto off = 0.5f * ((Blueprint != nullptr) ? Blueprint->Width : 64);
    auto tile = State.CenterTile();
    tile = st::Vector2(std::min(boundfactor + off, std::max(off - boundfactor, tile.X)), std::min(boundfactor + off, std::max(off - boundfactor, tile.Y)));
    // TS1(sims): FreeSims' bounds (up to 1.2 lot widths past the edge at near zoom) suit TSO, where the
    // neighbouring terrain surrounds the lot. A TS1 lot ends in black (the road and sidewalk are floor tiles inside the
    // lot, see shots/details/lot07_far.png), so the view centre stays a few tiles inside the lot: the street along the
    // edge stays reachable, the empty space beyond it is never more than about half the screen.
    if (TS1ScrollBounds && Blueprint != nullptr && State.WorldSpace.WorldPxWidth > 0) {
        // the tile under the middle of the screen (CenterTile sits CadgeBaseLine pixels off it)
        st::Vector2 mid(State.WorldSpace.WorldPxWidth / 2.0f, State.WorldSpace.WorldPxHeight / 2.0f);
        auto shown = State.WorldSpace.GetTileAtPosWithScroll(mid) - State.CenterTile() + tile;
        float m = (State.Zoom() == WorldZoom::Far) ? 6.0f : (State.Zoom() == WorldZoom::Medium ? 4.0f : 3.0f);
        float w = (float)Blueprint->Width, h = (float)Blueprint->Height;
        st::Vector2 clamped(std::min(w - m, std::max(m, shown.X)), std::min(h - m, std::max(m, shown.Y)));
        tile = tile + (clamped - shown);
    }
    if (FreeCenter) tile = State.CenterTile(); // ADDED(sims): the caller places the view (front/LotSave pictures)
    if (tile != State.CenterTile()) State.CenterTile(tile);

    if (HasInit == false || Blueprint == nullptr) { return; }
    State._2D->PreciseZoom = State.PreciseZoom();
    _2DWorld.ClearColor = ClearColor;
    State.OutsideColor = Blueprint->OutsideColor;
    for (auto ent : Blueprint->Objects) {
        ent->Update(State);
    }

    //For all the tiles in the dirty list, re-render them
    _2DWorld.PreDraw(State);
}

void World::Draw(Framebuffer& target) {
    if (target.Width != (int32_t)State.WorldSpace.WorldPxWidth || target.Height != (int32_t)State.WorldSpace.WorldPxHeight) {
        State.SetDimensions(st::Vector2((float)target.Width, (float)target.Height));
        InvalidateScroll();
        if (Blueprint != nullptr) Blueprint->Damage.push_back(BlueprintDamage(BlueprintDamageType::PRECISE_ZOOM)); // buffer size changed
    }
    FrameCounter++;
    PreDraw();
    if (HasInit == false) { target.Clear(ClearColor); return; }
    InternalDraw(target);
}

void World::InternalDraw(Framebuffer& target) {
    State._2D->OutputDepth = true;
    // World2D fills the whole target from its composite of the static layers (FSO_CPP, see World2D.h)
    _2DWorld.ClearColor = ClearColor;
    _2DWorld.Draw(State, target);
    if (DrawAvatars) DrawAvatarComponents(target);
    if (AvatarDrawHook) AvatarDrawHook(*this, target);
    State._2D->OutputDepth = false;
}

}}

namespace FSO { namespace LotView {

// World3D.DrawAfter2D / _3DWorldBatch: avatars are drawn by the Vitaboy software rasterizer
// (src/vitaboy/AvatarRenderer) with the same colour buffer and depth test as the 2D world.
// The 2D batch stores depth as -viewZ in world units (utils/WorldCamera.h); the avatar renderer uses the
// XNA NDC depth of WorldCamera View*Projection. Both are affine in view z, so the buffer is converted
// at the boundary (2 sample points give the affine map), then avatars test/write NDC depth.
void World::DrawAvatarComponents(Framebuffer& target) {
    if (Blueprint == nullptr || Blueprint->Avatars.empty()) return;
    auto& ws = State.WorldSpace;
    FSO::Vitaboy::WorldCamera cam;
    cam.Zoom = (FSO::Vitaboy::WorldZoom)(int)State.Zoom();
    cam.Rotation = State.Rotation();
    cam.CenterTile = State.CenterTile();
    cam.ViewDimensions = st::Vector2((float)target.Width, (float)target.Height);
    cam.PreciseZoom = 1.0f;
    // align with the 2D placement: GetScreenOffset() rounds the scroll; the 3D camera does not.
    // Object sprites hang off GetScreenFromTile(pos) + GetScreenOffset() + (CadgeWidth/2, CadgeBaseLine) - rotAdj,
    // the camera puts the same point at (W/2,H/2) + GetScreenFromTile(pos - center). (+1,0): PosCenterOffsets.
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

    // ndc = a * depth + b. FSO_CPP: instead of converting the whole depth buffer to NDC and back (2 passes over the
    // frame), the avatar renderer writes depth * (1/a) - b/a, i.e. the world's own depth space.
    st::Vector3 p0(0, 0, 0), p1(30, 7, 50);
    float d0 = State.Camera()->Depth(p0), d1 = State.Camera()->Depth(p1);
    float n0 = cam.DepthOfWorld(p0), n1 = cam.DepthOfWorld(p1);
    float a = (n1 - n0) / (d1 - d0), b = n0 - a * d0;

    FSO::Vitaboy::Framebuffer fb(target.Width, target.Height, reinterpret_cast<uint8_t*>(target.Color.data()));
    FSO::Vitaboy::ZBuffer zb(target.Width, target.Height, target.Depth.data());
    for (auto ava : Blueprint->Avatars) {
        if (ava == nullptr || ava->Avatar == nullptr || !ava->Visible) continue;
        auto pos = ava->DrawPosition();
        if (pos.X < -2043 && pos.Y < -2043) continue; // out of world
        if (ava->Level > State.Level()) continue;
        if (DrawAvatarShadows) DrawAvatarShadow(target, st::Vector3(pos.X, pos.Y, (ava->Level - 1) * 2.95f), 0.3f);
        FSO::Vitaboy::AvatarDrawOptions opt;
        opt.DepthScale = 1.0f / a;
        opt.DepthOffset = -b / a;
        if (State._2D->AmbientLight != nullptr) opt.Tint = State._2D->AmbientLight[ava->Room];
        if (opt.Tint.A == 0) opt.Tint = st::Color::White();
        auto r = FSO::Vitaboy::DrawAvatar(fb, zb, cam, *ava->Avatar, pos, ava->RadianDirection, opt);
        if (ava->CensorFlags != 0 && r.Pixels > 0) DrawCensor(target, ava, cam, r.MinX, r.MinY, r.MaxX, r.MaxY);
        if (getenv("WORLD_DEBUG")) printf("avatar #%d at %.2f,%.2f,%.2f: %d/%d tris, %d px, bounds %d,%d-%d,%d\n", ava->ObjectID, pos.X, pos.Y, pos.Z,
                                          r.TrianglesDrawn, r.Triangles, r.Pixels, r.MinX, r.MinY, r.MaxX, r.MaxY);
    }
}

// TS1(sims): the censor mosaic. TS1 behaviours set person data CensorshipFlags (shower, bath, hot tub, changing
// clothes...) and the game pixelates square areas of the screen over the flagged body parts (FreeSO keeps the flags,
// VMCensorshipFlags, but never draws them). Each area is centred on the posed bone (Skeleton AbsolutePosition through
// the avatar's world matrix, exactly as AvatarRenderer places it) and filled with block averages; skipped when the
// bone is hidden behind something nearer (depth test at its pixel). Full body: the avatar's screen bounds.
void World::DrawCensor(Framebuffer& target, Components::AvatarComponent* ava, const FSO::Vitaboy::WorldCamera& cam, int minX, int minY, int maxX, int maxY) {
    auto skel = ava->Avatar->Skeleton;
    if (skel == nullptr) return;
    float tpw = State.WorldSpace.TilePxWidth;
    int block = std::max(2, (int)(tpw / 10.0f));             // mosaic cell: ~3 / 6 / 12 px at far / medium / near
    auto mosaic = [&](int x0, int y0, int x1, int y1) {
        x0 = std::max(0, x0); y0 = std::max(0, y0); x1 = std::min<int32_t>(target.Width - 1, x1); y1 = std::min<int32_t>(target.Height - 1, y1);
        // cells aligned to the box so they move with the Sim
        for (int by = y0; by <= y1; by += block) for (int bx = x0; bx <= x1; bx += block) {
            int ex = std::min(x1, bx + block - 1), ey = std::min(y1, by + block - 1);
            uint32_t r = 0, g = 0, b = 0, n = 0;
            for (int y = by; y <= ey; y++) for (int x = bx; x <= ex; x++) {
                uint32_t c = target.Color[(size_t)y * target.Width + x];
                r += c & 0xFF; g += (c >> 8) & 0xFF; b += (c >> 16) & 0xFF; n++;
            }
            if (n == 0) continue;
            uint32_t avg = (r / n) | ((g / n) << 8) | ((b / n) << 16) | 0xFF000000u;
            for (int y = by; y <= ey; y++) for (int x = bx; x <= ex; x++) target.Color[(size_t)y * target.Width + x] = avg;
        }
    };
    if (ava->CensorFlags & 128) { mosaic(minX, minY, maxX, maxY); return; }
    static const struct { int bit; const char* bone; float half; } parts[] = {
        { 1, "PELVIS", 0.16f }, { 2, "SPINE2", 0.15f }, { 4, "HEAD", 0.13f },
        { 8, "L_HAND", 0.08f }, { 16, "R_HAND", 0.08f }, { 32, "L_FOOT", 0.08f }, { 64, "R_FOOT", 0.08f },
    };
    auto pos = ava->DrawPosition();
    auto world = FSO::Vitaboy::Matrix::CreateRotationY((float)(M_PI - ava->RadianDirection)) *
                 FSO::Vitaboy::Matrix::CreateTranslation(FSO::Vitaboy::WorldCamera::GetWorldFromTile(pos));
    for (auto& p : parts) {
        if ((ava->CensorFlags & p.bit) == 0) continue;
        auto bone = skel->GetBone(p.bone);
        if (bone == nullptr && p.bit == 2) bone = skel->GetBone("SPINE1");
        if (bone == nullptr) continue;
        auto w = FSO::Vitaboy::Transform(bone->AbsolutePosition, world);
        auto s = cam.ProjectWorld(w);
        int cx = (int)std::lround(s.X), cy = (int)std::lround(s.Y);
        if (cx < 0 || cy < 0 || cx >= target.Width || cy >= target.Height) continue;
        if (State.Camera()->Depth(w) - 1.5f > target.Depth[(size_t)cy * target.Width + cx]) continue; // hidden
        int h = std::max(3, (int)std::lround(p.half * tpw));
        // whole cells, centred
        h = ((2 * h + block - 1) / block) * block / 2;
        mosaic(cx - h, cy - h, cx + h - 1, cy + h - 1);
    }
}

// FSO_CPP (TS1): the soft round shadow the original draws on the floor under each Sim. Darkens the frame where the
// floor plane at the Sim's feet is visible (its depth is not behind something nearer), alpha falling off smoothly.
void World::DrawAvatarShadow(Framebuffer& target, const st::Vector3& tilePos, float radius) {
    auto& ws = State.WorldSpace;
    auto K = Utils::TileProjectionOrigin(State);
    auto c = ws.GetScreenFromTile(tilePos);
    float cx = c.X + K.X, cy = c.Y + K.Y;
    float ax = radius * ws.TilePxWidthHalf * 1.41421f, ay = radius * ws.TilePxHeightHalf * 1.41421f;
    auto cam = State.Camera();
    float d0 = cam->Depth(WorldSpace::GetWorldFromTile(tilePos));
    // depth gradient of the floor plane per screen pixel (camera depth is linear)
    auto tx = ws.GetTileFromScreen(st::Vector2(1, 0)), ty = ws.GetTileFromScreen(st::Vector2(0, 1));
    float ddx = cam->Depth(WorldSpace::GetWorldFromTile(st::Vector3(tx.X, tx.Y, 0.0f)));
    float ddy = cam->Depth(WorldSpace::GetWorldFromTile(st::Vector3(ty.X, ty.Y, 0.0f)));
    int x0 = std::max(0, (int)std::floor(cx - ax)), x1 = std::min<int32_t>(target.Width - 1, (int)std::ceil(cx + ax));
    int y0 = std::max(0, (int)std::floor(cy - ay)), y1 = std::min<int32_t>(target.Height - 1, (int)std::ceil(cy + ay));
    const float maxA = 0.45f, eps = 0.6f;
    for (int y = y0; y <= y1; y++) {
        float fy = (y + 0.5f - cy) / ay;
        uint32_t* crow = &target.Color[(size_t)y * target.Width];
        const float* drow = &target.Depth[(size_t)y * target.Width];
        for (int x = x0; x <= x1; x++) {
            float fx = (x + 0.5f - cx) / ax;
            float r2 = fx * fx + fy * fy;
            if (r2 >= 1.0f) continue;
            float plane = d0 + ddx * (x + 0.5f - cx) + ddy * (y + 0.5f - cy);
            if (plane - eps > drow[x]) continue; // something is in front of the floor here
            float f = std::min(1.0f, (1.0f - r2) * 2.5f);
            uint32_t k = 256 - (uint32_t)(maxA * f * 256.0f);
            uint32_t col = crow[x];
            crow[x] = (((col & 0xFF) * k) >> 8) | (((((col >> 8) & 0xFF) * k) >> 8) << 8) | (((((col >> 16) & 0xFF) * k) >> 8) << 16) | (col & 0xFF000000u);
        }
    }
}

}}
