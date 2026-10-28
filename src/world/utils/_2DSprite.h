/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.LotView.Utils._2DSprite / _2DBatchRenderMode (World/utils/_2DSprite.cs, _2DWorldBatch.cs)
#pragma once
#include <cstdint>
#include "core/math.h"

namespace FSO { namespace LotView { namespace Utils {

enum class _2DBatchRenderMode {
    NO_DEPTH,
    Z_BUFFER,
    RESTORE_DEPTH,
    WALL
};

/// STUB(3ds): stand-in for XNA Texture2D. A view onto already decoded sprite data (no copy):
/// Data = colour texels (SPR#/SPR2 PixelData), Alpha8 = single channel texels (SPR2 ZBufferData or a
/// generated wall/floor z-buffer). The renderer samples them exactly like the point samplers of 2DWorldBatch.fx.
struct Texture2D {
    int32_t Width = 0;
    int32_t Height = 0;
    const st::Color* Data = nullptr;
    const uint8_t* Alpha8 = nullptr;
    // FIX(sims) memory: palettised colour texels (SPR2 frames, see SPR2.h) when Data is null:
    // colour = Palette[Index[i]] with alpha Alpha[i]
    const uint8_t* Index = nullptr;
    const uint8_t* Alpha = nullptr;
    const st::Color* Palette = nullptr;
    bool HasColor() const { return Data != nullptr || (Index != nullptr && Palette != nullptr); }
};

/// Pixel + z-buffer pair (FSO.Files.Utils.WorldTexture).
struct WorldTexture {
    Texture2D* Pixel = nullptr;
    Texture2D* ZBuffer = nullptr;
};

/// Represents a 2D sprite in the game.
class _2DSprite {
public:
    _2DBatchRenderMode RenderMode = _2DBatchRenderMode::NO_DEPTH;
    Texture2D* Pixel = nullptr;
    Texture2D* Depth = nullptr;
    Texture2D* Mask = nullptr;
    st::Vector3 TilePosition;
    st::Vector3 WorldPosition;
    int16_t ObjectID = 0; //used for mouse hit test render mode
    uint16_t Room = 0xFFFF; //room to use for ambient light

    st::Rectangle SrcRect;
    st::Rectangle DestRect;

    //For internal use, do not set this
    int32_t DrawOrder = 0;
    bool FlipHorizontally = false;
    bool FlipVertically = false;

    st::Rectangle AbsoluteDestRect;
    st::Vector3 AbsoluteWorldPosition;
    st::Vector3 AbsoluteTilePosition;

    void Repurpose() {
        Pixel = nullptr;
        Depth = nullptr;
        Mask = nullptr;
        WorldPosition = st::Vector3();
        TilePosition = st::Vector3();
        ObjectID = 0;
        Room = 0xFFFF;

        //rects are written always by sprite drawer
        FlipHorizontally = false;
        FlipVertically = false;
    }
};

}}}
