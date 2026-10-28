/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// AvatarRenderer - software rasterizer for Vitaboy avatars (NEW for the 3DS port).
//
// Replaces the MonoGame path of FreeSims: _3DWorldBatch.DrawMesh/End + Avatar.DrawGeometry +
// Mesh.Draw + the Vitaboy.fx shader (vsVitaboy skinning, psVitaboy texture * AmbientLight with the
// "SoftwareDepth" test against the 2D world's depth buffer). The camera is a faithful port of
// FSO.LotView.Utils.WorldCamera (CalculateView/CalculateProjection), so avatars line up with the
// 2D world exactly as in FreeSims.
//
// ============================== API for the world renderer ==============================
//
//   FSO::Vitaboy::Framebuffer fb{w, h, rgbaPixels, strideBytes};   // RGBA8 bytes (R,G,B,A)
//   FSO::Vitaboy::ZBuffer     zb{w, h, depthFloats, strideFloats}; // same size, float depth
//   FSO::Vitaboy::WorldCamera cam;
//   cam.Zoom = WorldZoom::Near; cam.Rotation = WorldRotation::TopLeft;
//   cam.CenterTile = {cx, cy}; cam.ViewDimensions = {w, h};          // = WorldState.CenterTile etc.
//   DrawAvatar(fb, zb, cam, *vmAvatar->Avatar, AvatarComponent.Position, AvatarComponent.RadianDirection);
//   // or DrawAvatar(fb, zb, cam, avatar, tileX, tileY, level, radianDirection)
//
// Coordinates
//  * Position is AvatarComponent.Position: tile units (x, y) and height z in tiles
//    (VMEntity: z = (Level-1)*2.95 + height). World = RotY(PI - RadianDirection) * T(x*3, z*3, y*3)
//    exactly as AvatarComponent.Draw.
//  * Screen: a 3D point at tile-space P lands at pixel
//        (W/2, H/2) + GetScreenFromTile(P - CenterTile) + cam.PixelOffset
//    where GetScreenFromTile is WorldSpace.GetScreenFromTile for the camera's zoom/rotation
//    (WorldCamera::ScreenFromTile below is the same formula). With FreeSims' 2D placement
//    (sprites at GetScreenFromTile(tile) + GetScreenOffset()) this matches with PixelOffset ~ (1,0)
//    (derived from AvatarComponent.PosCenterOffsets); leave it (0,0) unless you see a 1px seam.
//  * Depth: the value stored in ZBuffer is the XNA NDC depth z/w of the WorldCamera's
//    View*Projection (orthographic, range [0,1], 0 = nearest to the viewer). The 2D world must
//    write its sprite depth in the same space - that is what FreeSims' 2DWorldBatch.fx does
//    (vsZSprite: depth of worldCoords(+offToBack, +dirToFront*zbufferSprite) through
//    worldViewProjection). Use WorldCamera::DepthOfWorld()/DepthOfTile() to compute it.
//  * Depth test: a pixel is drawn when depth <= zbuffer (psVitaboy discards when
//    zbuffer < depth) and the avatar writes its depth. Clear the z-buffer to 1.0 (far).
//
// Performance notes (3DS): skinning is folded into per-bone "bone -> screen" affine matrices
// (orthographic camera => blending after projection is exact), so a vertex costs one or two
// 3x4 transforms. Triangles are rasterised with incremental edge functions, nearest texel
// fetch from 8-bit palettised textures, back faces culled. Next steps if needed: 16.16 fixed
// point edge stepping, 16-bit depth, per-avatar LOD (skip hands at Far zoom), caching the
// skinned screen vertices when the pose did not change.
#pragma once
#include <cstdint>
#include "vitaboy/VitaboyMath.h"
#include "vitaboy/Avatar.h"
#include "world/model/WorldRotation.h"

