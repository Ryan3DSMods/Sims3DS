/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.LotView.WorldState / WorldSpace (World/WorldState.cs)
// C# properties with invalidating setters become getter/setter pairs of the same name
// (state.Zoom() / state.Zoom(WorldZoom::Medium)); the Silent* variants set without invalidation.
// WorldSize stays a public field because the VM (VMWorldActivator) assigns it directly.
#pragma once
#include <cstdint>
#include <cmath>
#include "core/math.h"
#include "world/WorldZoom.h"
#include "world/model/WorldRotation.h"
#include "world/utils/WorldCamera.h"

namespace FSO { namespace LotView {

class World;
class WorldState;
namespace Model { class Blueprint; }
namespace Utils { class _2DWorldBatch; }
namespace Components { class AvatarComponent; }

/// Holds information about the world space and drawing numbers.
class WorldSpace {
public:
    static constexpr float WorldUnitsPerTile = 3.0f;

    /// How big is a tile in 2D space
    float TilePxHeight = 0.0f;
    float TilePxWidth = 0.0f;

    float TilePxHeightHalf = 0.0f;
    float TilePxWidthHalf = 0.0f;

    float CadgeWidth = 0;
    float CadgeHeight = 0;
    float CadgeBaseLine = 0;

    float TileSin60 = 0;
    float TileSin30 = 0;

    /// How big is the world view area in pixels
    float WorldPxWidth;
    float WorldPxHeight;
    float OneUnitDistance = 0;

    WorldSpace(float worldPxWidth, float worldPxHeight, WorldState* state)
        : WorldPxWidth(worldPxWidth), WorldPxHeight(worldPxHeight), State(state) {}

    void SetDimensions(const st::Vector2& dim) { WorldPxWidth = dim.X; WorldPxHeight = dim.Y; }

    /// Gets the offset for the screen based on the scroll position
    st::Vector2 GetScreenOffset();
    /// Gets indices of a tile given a position with a scroll offset.
    st::Vector2 GetTileAtPosWithScroll(const st::Vector2& pos);
    /// Gets indices of a tile given a position without a scroll offset.
    st::Vector2 GetTileFromScreen(const st::Vector2& pos);
    /// Get screen coordinates from a tile coordinate without applying a scroll offset.
    st::Vector2 GetScreenFromTile(const st::Vector2& tile) { return GetScreenFromTile(st::Vector3(tile.X, tile.Y, 0.0f)); }
    st::Vector2 GetScreenFromTile(const st::Vector3& tile);

    static float GetWorldFromTile(float tile) { return tile * WorldUnitsPerTile; }
    /// Get world units from tile units
    static st::Vector3 GetWorldFromTile(const st::Vector2& tile) { return GetWorldFromTile(st::Vector3(tile.X, tile.Y, 0.0f)); }
    static st::Vector3 GetWorldFromTile(const st::Vector3& tile) {
        return st::Vector3(tile.X * WorldUnitsPerTile, tile.Z * WorldUnitsPerTile, tile.Y * WorldUnitsPerTile);
    }

    void Invalidate();

private:
    WorldState* State;
};

/// Holds state information retaining to world.
class WorldState {
public:
    WorldState(float worldPxWidth, float worldPxHeight, World* world);

    void SetDimensions(const st::Vector2& dim);

    /// Gets the camera used by this WorldState instance.
    Utils::WorldCamera* Camera() { return &WorldCamera_; }

    bool TempDraw = false; //set for OBJID mode and thumbs
    FSO::LotView::WorldSpace WorldSpace;
    Utils::_2DWorldBatch* _2D = nullptr;
    st::Color OutsideColor; //temporary to give this to terrain component. in future it will use ambient light texture
    bool DynamicCutaway = false;
    Components::AvatarComponent* ScrollAnchor = nullptr;

    /// Size of world (tiles). Plain field: the VM writes it directly; call InvalidateWorldSize() after.
    int32_t WorldSize = 64;
    void InvalidateWorldSize();

    bool DrawRoofs = false;
    /// TS1(sims): the blueprint whose terrain altitude the tile picking follows (FreeSO World.EstTileAtPosWithScroll)
    const FSO::LotView::Model::Blueprint* AltitudeBlueprint = nullptr;

    int32_t SilentBuildMode() const { return _BuildMode; }
    void SilentBuildMode(int32_t v) { _BuildMode = v; }
    int32_t BuildMode() const { return _BuildMode; }
    void BuildMode(int32_t value);

    /// What level is being displayed
    int8_t Level() const { return _Level; }
    void Level(int8_t value);
    /// Set level without invalidating.
    int8_t SilentLevel() const { return _Level; }
    void SilentLevel(int8_t value) { _Level = value; }

    float PreciseZoom() const { return _PreciseZoom; }
    void PreciseZoom(float value) { _PreciseZoom = value; InvalidatePreciseZoom(); }

    /// What zoom level is being displayed
    WorldZoom Zoom() const { return _Zoom; }
    void Zoom(WorldZoom value) { auto old = _Zoom; _Zoom = value; if (value != old) InvalidateZoom(); }
    WorldZoom SilentZoom() const { return _Zoom; }
    void SilentZoom(WorldZoom value) { _Zoom = value; }

    /// What rotation is being displayed
    WorldRotation Rotation() const { return _Rotation; }
    void Rotation(WorldRotation value) { _Rotation = value; InvalidateRotation(); }
    WorldRotation SilentRotation() const { return _Rotation; }
    void SilentRotation(WorldRotation value) { _Rotation = value; }

    /// Draw entities even if they are out of world. (for thumbnails)
    bool DrawOOB = false;

    st::Vector2 CenterTile() const { return _CenterTile; }
    void CenterTile(const st::Vector2& value) { _CenterTile = value; InvalidateScroll(); }

    void InvalidateCamera();

protected:
    void InvalidateZoom();
    void InvalidatePreciseZoom();
    void InvalidateRotation();
    void InvalidateScroll();

private:
    World* World_;
    Utils::WorldCamera WorldCamera_;
    int32_t _BuildMode = 0;
    int8_t _Level = 1;
    float _PreciseZoom = 1.0f;
    WorldZoom _Zoom = WorldZoom::Near;
    WorldRotation _Rotation = WorldRotation::TopLeft;
    st::Vector2 _CenterTile;
};

}}
