/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Avatar texture (NEW for the 3DS port).
// Replaces FSO.Content.Model.ITextureRef / Texture2D for Vitaboy: TS1 skin textures are Windows
// BMP files (8-bit palettised, some RLE8). They are kept palettised (w*h bytes + 256-entry
// palette) which is 4x smaller than RGBA - 64KB for the usual 256x256 body texture.
#pragma once
#include <cstdint>
#include <cmath>
#include <string>
#include <vector>

namespace FSO { namespace Vitaboy {

class Texture {
public:
    int32_t Width = 0;
    int32_t Height = 0;
    bool Paletted = true;
    std::vector<uint8_t> Indices;   // Paletted: Width*Height indices, row 0 = top of the image
    std::vector<uint32_t> Pixels;   // !Paletted: Width*Height packed RGBA (R | G<<8 | B<<16 | A<<24)
    uint32_t Palette[256] = {};     // packed RGBA
    int32_t WidthMask = 0, HeightMask = 0; // Width-1/Height-1 when power of two, else 0
    std::string Name;

    /// Decodes a Windows BMP (1/4/8-bit palettised incl. RLE8/RLE4, 24/32-bit). Returns false on bad data.
    bool LoadBMP(const std::vector<uint8_t>& data);

    /// Texel fetch with wrap addressing (XNA AddressU/V = Wrap), u,v in texture space [0,1).
    inline uint32_t SampleNearest(float u, float v) const {
        int32_t x = (int32_t)std::floor(u * Width);
        int32_t y = (int32_t)std::floor(v * Height);
        return Fetch(x, y);
    }
    inline uint32_t Fetch(int32_t x, int32_t y) const {
        if (WidthMask) x &= WidthMask; else { x %= Width; if (x < 0) x += Width; }
        if (HeightMask) y &= HeightMask; else { y %= Height; if (y < 0) y += Height; }
        int32_t i = y * Width + x;
        return Paletted ? Palette[Indices[i]] : Pixels[i];
    }
    /// Bilinear (XNA MinFilter/MagFilter = Linear), wrap addressing.
    uint32_t SampleBilinear(float u, float v) const;

    size_t MemoryBytes() const { return Indices.size() + Pixels.size() * 4 + sizeof(Palette); }
};

}}