namespace FSO { namespace Vitaboy {

/// RGBA8 colour buffer (bytes R,G,B,A per pixel). Non-owning view.
struct Framebuffer {
    int32_t Width = 0, Height = 0;
    uint8_t* Pixels = nullptr;
    int32_t Stride = 0; // bytes per row (0 = Width*4)
    Framebuffer() {}
    Framebuffer(int32_t w, int32_t h, uint8_t* px, int32_t stride = 0) : Width(w), Height(h), Pixels(px), Stride(stride ? stride : w * 4) {}
};

/// Float depth buffer, same convention as XNA NDC depth (0 near .. 1 far). Non-owning view.
struct ZBuffer {
    int32_t Width = 0, Height = 0;
    float* Depth = nullptr;
    int32_t Stride = 0; // floats per row (0 = Width)
    ZBuffer() {}
    ZBuffer(int32_t w, int32_t h, float* d, int32_t stride = 0) : Width(w), Height(h), Depth(d), Stride(stride ? stride : w) {}
    void Clear(float value = 1.0f) { for (int32_t y = 0; y < Height; y++) for (int32_t x = 0; x < Width; x++) Depth[y * Stride + x] = value; }
};

/// FSO.LotView.WorldZoom values.
enum class WorldZoom : int { Near = 3, Medium = 2, Far = 1 };

/// Port of FSO.LotView.Utils.WorldCamera (view/projection only) + WorldSpace screen helpers.
class WorldCamera {
public:
    WorldZoom Zoom = WorldZoom::Near;
    FSO::LotView::WorldRotation Rotation = FSO::LotView::WorldRotation::TopLeft;
    Vector2 CenterTile;
    Vector2 ViewDimensions = Vector2(400, 240);
    float PreciseZoom = 1.0f;
    float RotateX = 30.0f; // C# _RotateX (CalculateView hardcodes 30 as well)
    Vector2 PixelOffset;   // extra screen translation (see header comment)

    static constexpr float WorldUnitsPerTile = 3.0f;

    Matrix View() const;        // WorldCamera.CalculateView
    Matrix Projection() const;  // WorldCamera.CalculateProjection
    Matrix GetRotationMatrix() const;
    /// View * Projection * viewport (NDC -> pixels, z kept as NDC depth).
    Matrix ViewProjectionViewport() const;

    /// WorldSpace.GetWorldFromTile: tile (x, y, z) -> world units (x*3, z*3, y*3).
    static Vector3 GetWorldFromTile(const Vector3& tile) { return Vector3(tile.X * WorldUnitsPerTile, tile.Z * WorldUnitsPerTile, tile.Y * WorldUnitsPerTile); }
    /// Projects world units to (pixelX, pixelY, depth).
    Vector3 ProjectWorld(const Vector3& world) const;
    Vector3 ProjectTile(const Vector3& tile) const { return ProjectWorld(GetWorldFromTile(tile)); }
    float DepthOfWorld(const Vector3& world) const { return ProjectWorld(world).Z; }
    float DepthOfTile(const Vector3& tile) const { return ProjectTile(tile).Z; }
    /// WorldSpace.GetScreenFromTile for this zoom/rotation (no scroll offset).
    Vector2 ScreenFromTile(const Vector3& tile) const;
    /// WorldSpace tile metrics for this zoom (TilePxWidth/Height).
    float TilePxWidth() const;
    float TilePxHeight() const { return TilePxWidth() / 2; }
};

enum class AvatarCullMode { None, Back, Front };

struct AvatarDrawOptions {
    st::Color Tint = st::Color(255, 255, 255, 255); // AmbientLight * geom.Color (room light, ghost colour)
    bool Bilinear = false;      // XNA used Linear filtering; nearest is faster and close at Near zoom
    bool DepthTest = true;
    bool DepthWrite = true;
    AvatarCullMode Cull = AvatarCullMode::Back;
    // optional clip rectangle in pixels (Width==0: whole framebuffer)
    st::Rectangle Clip;
    /// Depth written/tested = NDC depth * DepthScale + DepthOffset. Lets the caller test directly against a buffer
    /// in another affine depth space (the 2D world's -viewZ) instead of converting the whole buffer (NDC range
    /// [0,1] maps to the clip range).
    float DepthScale = 1.0f, DepthOffset = 0.0f;
};

/// Draw statistics / touched screen area (useful for dirty rectangles).
struct AvatarDrawResult {
    int32_t Triangles = 0, TrianglesDrawn = 0, Pixels = 0;
    int32_t MinX = 0, MinY = 0, MaxX = -1, MaxY = -1; // inclusive bounds of the projected avatar
};

/// Draws the avatar's bindings with the current pose (Avatar::ReloadSkeleton must have run after
/// posing; VMAvatar::Tick does that every tick).
AvatarDrawResult DrawAvatar(Framebuffer& color, ZBuffer& depth, const WorldCamera& camera, Avatar& avatar,
                            const Vector3& tilePosition, float radianDirection,
                            const AvatarDrawOptions& options = AvatarDrawOptions());

/// Convenience overload: world tile x/y (avatar centre, e.g. LotTilePos.x/16) and floor level (1 = ground).
inline AvatarDrawResult DrawAvatar(Framebuffer& color, ZBuffer& depth, const WorldCamera& camera, Avatar& avatar,
                                   float worldX, float worldY, int32_t level, float radianDirection,
                                   const AvatarDrawOptions& options = AvatarDrawOptions()) {
    return DrawAvatar(color, depth, camera, avatar, Vector3(worldX, worldY, (level - 1) * 2.95f), radianDirection, options);
}

}}
