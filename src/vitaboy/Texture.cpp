/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Avatar texture / BMP decoder (NEW for the 3DS port, see Texture.h)
#include "vitaboy/Texture.h"
#include <cmath>
#include <cstring>

namespace FSO { namespace Vitaboy {

static uint32_t RD32(const std::vector<uint8_t>& d, size_t p) { return p + 4 <= d.size() ? (uint32_t)d[p] | ((uint32_t)d[p+1] << 8) | ((uint32_t)d[p+2] << 16) | ((uint32_t)d[p+3] << 24) : 0; }
static uint16_t RD16(const std::vector<uint8_t>& d, size_t p) { return p + 2 <= d.size() ? (uint16_t)(d[p] | (d[p+1] << 8)) : 0; }
static inline uint32_t Pack(uint8_t r, uint8_t g, uint8_t b, uint8_t a) { return (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | ((uint32_t)a << 24); }

bool Texture::LoadBMP(const std::vector<uint8_t>& d) {
    if (d.size() < 54 || d[0] != 'B' || d[1] != 'M') return false;
    uint32_t dataOff = RD32(d, 10);
    uint32_t hdrSize = RD32(d, 14);
    int32_t w = (int32_t)RD32(d, 18);
    int32_t h = (int32_t)RD32(d, 22);
    uint16_t bpp = RD16(d, 28);
    uint32_t comp = RD32(d, 30);
    uint32_t clrUsed = RD32(d, 46);
    bool topDown = h < 0;
    if (topDown) h = -h;
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096) return false;
    Width = w; Height = h;
    WidthMask = ((w & (w - 1)) == 0) ? w - 1 : 0;
    HeightMask = ((h & (h - 1)) == 0) ? h - 1 : 0;

    if (bpp <= 8) {
        Paletted = true;
        uint32_t ncol = clrUsed ? clrUsed : (1u << bpp);
        size_t palOff = 14 + hdrSize;
        for (uint32_t i = 0; i < 256; i++) {
            if (i < ncol && palOff + i * 4 + 3 < d.size()) {
                const uint8_t* c = &d[palOff + i * 4];
                Palette[i] = Pack(c[2], c[1], c[0], 255);
            } else Palette[i] = Pack(0, 0, 0, 255);
        }
        Indices.assign((size_t)w * h, 0);
        auto put = [&](int32_t x, int32_t y, uint8_t idx) {
            if (x < 0 || x >= w || y < 0 || y >= h) return;
            int32_t row = topDown ? y : (h - 1 - y);
            Indices[(size_t)row * w + x] = idx;
        };
        if (comp == 1 || comp == 2) { // RLE8 / RLE4
            size_t p = dataOff; int32_t x = 0, y = 0;
            while (p + 1 < d.size()) {
                uint8_t n = d[p++], v = d[p++];
                if (n > 0) {
                    for (int i = 0; i < n; i++, x++)
                        put(x, y, comp == 1 ? v : (uint8_t)((i & 1) ? (v & 15) : (v >> 4)));
                } else if (v == 0) { x = 0; y++; }
                else if (v == 1) break;
                else if (v == 2) { if (p + 1 >= d.size()) break; x += d[p++]; y += d[p++]; }
                else {
                    int cnt = v;
                    if (comp == 1) {
                        for (int i = 0; i < cnt && p < d.size(); i++, x++) put(x, y, d[p++]);
                        if (cnt & 1) p++;
                    } else {
                        int bytes = (cnt + 1) / 2;
                        for (int i = 0; i < cnt; i++, x++) {
                            size_t bp = p + i / 2; if (bp >= d.size()) break;
                            put(x, y, (uint8_t)((i & 1) ? (d[bp] & 15) : (d[bp] >> 4)));
                        }
                        p += bytes; if (bytes & 1) p++;
                    }
                }
            }
        } else if (comp == 0) {
            size_t stride = (((size_t)w * bpp + 31) / 32) * 4;
            for (int32_t y = 0; y < h; y++) {
                size_t rowp = dataOff + (size_t)y * stride;
                for (int32_t x = 0; x < w; x++) {
                    size_t bit = (size_t)x * bpp;
                    size_t bp = rowp + bit / 8;
                    if (bp >= d.size()) return false;
                    uint8_t idx;
                    if (bpp == 8) idx = d[bp];
                    else if (bpp == 4) idx = (x & 1) ? (d[bp] & 15) : (d[bp] >> 4);
                    else idx = (d[bp] >> (7 - (bit & 7))) & 1;
                    put(x, y, idx);
                }
            }
        } else return false;
        return true;
    }

    if (bpp == 24 || bpp == 32) {
        Paletted = false;
        Pixels.assign((size_t)w * h, 0);
        size_t bypp = bpp / 8;
        size_t stride = (((size_t)w * bpp + 31) / 32) * 4;
        for (int32_t y = 0; y < h; y++) {
            int32_t row = topDown ? y : (h - 1 - y);
            for (int32_t x = 0; x < w; x++) {
                size_t p = dataOff + (size_t)y * stride + x * bypp;
                if (p + bypp > d.size()) return false;
                Pixels[(size_t)row * w + x] = Pack(d[p + 2], d[p + 1], d[p], 255);
            }
        }
        return true;
    }
    return false;
}

uint32_t Texture::SampleBilinear(float u, float v) const {
    float fx = u * Width - 0.5f, fy = v * Height - 0.5f;
    float flx = std::floor(fx), fly = std::floor(fy);
    int32_t x0 = (int32_t)flx, y0 = (int32_t)fly;
    float ax = fx - flx, ay = fy - fly;
    uint32_t c00 = Fetch(x0, y0), c10 = Fetch(x0 + 1, y0), c01 = Fetch(x0, y0 + 1), c11 = Fetch(x0 + 1, y0 + 1);
    uint32_t out = 0;
    for (int s = 0; s < 32; s += 8) {
        float a = (float)((c00 >> s) & 255) * (1 - ax) + (float)((c10 >> s) & 255) * ax;
        float b = (float)((c01 >> s) & 255) * (1 - ax) + (float)((c11 >> s) & 255) * ax;
        float r = a * (1 - ay) + b * ay;
        out |= ((uint32_t)(r + 0.5f) & 255) << s;
    }
    return out;
}

}}
