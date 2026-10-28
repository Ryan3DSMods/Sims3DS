/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
#include "app/Canvas.h"
#include <algorithm>
#include <cstdlib>
#include <cmath>

namespace simapp {

void Canvas::Clear(uint32_t c) { std::fill(Px.begin(), Px.end(), c | 0xFF000000u); }

void Canvas::FillRect(int x, int y, int w, int h, uint32_t c) {
    int x0 = std::max(0, x), y0 = std::max(0, y), x1 = std::min(Width, x + w), y1 = std::min(Height, y + h);
    if (x0 >= x1 || y0 >= y1) return;
    bool opaque = (c >> 24) >= 255;
    for (int yy = y0; yy < y1; yy++) {
        uint32_t* row = &Px[(size_t)yy * Width];
        if (opaque) std::fill(row + x0, row + x1, c);
        else for (int xx = x0; xx < x1; xx++) BlendPx(row[xx], c);
    }
}

// Rounded rectangle with a 1px anti-aliased corner.
void Canvas::FillRoundRect(int x, int y, int w, int h, int radius, uint32_t c) {
    if (radius <= 0) { FillRect(x, y, w, h, c); return; }
    radius = std::min(radius, std::min(w, h) / 2);
    for (int yy = 0; yy < h; yy++) {
        int dy = 0;
        if (yy < radius) dy = radius - yy;
        else if (yy >= h - radius) dy = yy - (h - radius - 1);
        if (dy == 0) { FillRect(x, y + yy, w, 1, c); continue; }
        float fy = dy - 0.5f;
        float span = std::sqrt(std::max(0.0f, (float)(radius * radius) - fy * fy));
        float inset = radius - span;
        int full = (int)std::ceil(inset);
        FillRect(x + full, y + yy, w - 2 * full, 1, c);
        float cov = (float)full - inset; // partial coverage of the edge pixel
        if (full > 0) {
            int a = (int)(cov * 255);
            BlendA(x + full - 1, y + yy, c, a);
            BlendA(x + w - full, y + yy, c, a);
        }
    }
}

void Canvas::StrokeRect(int x, int y, int w, int h, uint32_t c) {
    FillRect(x, y, w, 1, c); FillRect(x, y + h - 1, w, 1, c);
    FillRect(x, y + 1, 1, h - 2, c); FillRect(x + w - 1, y + 1, 1, h - 2, c);
}

void Canvas::StrokeRoundRect(int x, int y, int w, int h, int radius, uint32_t c) {
    radius = std::min(radius, std::min(w, h) / 2);
    FillRect(x + radius, y, w - 2 * radius, 1, c);
    FillRect(x + radius, y + h - 1, w - 2 * radius, 1, c);
    FillRect(x, y + radius, 1, h - 2 * radius, c);
    FillRect(x + w - 1, y + radius, 1, h - 2 * radius, c);
    // corners: quarter circles
    for (int i = 0; i <= radius * 2; i++) {
        float a = (float)i / (radius * 2) * 1.5707963f;
        int dx = (int)std::lround(radius - std::cos(a) * radius), dy = (int)std::lround(radius - std::sin(a) * radius);
        Blend(x + dx, y + dy, c);
        Blend(x + w - 1 - dx, y + dy, c);
        Blend(x + dx, y + h - 1 - dy, c);
        Blend(x + w - 1 - dx, y + h - 1 - dy, c);
    }
}

static inline int Lerp8(int a, int b, int t, int n) { return a + (b - a) * t / std::max(1, n); }

void Canvas::GradientV(int x, int y, int w, int h, uint32_t top, uint32_t bottom) {
    for (int i = 0; i < h; i++) {
        uint32_t c = RGBA(Lerp8(top & 255, bottom & 255, i, h - 1), Lerp8((top >> 8) & 255, (bottom >> 8) & 255, i, h - 1),
                          Lerp8((top >> 16) & 255, (bottom >> 16) & 255, i, h - 1), Lerp8(top >> 24, bottom >> 24, i, h - 1));
        FillRect(x, y + i, w, 1, c);
    }
}

void Canvas::Line(int x0, int y0, int x1, int y1, uint32_t c) {
    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        Blend(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void Canvas::FillCircle(int cx, int cy, int r, uint32_t c) {
    uint32_t baseA = c >> 24;
    for (int y = -r - 1; y <= r + 1; y++) {
        for (int x = -r - 1; x <= r + 1; x++) {
            float d = std::sqrt((float)(x * x + y * y)) - (float)r;
            if (d <= -0.5f) Blend(cx + x, cy + y, c);
            else if (d < 0.5f) BlendA(cx + x, cy + y, c, (int)((0.5f - d) * baseA));
        }
    }
}

void Canvas::StrokeCircle(int cx, int cy, int r, uint32_t c) {
    uint32_t baseA = c >> 24;
    for (int y = -r - 1; y <= r + 1; y++) {
        for (int x = -r - 1; x <= r + 1; x++) {
            float d = std::fabs(std::sqrt((float)(x * x + y * y)) - (float)r);
            if (d < 1.0f) BlendA(cx + x, cy + y, c, (int)((1.0f - d) * baseA));
        }
    }
}

void Canvas::FillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t c) {
    int minX = std::max(0, std::min(x0, std::min(x1, x2))), maxX = std::min(Width - 1, std::max(x0, std::max(x1, x2)));
    int minY = std::max(0, std::min(y0, std::min(y1, y2))), maxY = std::min(Height - 1, std::max(y0, std::max(y1, y2)));
    auto edge = [](int ax, int ay, int bx, int by, int px, int py) { return (bx - ax) * (py - ay) - (by - ay) * (px - ax); };
    int area = edge(x0, y0, x1, y1, x2, y2);
    if (area == 0) return;
    for (int y = minY; y <= maxY; y++)
        for (int x = minX; x <= maxX; x++) {
            int w0 = edge(x1, y1, x2, y2, x, y), w1 = edge(x2, y2, x0, y0, x, y), w2 = edge(x0, y0, x1, y1, x, y);
            if (area > 0 ? (w0 >= 0 && w1 >= 0 && w2 >= 0) : (w0 <= 0 && w1 <= 0 && w2 <= 0)) Blend(x, y, c);
        }
}

void Canvas::Dim(int x, int y, int w, int h, int mul) {
    int x0 = std::max(0, x), y0 = std::max(0, y), x1 = std::min(Width, x + w), y1 = std::min(Height, y + h);
    for (int yy = y0; yy < y1; yy++) {
        uint32_t* row = &Px[(size_t)yy * Width];
        for (int xx = x0; xx < x1; xx++) {
            uint32_t p = row[xx];
            uint32_t r = ((p & 255) * mul) >> 8, g = (((p >> 8) & 255) * mul) >> 8, b = (((p >> 16) & 255) * mul) >> 8;
            row[xx] = r | (g << 8) | (b << 16) | 0xFF000000u;
        }
    }
}

// Box-filtered downscale (each destination pixel averages the 2x2 nearest source samples).
// Column/row sample positions are computed once per call (no divisions in the pixel loop): this runs every
// frame for the touch-screen view of the lot (260x156 from 400x240).
void Canvas::BlitScaled(const uint32_t* src, int sw, int sh, const Rect& dst, bool blend) {
    if (dst.W <= 0 || dst.H <= 0 || sw <= 0 || sh <= 0) return;
    int x0 = std::max(0, dst.X), x1 = std::min(Width, dst.X + dst.W);
    if (x0 >= x1) return;
    static std::vector<int> cx0, cx1; // single-threaded app
    cx0.resize((size_t)dst.W); cx1.resize((size_t)dst.W);
    for (int x = 0; x < dst.W; x++) {
        cx0[(size_t)x] = (int)(((int64_t)x * 2 * sw) / (2 * dst.W));
        cx1[(size_t)x] = std::min(sw - 1, (int)(((int64_t)(2 * x + 1) * sw) / (2 * dst.W)));
    }
    for (int y = 0; y < dst.H; y++) {
        int dy = dst.Y + y;
        if ((unsigned)dy >= (unsigned)Height) continue;
        int sy0 = (int)(((int64_t)y * 2 * sh) / (2 * dst.H));
        int sy1 = std::min(sh - 1, (int)(((int64_t)(2 * y + 1) * sh) / (2 * dst.H)));
        const uint32_t* r0 = src + (size_t)sy0 * sw;
        const uint32_t* r1 = src + (size_t)sy1 * sw;
        uint32_t* out = &Px[(size_t)dy * Width];
        const int* c0 = cx0.data() - dst.X;
        const int* c1 = cx1.data() - dst.X;
        if (!blend) {
            for (int dx = x0; dx < x1; dx++) {
                uint32_t a = r0[c0[dx]], b = r0[c1[dx]], c = r1[c0[dx]], d = r1[c1[dx]];
                // average 4 pixels per channel: R and B together in one register, G separately
                uint32_t rb = ((a & 0x00FF00FF) + (b & 0x00FF00FF) + (c & 0x00FF00FF) + (d & 0x00FF00FF)) >> 2;
                uint32_t g = ((a & 0x0000FF00) + (b & 0x0000FF00) + (c & 0x0000FF00) + (d & 0x0000FF00)) >> 2;
                out[dx] = (rb & 0x00FF00FF) | (g & 0x0000FF00) | 0xFF000000u;
            }
            continue;
        }
        for (int dx = x0; dx < x1; dx++) {
            uint32_t px[4] = { r0[c0[dx]], r0[c1[dx]], r1[c0[dx]], r1[c1[dx]] };
            uint32_t sa = 0, sr = 0, sg = 0, sb = 0;
            for (auto p : px) { uint32_t al = p >> 24; sa += al; sr += (p & 255) * al; sg += ((p >> 8) & 255) * al; sb += ((p >> 16) & 255) * al; }
            if (sa == 0) continue;
            uint32_t col = (sr / sa) | ((sg / sa) << 8) | ((sb / sa) << 16) | ((sa / 4) << 24);
            BlendPx(out[dx], col);
        }
    }
}

void Canvas::Blit(const uint32_t* src, int sw, int sh, int x, int y, bool blend) {
    if (!blend && x >= 0 && x + sw <= Width) {
        for (int yy = 0; yy < sh; yy++) {
            int dy = y + yy;
            if ((unsigned)dy >= (unsigned)Height) continue;
            std::copy(src + (size_t)yy * sw, src + (size_t)yy * sw + sw, &Px[(size_t)dy * Width + x]);
        }
        return;
    }
    for (int yy = 0; yy < sh; yy++) {
        int dy = y + yy;
        if ((unsigned)dy >= (unsigned)Height) continue;
        for (int xx = 0; xx < sw; xx++) {
            int dx = x + xx;
            if ((unsigned)dx >= (unsigned)Width) continue;
            uint32_t c = src[(size_t)yy * sw + xx];
            if (blend) BlendPx(Px[(size_t)dy * Width + dx], c);
            else Px[(size_t)dy * Width + dx] = c | 0xFF000000u;
        }
    }
}

}
