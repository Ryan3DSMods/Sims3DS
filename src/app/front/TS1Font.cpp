/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims front end — see TS1Font.h. TS1(sims): new code.
#include "app/front/TS1Font.h"
#include "app/front/ResGraphics.h"
#include "app/Font.h"
#include <algorithm>
#include <cstdio>

namespace simapp { namespace front {

static inline uint16_t U16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static inline uint32_t U32(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

bool TS1Font::Load(const std::vector<uint8_t>& d) {
    _Atlas.clear();
    if (d.size() < 64 || d[0] != 'F' || d[1] != 'N' || d[2] != 'T' || d[3] != 'F') return false;
    int count = U16(d.data() + 10);
    uint32_t atlasOff = U32(d.data() + 28);
    if (32 + (size_t)count * 11 > d.size() || atlasOff + 16 > d.size()) return false;
    for (auto& g : _G) g = Glyph();
    int bottom = 0;
    for (int i = 0; i < count; i++) {
        const uint8_t* e = d.data() + 32 + i * 11;
        int ch = U16(e);
        if (ch < 0 || ch > 255) continue;
        Glyph g;
        g.W = e[2]; g.H = e[3]; g.X = U16(e + 4); g.Y = U16(e + 6);
        g.Left = (int8_t)e[8]; g.Right = (int8_t)e[9]; g.Top = (int8_t)e[10];
        _G[ch] = g;
        if (ch > ' ' && ch < 127 && g.H > 1) bottom = std::max(bottom, g.Top + g.H); // ASCII extent (accented capitals reach higher)
    }
    _AtlasW = U16(d.data() + atlasOff + 4);
    _AtlasH = U16(d.data() + atlasOff + 6);
    size_t bytes = ((size_t)_AtlasW * _AtlasH + 1) / 2;
    if (atlasOff + 16 + bytes > d.size()) bytes = d.size() - atlasOff - 16;
    _Atlas.assign(d.begin() + atlasOff + 16, d.begin() + atlasOff + 16 + bytes);
    // line height: the cap/descender extent of the face (e.g. 12 pt -> 17 px)
    _Line = bottom;
    return true;
}

int TS1Font::Advance(unsigned char ch) const {
    const Glyph& g = _G[ch];
    if (ch == ' ') return g.Top < 0 ? -g.Top : std::max(3, _Line / 4);
    if (g.W == 0) return _G['?'].Left + _G['?'].W + _G['?'].Right;
    return g.Left + g.W + g.Right;
}

int TS1Font::Width(const std::string& s) const {
    int w = 0;
    for (unsigned char ch : s) if (ch >= 32) w += Advance(ch);
    return w;
}

int TS1Font::Draw(Canvas& c, int x, int y, const std::string& s, uint32_t color) const {
    for (unsigned char ch : s) {
        if (ch < 32) continue; // \r and other control bytes in the game's strings
        const Glyph& g = _G[ch];
        if (ch != ' ' && g.W > 0) {
            int gx = x + g.Left, gy = y + g.Top;
            for (int yy = 0; yy < g.H; yy++) {
                int ay = g.Y + yy;
                if (ay >= _AtlasH) break;
                for (int xx = 0; xx < g.W; xx++) {
                    size_t i = (size_t)ay * _AtlasW + g.X + xx;
                    if ((i >> 1) >= _Atlas.size()) continue;
                    uint8_t b = _Atlas[i >> 1];
                    int v = (i & 1) ? (b & 15) : (b >> 4);
                    if (v) c.BlendA(gx + xx, gy + yy, color, v * 17);
                }
            }
        }
        x += Advance(ch);
    }
    return x;
}

// ---------------------------------------------------------------- registry + fallbacks

static TS1Font g_Fonts[7];
static const int kSizes[7] = { 9, 10, 11, 12, 14, 16, 20 };
static bool g_Loaded = false;

static int SlotOf(TS1Size s) {
    for (int i = 0; i < 7; i++) if (kSizes[i] == (int)s) return i;
    return 3;
}

bool LoadTS1Fonts() {
    auto& ui = ResGraphics::Get();
    g_Loaded = false;
    if (!ui.IsOpen()) return false;
    for (int i = 0; i < 7; i++) {
        char name[64];
        snprintf(name, sizeof name, "Fonts/variablesans_%02d.ffn", kSizes[i]);
        if (g_Fonts[i].Load(ui.Raw(name))) g_Loaded = true;
    }
    return g_Loaded;
}

bool TS1FontsLoaded() { return g_Loaded; }

static FontId Fallback(TS1Size s) {
    if ((int)s <= 10) return FontId::Small;
    if ((int)s <= 12) return FontId::Bold;
    return FontId::Title;
}

static const TS1Font* FontFor(TS1Size s) {
    if (!g_Loaded) return nullptr;
    const TS1Font& f = g_Fonts[SlotOf(s)];
    return f.Loaded() ? &f : nullptr;
}

int TextW(TS1Size s, const std::string& text) {
    if (auto f = FontFor(s)) return f->Width(text);
    return TextWidth(Fallback(s), text);
}

int TextH(TS1Size s) {
    if (auto f = FontFor(s)) return f->LineHeight();
    return LineHeight(Fallback(s));
}

int Text(Canvas& c, TS1Size s, int x, int y, const std::string& text, uint32_t color, uint32_t shadow) {
    if (auto f = FontFor(s)) {
        if ((shadow >> 24) != 0) f->Draw(c, x + 1, y + 1, text, shadow);
        return f->Draw(c, x, y, text, color);
    }
    if ((shadow >> 24) != 0) DrawText(c, Fallback(s), x + 1, y + 1, text, shadow);
    return DrawText(c, Fallback(s), x, y, text, color);
}

void TextCentered(Canvas& c, TS1Size s, const Rect& r, const std::string& text, uint32_t color, uint32_t shadow) {
    int w = TextW(s, text), h = TextH(s);
    Text(c, s, r.X + (r.W - w) / 2, r.Y + (r.H - h) / 2, text, color, shadow);
}

std::vector<std::string> Wrap(TS1Size s, const std::string& text, int maxWidth) {
    std::vector<std::string> lines;
    size_t start = 0;
    while (start <= text.size()) {
        size_t nl = text.find('\n', start);
        std::string para = text.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
        if (!para.empty() && para.back() == '\r') para.pop_back();
        std::string line;
        size_t p = 0;
        while (p < para.size()) {
            size_t sp = para.find(' ', p);
            std::string word = para.substr(p, sp == std::string::npos ? std::string::npos : sp - p);
            std::string cand = line.empty() ? word : line + " " + word;
            if (!line.empty() && TextW(s, cand) > maxWidth) { lines.push_back(line); line = word; }
            else line = cand;
            if (sp == std::string::npos) break;
            p = sp + 1;
        }
        lines.push_back(line);
        if (nl == std::string::npos) break;
        start = nl + 1;
    }
    return lines;
}

std::string Fit(TS1Size s, const std::string& text, int maxWidth) {
    if (TextW(s, text) <= maxWidth) return text;
    std::string t = text;
    while (!t.empty() && TextW(s, t + "...") > maxWidth) t.pop_back();
    return t + "...";
}

}}
