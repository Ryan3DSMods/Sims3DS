/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.LotView.Utils._2DWorldBatch (World/utils/_2DWorldBatch.cs + Content/Effects/2DWorldBatch.fx)
//
// "Similar to SpriteBatch but has additional features that target world object rendering in this game
// such as zbuffers." The C# batch collects sprites between Begin/End and draws them on the GPU with the
// techniques of 2DWorldBatch.fx. This is a CPU rasterizer with the same semantics:
//   * NO_DEPTH  -> drawSimple:              colour only, no depth test/write.
//   * Z_BUFFER  -> drawZSprite(DepthChannel): per-pixel depth = backDepth + ((1 - z/255) / 0.4) * frontDepth,
//                  where backDepth = Depth(worldPos + offToBack) and frontDepth = Depth(+dirToFront) - backDepth
//                  (vsZSprite), z = the sprite's z-buffer texel; LessEqual test, depth write, alpha blend,
//                  colour multiplied by the room's ambient light.
//   * WALL      -> drawZWall(DepthChannel): as Z_BUFFER but alpha comes from the style mask texture.
// The main FreeSims draw always runs with OutputDepth = true (World.InternalDraw / WithBuffer), so the
// *DepthChannel pixel shaders are the ones translated here (they always apply ambient light).
// Sprites are drawn in submission order per mode (NO_DEPTH, Z_BUFFER, WALL) instead of grouped by texture
// (a GPU batching detail; it only matters for exactly equal depths).
#pragma once
#include <cstdint>
#include <vector>
#include <memory>
#include <unordered_map>
#include "core/math.h"
#include "world/Framebuffer.h"
#include "world/utils/_2DSprite.h"
#include "world/utils/WorldCamera.h"

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks { class SPRFrame; class SPR2Frame; } } } } }

namespace FSO { namespace LotView { namespace Utils {

class _2DWorldBatch {
public:
    _2DWorldBatch();

    bool OutputDepth = true;
    bool OBJIDMode = false;
    /// Ambient light per room (C#: a 256x256 texture filled from Blueprint.RoomColors). 65536 entries.
    const st::Color* AmbientLight = nullptr;
    float PreciseZoom = 1.0f;

    /// The render target (back buffer or one of the static layer buffers).
    Framebuffer* Target = nullptr;

    void SetScroll(const st::Vector2& scroll) { Scroll = scroll; }
    st::Vector2 GetScroll() const { return Scroll; }
    void OffsetPixel(const st::Vector2& pxOffset) { PxOffset = pxOffset; }
    void OffsetTile(const st::Vector3& tileOffset);
    void SetObjID(int16_t obj) { ObjectID = obj; }

    _2DSprite* NewSprite(_2DBatchRenderMode mode);
    void Draw(_2DSprite* sprite);

    /// Reset for a draw loop
    void Begin(WorldCamera* worldCamera);
    void Pause() { End(); }
    void Resume() { Begin(WorldCamera_); }
    /// Processes the accumulated draw commands and paints Target.
    void End();

    st::Rectangle GetSpriteListBounds();

    /// Gets a texture from this 2DWorldBatch's texture cache (decodes the frame on first use).
    Texture2D* GetTexture(FSO::Files::Formats::IFF::Chunks::SPRFrame* item);
    Texture2D* GetTexture(FSO::Files::Formats::IFF::Chunks::SPR2Frame* item);
    WorldTexture GetWorldTexture(FSO::Files::Formats::IFF::Chunks::SPR2Frame* item);
    void ClearTextureCache();

    /// Statistics of the last End() (for tools).
    int32_t SpritesDrawn = 0;

    static st::Vector3 FrontDirForRot(WorldRotation rot);
    static st::Vector3 BackOffForRot(WorldRotation rot);

private:
    std::vector<_2DSprite*> Sprites[4];
    std::vector<std::unique_ptr<_2DSprite>> SpritePool;
    size_t SpriteIndex = 0;
    int32_t DrawOrder = 0;
    WorldCamera* WorldCamera_ = nullptr;

    st::Vector2 PxOffset;
    st::Vector3 WorldOffset;
    st::Vector3 TileOffset;
    int16_t ObjectID = 0;
    st::Vector2 Scroll;

    std::unordered_map<const void*, Texture2D> _TextureCache;
    std::unordered_map<const void*, Texture2D> _ZTextureCache;

    // scratch for the rasterizer (texel lookup per destination column/row)
    std::vector<int32_t> ColPx, ColZ, ColM, RowPx, RowZ, RowM;

    void RenderSprite(_2DSprite* sprite);
};

}}}
