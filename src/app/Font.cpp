/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
#include "app/Font.h"
#include "app/FontData.h"

namespace simapp {

using namespace fontdata;

struct FontDesc { const uint8_t* Bits; const Glyph* Glyphs; int Ascent; int Line; };

static FontDesc Get(FontId f) {
    switch (f) {
        case FontId::Bold: return { BoldBits, BoldGlyphs, BoldAscent, BoldLineHeight };
        case FontId::Title: return { TitleBits, TitleGlyphs, TitleAscent, TitleLineHeight };
        default: return { SmallBits, SmallGlyphs, SmallAscent, SmallLineHeight };
    }
}

static inline int Index(unsigned char ch) {
    if (ch < 32) return '?' - 32;
    return ch - 32;
}

int LineHeight(FontId f) { return Get(f).Line; }

int TextWidth(FontId f, const std::string& s) {
    auto d = Get(f);
    int w = 0;
    for (unsigned char ch : s) w += d.Glyphs[Index(ch)].advance;
    return w;
}

int DrawText(Canvas& c, FontId f, int x, int y, const std::string& s, uint32_t color) {
    auto d = Get(f);
    for (unsigned char ch : s) {
        const Glyph& g = d.Glyphs[Index(ch)];
        if (g.w > 0) {
            int gx = x + g.xoff, gy = y + g.yoff;
            for (int yy = 0; yy < g.h; yy++) {
                for (int xx = 0; xx < g.w; xx++) {
                    int i = yy * g.w + xx;
                    int nib = (d.Bits[g.offset + (i >> 1)] >> ((i & 1) * 4)) & 15;
                    if (nib) c.BlendA(gx + xx, gy + yy, color, nib * 17);
                }
            }
        }
        x += g.advance;
    }
    return x;
}

void DrawTextCentered(Canvas& c, FontId f, const Rect& r, const std::string& s, uint32_t color) {
    int w = TextWidth(f, s);
    int h = LineHeight(f);
    DrawText(c, f, r.X + (r.W - w) / 2, r.Y + (r.H - h) / 2, s, color);
}

int DrawTextShadow(Canvas& c, FontId f, int x, int y, const std::string& s, uint32_t color, uint32_t shadow) {
    DrawText(c, f, x + 1, y + 1, s, shadow);
    return DrawText(c, f, x, y, s, color);
}

std::string Ellipsize(FontId f, const std::string& s, int maxWidth) {
    if (TextWidth(f, s) <= maxWidth) return s;
    std::string r = s;
    while (!r.empty() && TextWidth(f, r + "...") > maxWidth) r.pop_back();
    while (!r.empty() && r.back() == ' ') r.pop_back();
    return r + "...";
}

std::string ToLatin1(const std::string& s) {
    // only decode if the whole string is valid UTF-8 with multi-byte sequences
    std::string out;
    bool multibyte = false;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x80) { out += (char)c; i++; continue; }
        if ((c & 0xE0) == 0xC0 && i + 1 < s.size() && ((unsigned char)s[i + 1] & 0xC0) == 0x80) {
            unsigned cp = ((c & 0x1F) << 6) | ((unsigned char)s[i + 1] & 0x3F);
            out += (char)(cp < 256 ? cp : '?');
            multibyte = true;
            i += 2;
            continue;
        }
        return s; // not UTF-8: already Latin-1 / cp1252
    }
    return multibyte ? out : s;
}

std::vector<std::string> WrapText(FontId f, const std::string& text, int maxWidth) {
    std::vector<std::string> lines;
    std::string line, word;
    auto flushWord = [&]() {
        if (word.empty()) return;
        std::string cand = line.empty() ? word : line + " " + word;
        if (TextWidth(f, cand) <= maxWidth || line.empty()) line = cand;
        else { lines.push_back(line); line = word; }
        while (TextWidth(f, line) > maxWidth && line.size() > 1) { // very long word
            size_t n = line.size();
            while (n > 1 && TextWidth(f, line.substr(0, n)) > maxWidth) n--;
            lines.push_back(line.substr(0, n)); line = line.substr(n);
        }
        word.clear();
    };
    for (char ch : text) {
        if (ch == '\r') continue;
        if (ch == '\n') { flushWord(); lines.push_back(line); line.clear(); continue; }
        if (ch == ' ' || ch == '\t') { flushWord(); continue; }
        word += ch;
    }
    flushWord();
    if (!line.empty()) lines.push_back(line);
    return lines;
}

}
