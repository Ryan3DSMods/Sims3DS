/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.LotView.WorldState / WorldSpace (World/WorldState.cs)
#include "world/WorldState.h"
#include "world/World.h"
#include "world/model/Blueprint.h"

namespace FSO { namespace LotView {

// ---------------------------------------------------------------- WorldState

WorldState::WorldState(float worldPxWidth, float worldPxHeight, World* world)
    : WorldSpace(worldPxWidth, worldPxHeight, this), World_(world) {
    WorldCamera_.ViewDimensions = st::Vector2(worldPxWidth, worldPxHeight);
    _Zoom = WorldZoom::Near;
    _Rotation = WorldRotation::TopLeft;
    _Level = 1;
    WorldSpace.Invalidate();
    InvalidateCamera();
}

void WorldState::SetDimensions(const st::Vector2& dim) {
    WorldCamera_.ViewDimensions = dim;
    WorldSpace.SetDimensions(dim);
}

void WorldState::BuildMode(int32_t value) {
    if (_BuildMode != value && World_ != nullptr) World_->InvalidateFloor();
    _BuildMode = value;
}

void WorldState::Level(int8_t value) {
    _Level = value;
    if (World_ != nullptr) World_->InvalidateFloor();
}

void WorldState::InvalidateZoom() {
    WorldSpace.Invalidate();
    InvalidateCamera();
    if (World_ != nullptr) World_->InvalidateZoom();
}

void WorldState::InvalidatePreciseZoom() {
    InvalidateCamera();
    if (World_ != nullptr) World_->InvalidatePreciseZoom();
}

void WorldState::InvalidateRotation() {
    WorldSpace.Invalidate();
    InvalidateCamera();
    if (World_ != nullptr) World_->InvalidateRotation();
}

void WorldState::InvalidateScroll() {
    WorldSpace.Invalidate();
    InvalidateCamera();
    if (World_ != nullptr) World_->InvalidateScroll();
}

void WorldState::InvalidateWorldSize() {
    // C#: also positions the XNA camera (Position/Target); our camera has no matrices, only the depth basis.
    WorldCamera_.WorldSize = (float)WorldSize;
    InvalidateCamera();
}

void WorldState::InvalidateCamera() {
    WorldCamera_.CenterTile = _CenterTile;
    WorldCamera_.Zoom = _Zoom;
    WorldCamera_.Rotation = _Rotation;
    WorldCamera_.PreciseZoom = _PreciseZoom;
}

// ---------------------------------------------------------------- WorldSpace

st::Vector2 WorldSpace::GetScreenOffset() {
    auto centerPos = GetScreenFromTile(State->CenterTile());
    st::Vector2 result(-centerPos.X, -centerPos.Y);
    result.X += (WorldPxWidth / 2.0f);
    result.Y += (WorldPxHeight / 2.0f);
    result.Y -= CadgeBaseLine;
    result.X -= (CadgeWidth / 2.0f);

    switch (State->Rotation()) {
        case WorldRotation::TopLeft:
            result.Y += TilePxHeightHalf;
            break;
        case WorldRotation::TopRight:
            result.X -= TilePxWidthHalf;
            break;
        case WorldRotation::BottomRight:
            result.Y -= TilePxHeightHalf;
            break;
        case WorldRotation::BottomLeft:
            result.X += TilePxWidthHalf;
            break;
    }

    result.X = std::round(result.X);
    result.Y = std::round(result.Y);
    return result;
}

st::Vector2 WorldSpace::GetTileAtPosWithScroll(const st::Vector2& pos) {
    int wallHeight = 0;
    switch (State->Zoom()) {
        case WorldZoom::Far: wallHeight = 57; break;
        case WorldZoom::Medium: wallHeight = 115; break;
        case WorldZoom::Near: wallHeight = 231; break;
    }
    auto t = State->CenterTile() + GetTileFromScreen(pos - st::Vector2((WorldPxWidth / 2.0f), (WorldPxHeight / 2.0f) - wallHeight * (State->Level() - 1)));
    // TS1(sims): terrain altitude lifts the floor plane (FreeSO EstTileAtPosWithScroll): refine a few times
    if (auto bp = State->AltitudeBlueprint) {
        auto t0 = t;
        for (int i = 0; i < 4; i++) {
            float h = bp->InterpAltitude(st::Vector3(t.X, t.Y, 0));
            t = State->CenterTile() + GetTileFromScreen(pos - st::Vector2((WorldPxWidth / 2.0f), (WorldPxHeight / 2.0f) - wallHeight * (State->Level() - 1) - wallHeight * h / 2.95f));
        }
        if (!(t.X == t.X) || !(t.Y == t.Y)) t = t0;
    }
    return t;
}

st::Vector2 WorldSpace::GetTileFromScreen(const st::Vector2& pos) { //gets floor tile at a screen position w/o scroll
    st::Vector2 result;
    switch (State->Rotation()) {
        case WorldRotation::TopLeft:
            result.Y = (pos.Y / TilePxHeightHalf - pos.X / TilePxWidthHalf) / 2;
            result.X = result.Y + pos.X / TilePxWidthHalf;
            break;
        case WorldRotation::TopRight:
            result.Y = (-pos.Y / TilePxHeightHalf - pos.X / TilePxWidthHalf) / 2;
            result.X = -result.Y - pos.X / TilePxWidthHalf;
            break;
        case WorldRotation::BottomRight:
            result.Y = (-pos.Y / TilePxHeightHalf + pos.X / TilePxWidthHalf) / 2;
            result.X = result.Y - pos.X / TilePxWidthHalf;
            break;
        case WorldRotation::BottomLeft:
            result.Y = (pos.Y / TilePxHeightHalf + pos.X / TilePxWidthHalf) / 2;
            result.X = pos.X / TilePxWidthHalf - result.Y;
            break;
    }
    return result;
}

st::Vector2 WorldSpace::GetScreenFromTile(const st::Vector3& tile) {
    float screenx = 0.0f;
    float screeny = 0.0f;
    const float zf = (float)std::cos(M_PI / 6);

    switch (State->Rotation()) {
        case WorldRotation::TopLeft:
            screenx = ((tile.X - tile.Y) * TilePxWidthHalf);
            screeny = ((tile.X + tile.Y) * TilePxHeightHalf) - (tile.Z * OneUnitDistance * zf);
            break;
        case WorldRotation::TopRight:
            /**as y gets bigger pxX gets smaller
            as x gets bigger pxY gets bigger**/
            screenx = ((-tile.X - tile.Y) * TilePxWidthHalf);
            screeny = ((tile.X - tile.Y) * TilePxHeightHalf) - (tile.Z * OneUnitDistance * zf);
            break;
        case WorldRotation::BottomRight:
            screenx = ((-tile.X + tile.Y) * TilePxWidthHalf);
            screeny = ((-tile.X - tile.Y) * TilePxHeightHalf) - (tile.Z * OneUnitDistance * zf);
            break;
        case WorldRotation::BottomLeft:
            screenx = ((tile.X + tile.Y) * TilePxWidthHalf);
            screeny = ((-tile.X + tile.Y) * TilePxHeightHalf) - (tile.Z * OneUnitDistance * zf);
            break;
    }
    return st::Vector2(screenx, screeny);
}

void WorldSpace::Invalidate() {
    switch (State->Zoom()) {
        case WorldZoom::Far:
            TilePxWidth = 32;
            TilePxHeight = 16;
            TilePxWidthHalf = 16;
            TilePxHeightHalf = 8;
            CadgeWidth = 34;
            CadgeHeight = 96;
            CadgeBaseLine = 87;
            break;
        case WorldZoom::Medium:
            TilePxWidth = 64;
            TilePxHeight = 32;
            TilePxWidthHalf = 32;
            TilePxHeightHalf = 16;
            CadgeWidth = 68;
            CadgeHeight = 192;
            CadgeBaseLine = 174;
            break;
        case WorldZoom::Near:
            TilePxWidth = 128;
            TilePxHeight = 64;
            TilePxWidthHalf = 64;
            TilePxHeightHalf = 32;
            CadgeWidth = 136;
            CadgeHeight = 384;
            CadgeBaseLine = 348;
            break;
    }

    OneUnitDistance = (float)std::sqrt(std::pow(TilePxWidth, 2) / 2.0);
    TileSin60 = TilePxWidth / (float)std::sqrt(5.0);
    TileSin30 = TilePxHeight / (float)std::sqrt(5.0);
}

}}
