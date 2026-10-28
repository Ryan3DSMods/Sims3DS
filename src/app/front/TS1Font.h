/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims front end — the original The Sims UI typeface ("variablesans", UIGraphics.far Fonts\*.ffn).
//
// TS1(sims): the .ffn format is undocumented; this reader was worked out from the files:
//   0  "FNTF"  u32 file size  u16 ?  u16 glyph count (224 = chars 0x20..0xFF)  u16 ?  ...  u32 @28 = atlas offset
//   32 glyph table, 11 bytes each: u16 char, u8 w, u8 h, u16 atlas x, u16 atlas y, s8 left, s8 right, s8 top
//      (top = offset below the line top; for the space the last byte is minus its advance)
//   atlas: u32 ?, u16 width (256), u16 height, 8 bytes 0, then 4 bpp coverage (high nibble = left pixel).
// The "_s" / "_bs" variants carry a baked shadow and are not used (shadows are drawn here instead).
// Falls back to the app's Inter font (app/Font.h) when UIGraphics.far is absent.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "app/Canvas.h"

namespace simapp { namespace front {

class TS1Font {
public:
    struct Glyph { uint8_t W = 0, H = 0; uint16_t X = 0, Y = 0; int8_t Left = 0, Right = 0, Top = 0; };
    bool Load(const std::vector<uint8_t>& data);
    bool Loaded() const { return !_Atlas.empty(); }
    int Width(const std::string& s) const;
    int LineHeight() const { return _Line; }
    int Draw(Canvas& c, int x, int y, const std::string& s, uint32_t color) const;

private:
    Glyph _G[256];
    int _AtlasW = 0, _AtlasH = 0, _Line = 0;
    std::vector<uint8_t> _Atlas; // 4 bpp
    int Advance(unsigned char ch) const;
};

/// Text sizes used by the front end (point sizes of the original font files).
enum class TS1Size { S9 = 9, S10 = 10, S11 = 11, S12 = 12, S14 = 14, S16 = 16, S20 = 20 };

/// Loads the fonts from UIGraphics (call once after ResGraphics::Open). Returns false if none could be read.
bool LoadTS1Fonts();
bool TS1FontsLoaded();
int TextW(TS1Size s, const std::string& text);
int TextH(TS1Size s);
/// Draws Latin-1 text with its top at y; shadow (if non-zero alpha) is drawn 1px down-right first.
int Text(Canvas& c, TS1Size s, int x, int y, const std::string& text, uint32_t color, uint32_t shadow = 0);
void TextCentered(Canvas& c, TS1Size s, const Rect& r, const std::string& text, uint32_t color, uint32_t shadow = 0);
std::vector<std::string> Wrap(TS1Size s, const std::string& text, int maxWidth);
std::string Fit(TS1Size s, const std::string& text, int maxWidth);

}}
