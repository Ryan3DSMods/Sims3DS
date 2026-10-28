/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims app — a tiny 32-bit software canvas for the UI (both screens).
// Pixels use the same packing as FSO::LotView::Framebuffer: R | G<<8 | B<<16 | A<<24 (bytes R,G,B,A).
// Platform code converts a canvas to the console framebuffer format (src/platform/3ds/main.cpp)
// or to PNG (src/tools/sim3ds_host.cpp).
#pragma once
#include <cstdint>
#include <vector>
#include <string>

namespace simapp {

inline uint32_t RGBA(int r, int g, int b, int a = 255) {
    return (uint32_t)(r & 255) | ((uint32_t)(g & 255) << 8) | ((uint32_t)(b & 255) << 16) | ((uint32_t)(a & 255) << 24);
}

struct Rect {
    int X = 0, Y = 0, W = 0, H = 0;
    Rect() {}
    Rect(int x, int y, int w, int h) : X(x), Y(y), W(w), H(h) {}
    bool Contains(int px, int py) const { return px >= X && py >= Y && px < X + W && py < Y + H; }
    int Right() const { return X + W; }
    int Bottom() const { return Y + H; }
};

class Canvas {
public:
    int Width = 0, Height = 0;
    std::vector<uint32_t> Px;

    Canvas() {}
    Canvas(int w, int h) { Resize(w, h); }
    void Resize(int w, int h) { Width = w; Height = h; Px.assign((size_t)w * h, RGBA(0, 0, 0)); }

    void Clear(uint32_t c);
    void FillRect(int x, int y, int w, int h, uint32_t c);          // opaque or alpha-blended by c's alpha
    void FillRect(const Rect& r, uint32_t c) { FillRect(r.X, r.Y, r.W, r.H, c); }
    void FillRoundRect(int x, int y, int w, int h, int radius, uint32_t c);
    void FillRoundRect(const Rect& r, int radius, uint32_t c) { FillRoundRect(r.X, r.Y, r.W, r.H, radius, c); }
    void StrokeRect(int x, int y, int w, int h, uint32_t c);
    void StrokeRoundRect(int x, int y, int w, int h, int radius, uint32_t c);
    void GradientV(int x, int y, int w, int h, uint32_t top, uint32_t bottom);
    void Line(int x0, int y0, int x1, int y1, uint32_t c);
    void FillCircle(int cx, int cy, int r, uint32_t c);
    void StrokeCircle(int cx, int cy, int r, uint32_t c);
    void FillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t c);
    /// Darkens/tints a region (multiplies colour by mul/256 and adds c).
    void Dim(int x, int y, int w, int h, int mul);
    void Blend(int x, int y, uint32_t c) {
        if ((unsigned)x >= (unsigned)Width || (unsigned)y >= (unsigned)Height) return;
        BlendPx(Px[(size_t)y * Width + x], c);
    }
    /// Alpha blend of `alpha` (0..255) times colour c (its own alpha is ignored).
    void BlendA(int x, int y, uint32_t c, int alpha) {
        if ((unsigned)x >= (unsigned)Width || (unsigned)y >= (unsigned)Height || alpha <= 0) return;
        uint32_t ca = (uint32_t)((((c >> 24) & 255) * (uint32_t)alpha) / 255);
        BlendPx(Px[(size_t)y * Width + x], (c & 0x00FFFFFFu) | (ca << 24));
    }
    /// Copies (with scaling, box-averaged) a 32-bit image into dst rect; blend: use the source alpha.
    void BlitScaled(const uint32_t* src, int sw, int sh, const Rect& dst, bool blend = false);
    /// Copies a 32-bit image at x,y (alpha blended unless opaque).
    void Blit(const uint32_t* src, int sw, int sh, int x, int y, bool blend = true);

    static inline void BlendPx(uint32_t& d, uint32_t c) {
        uint32_t a = c >> 24;
        if (a == 0) return;
        if (a >= 255) { d = c | 0xFF000000u; return; }
        uint32_t ia = 255 - a;
        uint32_t r = ((c & 255) * a + (d & 255) * ia) / 255;
        uint32_t g = (((c >> 8) & 255) * a + ((d >> 8) & 255) * ia) / 255;
        uint32_t b = (((c >> 16) & 255) * a + ((d >> 16) & 255) * ia) / 255;
        d = r | (g << 8) | (b << 16) | 0xFF000000u;
    }
};

}
