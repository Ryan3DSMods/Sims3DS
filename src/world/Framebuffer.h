/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims: the software render target that replaces MonoGame's back buffer / RenderTarget2D pairs.
// Color is RGBA8888 packed little-endian (R | G<<8 | B<<16 | A<<24, i.e. bytes R,G,B,A in memory),
// Depth is the per-pixel world depth written by the z-buffered sprite techniques of 2DWorldBatch.fx
// (smaller = closer to the camera, cleared to +inf), see utils/WorldCamera.h for the depth function.
#pragma once
#include <cstdint>
#include <vector>
#include <limits>
#include <algorithm>
#include <cstring>

namespace FSO { namespace LotView {

class Framebuffer {
public:
    int32_t Width = 0;
    int32_t Height = 0;
    std::vector<uint32_t> Color;
    std::vector<float> Depth;
    /// FSO_CPP (deferred room light): when non-empty (same size as Color), the rasterizers write unlit colour and the
    /// room id of each pixel here; the light is applied when the layer is composited (World2D), so a lighting change
    /// (time of day, a lamp switched) only re-composites instead of re-rendering every sprite.
    std::vector<uint16_t> Room;
    /// Room id for pixels lit by the outside colour (terrain, roofs).
    static const uint16_t OUTSIDE_LIGHT_ROOM = 65532;
    /// Clip rectangle for the rasterizers (_2DWorldBatch sprites, TriangleRaster), exclusive max. World2D uses it to
    /// redraw only the strip of a cached layer that scrolled into view.
    int32_t ClipX0 = 0, ClipY0 = 0, ClipX1 = 0x7FFFFFFF, ClipY1 = 0x7FFFFFFF;
    void SetClip(int32_t x0, int32_t y0, int32_t x1, int32_t y1) { ClipX0 = x0; ClipY0 = y0; ClipX1 = x1; ClipY1 = y1; }
    void ResetClip() { ClipX0 = 0; ClipY0 = 0; ClipX1 = 0x7FFFFFFF; ClipY1 = 0x7FFFFFFF; }
    /// Clears colour and depth inside [x0,x1) x [y0,y1).
    void ClearRect(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t rgba) {
        x0 = std::max<int32_t>(0, x0); y0 = std::max<int32_t>(0, y0); x1 = std::min<int32_t>(Width, x1); y1 = std::min<int32_t>(Height, y1);
        for (int32_t y = y0; y < y1; y++) {
            std::fill(Color.begin() + (size_t)y * Width + x0, Color.begin() + (size_t)y * Width + x1, rgba);
            std::fill(Depth.begin() + (size_t)y * Width + x0, Depth.begin() + (size_t)y * Width + x1, std::numeric_limits<float>::infinity());
            if (!Room.empty()) std::fill(Room.begin() + (size_t)y * Width + x0, Room.begin() + (size_t)y * Width + x1, (uint16_t)65535);
        }
    }

    Framebuffer() {}
    Framebuffer(int32_t w, int32_t h) { Resize(w, h); }

    void Resize(int32_t w, int32_t h) {
        if (w == Width && h == Height && !Color.empty()) return;
        Width = w; Height = h;
        Color.assign((size_t)w * h, 0);
        Depth.assign((size_t)w * h, std::numeric_limits<float>::infinity());
        if (!Room.empty()) Room.assign((size_t)w * h, 65535);
    }

    /// Enables/disables the per-pixel room ids (deferred lighting).
    void EnableRoom(bool on) { if (on) Room.assign(Color.size(), 65535); else { Room.clear(); Room.shrink_to_fit(); } }
    void Clear(uint32_t rgba) {
        std::fill(Color.begin(), Color.end(), rgba);
        if (!Room.empty()) std::fill(Room.begin(), Room.end(), (uint16_t)65535);
        std::fill(Depth.begin(), Depth.end(), std::numeric_limits<float>::infinity());
    }

    static uint32_t Pack(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        return (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | ((uint32_t)a << 24);
    }

    /// Convert to RGB565 (e.g. for the 3DS framebuffer); dst must hold Width*Height entries.
    void ToRGB565(uint16_t* dst) const {
        size_t n = Color.size();
        for (size_t i = 0; i < n; i++) {
            uint32_t c = Color[i];
            dst[i] = (uint16_t)(((c & 0xF8) << 8) | ((c >> 5) & 0x7E0) | ((c >> 19) & 0x1F));
        }
    }
};

}}
