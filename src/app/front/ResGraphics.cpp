/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims front end — see UIGraphics.h. TS1(sims): new code (no FreeSims equivalent).
#include "app/front/ResGraphics.h"
#include "files/FAR1/FAR1Archive.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

namespace simapp { namespace front {

using FSO::Files::FAR1::FAR1Archive;
using FSO::Files::FAR1::FarEntry;

// ======================================================================= image decoding

static inline uint16_t U16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static inline uint32_t U32(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

static inline bool IsMagenta(uint32_t r, uint32_t g, uint32_t b) { return r >= 240 && g <= 8 && b >= 240; }

static bool DecodeBMP(const uint8_t* d, size_t n, Canvas& out) {
    if (n < 54 || d[0] != 'B' || d[1] != 'M') return false;
    uint32_t pixOff = U32(d + 10);
    uint32_t hdr = U32(d + 14);
    if (hdr < 12 || 14 + hdr > n) return false;
    int32_t w, h; int bpp; uint32_t comp = 0, clrUsed = 0;
    if (hdr == 12) { w = U16(d + 18); h = (int16_t)U16(d + 20); bpp = U16(d + 24); }
    else { w = (int32_t)U32(d + 18); h = (int32_t)U32(d + 22); bpp = U16(d + 28); comp = U32(d + 30); clrUsed = U32(d + 46); }
    bool topDown = h < 0;
    if (h < 0) h = -h;
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096) return false;
    // palette
    std::vector<uint32_t> pal;
    if (bpp <= 8) {
        uint32_t count = clrUsed ? clrUsed : (1u << bpp);
        size_t po = 14 + hdr, es = (hdr == 12) ? 3 : 4;
        pal.resize(256, 0xFF000000u);
        for (uint32_t i = 0; i < count && i < 256 && po + i * es + 3 <= n; i++) {
            const uint8_t* e = d + po + i * es;
            pal[i] = RGBA(e[2], e[1], e[0]);
        }
    }
    out.Resize(w, h);
    auto row = [&](int y) -> uint32_t* { return &out.Px[(size_t)(topDown ? y : (h - 1 - y)) * w]; };
    if (pixOff >= n) return false;
    const uint8_t* p = d + pixOff;
    const uint8_t* end = d + n;
    if (comp == 1 || comp == 2) { // RLE8 / RLE4: rows are emitted bottom-up (BMP order)
        std::vector<uint8_t> idx((size_t)w * h, 0);
        int x = 0, y = 0; // y counts in file order
        bool rle4 = comp == 2;
        while (p + 1 < end && y < h) {
            uint8_t c = p[0], v = p[1];
            p += 2;
            if (c > 0) {
                for (int i = 0; i < c && x < w; i++, x++)
                    idx[(size_t)y * w + x] = rle4 ? ((i & 1) ? (v & 15) : (v >> 4)) : v;
            } else if (v == 0) { x = 0; y++; }
            else if (v == 1) break;
            else if (v == 2) { if (p + 1 >= end) break; x += p[0]; y += p[1]; p += 2; }
            else {
                int cnt = v;
                int bytes = rle4 ? (cnt + 1) / 2 : cnt;
                for (int i = 0; i < cnt && x < w; i++, x++) {
                    uint8_t b = (p + (rle4 ? i / 2 : i) < end) ? p[rle4 ? i / 2 : i] : 0;
                    idx[(size_t)y * w + x] = rle4 ? ((i & 1) ? (b & 15) : (b >> 4)) : b;
                }
                p += (bytes + 1) & ~1;
            }
        }
        for (int fy = 0; fy < h; fy++) {
            uint32_t* r = &out.Px[(size_t)(topDown ? fy : (h - 1 - fy)) * w];
            for (int xx = 0; xx < w; xx++) r[xx] = pal[idx[(size_t)fy * w + xx]];
        }
        return true;
    }
    if (comp != 0 && !(comp == 3 && (bpp == 16 || bpp == 32))) return false;
    size_t stride = (((size_t)w * bpp + 31) / 32) * 4;
    for (int fy = 0; fy < h; fy++) {
        const uint8_t* s = p + (size_t)fy * stride;
        if (s + stride > end) break;
        uint32_t* r = &out.Px[(size_t)(topDown ? fy : (h - 1 - fy)) * w];
        for (int x = 0; x < w; x++) {
            switch (bpp) {
                case 1: r[x] = pal[(s[x >> 3] >> (7 - (x & 7))) & 1]; break;
                case 4: r[x] = pal[(x & 1) ? (s[x >> 1] & 15) : (s[x >> 1] >> 4)]; break;
                case 8: r[x] = pal[s[x]]; break;
                case 16: { uint16_t v = U16(s + x * 2); r[x] = RGBA(((v >> 10) & 31) * 255 / 31, ((v >> 5) & 31) * 255 / 31, (v & 31) * 255 / 31); break; }
                case 24: r[x] = RGBA(s[x * 3 + 2], s[x * 3 + 1], s[x * 3]); break;
                case 32: r[x] = RGBA(s[x * 4 + 2], s[x * 4 + 1], s[x * 4]); break;
                default: return false;
            }
        }
    }
    (void)row;
    return true;
}

static bool DecodeTGA(const uint8_t* d, size_t n, Canvas& out) {
    if (n < 18) return false;
    int idLen = d[0], cmType = d[1], type = d[2];
    int cmFirst = U16(d + 3), cmLen = U16(d + 5), cmBits = d[7];
    int w = U16(d + 12), h = U16(d + 14), bpp = d[16], desc = d[17];
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096) return false;
    if (type != 1 && type != 2 && type != 3 && type != 9 && type != 10 && type != 11) return false;
    size_t p = 18 + idLen;
    std::vector<uint32_t> cmap;
    if (cmType == 1) {
        int eb = (cmBits + 7) / 8;
        cmap.resize((size_t)cmFirst + cmLen, 0xFF000000u);
        for (int i = 0; i < cmLen && p + eb <= n; i++, p += eb) {
            const uint8_t* e = d + p;
            cmap[(size_t)cmFirst + i] = (eb >= 3) ? RGBA(e[2], e[1], e[0], eb == 4 ? e[3] : 255) : RGBA(e[0], e[0], e[0]);
        }
    }
    int pb = (bpp + 7) / 8;
    bool rle = type >= 9;
    auto readPx = [&](const uint8_t* s) -> uint32_t {
        if (type == 1 || type == 9) { uint32_t i = pb == 2 ? U16(s) : s[0]; return i < cmap.size() ? cmap[i] : 0xFF000000u; }
        if (type == 3 || type == 11) return RGBA(s[0], s[0], s[0]);
        if (pb == 2) { uint16_t v = U16(s); return RGBA(((v >> 10) & 31) * 255 / 31, ((v >> 5) & 31) * 255 / 31, (v & 31) * 255 / 31, (v & 0x8000) ? 255 : 255); }
        return RGBA(s[2], s[1], s[0], pb == 4 ? s[3] : 255);
    };
    out.Resize(w, h);
    std::vector<uint32_t> lin((size_t)w * h, 0);
    size_t total = (size_t)w * h, i = 0;
    while (i < total && p < n) {
        if (rle) {
            uint8_t hdr = d[p++];
            int cnt = (hdr & 0x7F) + 1;
            if (hdr & 0x80) {
                if (p + pb > n) break;
                uint32_t v = readPx(d + p); p += pb;
                for (int k = 0; k < cnt && i < total; k++) lin[i++] = v;
            } else {
                for (int k = 0; k < cnt && i < total && p + pb <= n; k++, p += pb) lin[i++] = readPx(d + p);
            }
        } else {
            if (p + pb > n) break;
            lin[i++] = readPx(d + p); p += pb;
        }
    }
    bool topLeft = (desc & 0x20) != 0;
    for (int y = 0; y < h; y++) {
        int dy = topLeft ? y : h - 1 - y;
        std::copy(lin.begin() + (size_t)y * w, lin.begin() + (size_t)(y + 1) * w, out.Px.begin() + (size_t)dy * w);
    }
    return true;
}

bool DecodeImage(const uint8_t* data, size_t size, Canvas& out, bool keyMagenta) {
    bool ok = (size >= 2 && data[0] == 'B' && data[1] == 'M') ? DecodeBMP(data, size, out) : DecodeTGA(data, size, out);
    if (!ok) return false;
    if (keyMagenta) {
        for (auto& p : out.Px) if (IsMagenta(p & 255, (p >> 8) & 255, (p >> 16) & 255)) p = 0;
    }
    return true;
}

void Resample(const Canvas& src, Canvas& dst, int dw, int dh) {
    dw = std::max(1, dw); dh = std::max(1, dh);
    dst.Resize(dw, dh);
    if (src.Width <= 0 || src.Height <= 0) return;
    // separable box filter with fractional coverage, fixed point (16.16), premultiplied alpha
    const int sw = src.Width, sh = src.Height;
    std::vector<uint32_t> acc((size_t)dw * 4);
    std::vector<int32_t> x0(dw), x1(dw);
    std::vector<uint32_t> w0(dw), w1(dw);
    // horizontal spans: dst x covers [x*sw/dw, (x+1)*sw/dw)
    for (int x = 0; x < dw; x++) {
        uint64_t a = (uint64_t)x * sw * 65536 / dw, b = (uint64_t)(x + 1) * sw * 65536 / dw;
        x0[x] = (int32_t)(a >> 16); x1[x] = (int32_t)((b - 1) >> 16);
        w0[x] = (uint32_t)(65536 - (a & 65535)); w1[x] = (uint32_t)(((b - 1) & 65535) + 1);
    }
    std::vector<uint64_t> rowAcc((size_t)dw * 4);
    for (int y = 0; y < dh; y++) {
        uint64_t a = (uint64_t)y * sh * 65536 / dh, b = (uint64_t)(y + 1) * sh * 65536 / dh;
        int sy0 = (int)(a >> 16), sy1 = (int)((b - 1) >> 16);
        std::fill(rowAcc.begin(), rowAcc.end(), 0);
        uint64_t totalW = 0;
        for (int sy = sy0; sy <= sy1 && sy < sh; sy++) {
            uint32_t wy = 65536;
            if (sy == sy0) wy = (uint32_t)(65536 - (a & 65535));
            if (sy == sy1) wy = (sy0 == sy1) ? (uint32_t)(b - a) : (uint32_t)(((b - 1) & 65535) + 1);
            wy >>= 8; // 8.8
            if (!wy) continue;
            totalW += wy;
            const uint32_t* row = &src.Px[(size_t)sy * sw];
            for (int x = 0; x < dw; x++) {
                uint64_t r = 0, g = 0, bl = 0, al = 0, tw = 0;
                for (int sx = x0[x]; sx <= x1[x] && sx < sw; sx++) {
                    uint32_t wx = 65536;
                    if (sx == x0[x]) wx = w0[x];
                    if (sx == x1[x]) wx = (x0[x] == x1[x]) ? (w0[x] + w1[x] - 65536) : w1[x];
                    wx >>= 8;
                    uint32_t p = row[sx];
                    uint32_t pa = (p >> 24) * wx;
                    r += (p & 255) * pa; g += ((p >> 8) & 255) * pa; bl += ((p >> 16) & 255) * pa; al += pa;
                    tw += wx;
                }
                // normalise horizontally to 8.8 weight
                if (tw) { r /= tw; g /= tw; bl /= tw; al /= tw; }
                rowAcc[(size_t)x * 4 + 0] += r * wy; rowAcc[(size_t)x * 4 + 1] += g * wy;
                rowAcc[(size_t)x * 4 + 2] += bl * wy; rowAcc[(size_t)x * 4 + 3] += al * wy;
            }
        }
        uint32_t* out = &dst.Px[(size_t)y * dw];
        for (int x = 0; x < dw; x++) {
            uint64_t al = rowAcc[(size_t)x * 4 + 3];
            if (al == 0 || totalW == 0) { out[x] = 0; continue; }
            uint32_t r = (uint32_t)(rowAcc[(size_t)x * 4 + 0] / al);
            uint32_t g = (uint32_t)(rowAcc[(size_t)x * 4 + 1] / al);
            uint32_t bl = (uint32_t)(rowAcc[(size_t)x * 4 + 2] / al);
            uint32_t aa = (uint32_t)(al / totalW);
            out[x] = RGBA((int)std::min<uint32_t>(255u, r), (int)std::min<uint32_t>(255u, g), (int)std::min<uint32_t>(255u, bl), (int)std::min<uint32_t>(255u, aa));
        }
    }
}

void Crop(const Canvas& src, Canvas& dst, int x, int y, int w, int h) {
    dst.Resize(std::max(0, w), std::max(0, h));
    for (int yy = 0; yy < h; yy++) for (int xx = 0; xx < w; xx++) {
        int sx = x + xx, sy = y + yy;
        dst.Px[(size_t)yy * w + xx] = (sx >= 0 && sy >= 0 && sx < src.Width && sy < src.Height) ? src.Px[(size_t)sy * src.Width + sx] : 0;
    }
}

void BlitWindow(Canvas& dst, const Canvas& src, int sx, int sy) {
    for (int y = 0; y < dst.Height; y++) {
        int yy = sy + y;
        if (yy < 0 || yy >= src.Height) continue;
        const uint32_t* s = &src.Px[(size_t)yy * src.Width];
        uint32_t* d = &dst.Px[(size_t)y * dst.Width];
        for (int x = 0; x < dst.Width; x++) {
            int xx = sx + x;
            if (xx < 0 || xx >= src.Width) continue;
            uint32_t c = s[xx];
            if (c >> 24) Canvas::BlendPx(d[x], c);
        }
    }
}

// ======================================================================= the archive + resource tables

ResGraphics::ResGraphics() {}
ResGraphics::~ResGraphics() {}

ResGraphics& ResGraphics::Get() {
    static ResGraphics instance;
    return instance;
}

std::string ResGraphics::Normalize(const std::string& p) {
    std::string r;
    r.reserve(p.size());
    for (char c : p) r += (c == '/') ? '\\' : (char)std::tolower((unsigned char)c);
    return r;
}

static std::string ReadText(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return "";
    std::string s;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) s.append(buf, n);
    fclose(f);
    return s;
}

bool ResGraphics::Open(const std::string& dir) {
    _Far.reset();
    _Index.clear(); _Consts.clear(); _Paths.clear();
    try {
        _Far.reset(new FAR1Archive(dir + "/UIGraphics.far", false));
    } catch (...) {
        _Far.reset();
        return false;
    }
    auto& entries = _Far->GetAllFarEntries();
    for (size_t i = 0; i < entries.size(); i++) _Index[Normalize(entries[i].Filename)] = i;
    static const char* tables[] = { "Res_Languages", "Res_Nbhd", "Res_CPanel", "Res_Other", "SMCtrlMgrRes" };
    for (auto t : tables) {
        std::string h = ReadText(dir + "/" + t + ".h");
        if (h.empty()) { auto raw = Raw(std::string(t) + ".h"); h.assign(raw.begin(), raw.end()); }
        ParseHeader(h);
    }
    for (auto t : tables) {
        std::string rt = ReadText(dir + "/" + t + ".RT");
        if (rt.empty()) { auto raw = Raw(std::string(t) + ".RT"); rt.assign(raw.begin(), raw.end()); }
        ParseRT(rt);
    }
    return true;
}

// "const Sint32 kName = 5000;"
void ResGraphics::ParseHeader(const std::string& text) {
    size_t pos = 0;
    while (pos < text.size()) {
        size_t eol = text.find('\n', pos);
        std::string line = text.substr(pos, eol == std::string::npos ? std::string::npos : eol - pos);
        pos = (eol == std::string::npos) ? text.size() : eol + 1;
        size_t c = line.find("const");
        if (c == std::string::npos || line.compare(0, 2, "//") == 0) continue;
        size_t eq = line.find('=', c);
        if (eq == std::string::npos) continue;
        // name = last token before '='
        size_t e = eq;
        while (e > 0 && std::isspace((unsigned char)line[e - 1])) e--;
        size_t s = e;
        while (s > 0 && (std::isalnum((unsigned char)line[s - 1]) || line[s - 1] == '_')) s--;
        if (s >= e) continue;
        std::string name = line.substr(s, e - s);
        _Consts[name] = atoi(line.c_str() + eq + 1);
    }
}

// "{RLEBMP,kName,"Path/File.bmp"}" or "{BITMAP,kName,"Path", kRes_LanguageX}" (only the default entry is kept)
void ResGraphics::ParseRT(const std::string& text) {
    size_t pos = 0;
    while ((pos = text.find('{', pos)) != std::string::npos) {
        size_t end = text.find('}', pos);
        if (end == std::string::npos) break;
        size_t lineStart = text.rfind('\n', pos);
        lineStart = (lineStart == std::string::npos) ? 0 : lineStart + 1;
        std::string body = text.substr(pos + 1, end - pos - 1);
        bool commented = text.substr(lineStart, pos - lineStart).find("//") != std::string::npos;
        pos = end + 1;
        if (commented) continue;
        size_t c1 = body.find(',');
        if (c1 == std::string::npos) continue;
        std::string type = body.substr(0, c1);
        if (type == "INCLUDE") continue;
        size_t q1 = body.find('"', c1), q2 = (q1 == std::string::npos) ? q1 : body.find('"', q1 + 1);
        if (q2 == std::string::npos) continue;
        std::string name = body.substr(c1 + 1, q1 - c1 - 1);
        while (!name.empty() && (name.back() == ',' || std::isspace((unsigned char)name.back()))) name.pop_back();
        while (!name.empty() && std::isspace((unsigned char)name[0])) name.erase(0, 1);
        std::string path = body.substr(q1 + 1, q2 - q1 - 1);
        bool lang = body.find("kRes_Language", q2) != std::string::npos;
        int id = Const(name);
        if (id < 0) { // a constant missing from the headers: keep the entry by name
            id = 1000000 + (int)_Consts.size();
            _Consts[name] = id;
        }
        if (lang && _Paths.count(id)) continue;
        if (!_Paths.count(id)) _Paths[id] = path;
    }
}

int ResGraphics::Const(const std::string& name) const {
    auto it = _Consts.find(name);
    return it == _Consts.end() ? -1 : it->second;
}

std::string ResGraphics::PathFor(int id) const {
    auto it = _Paths.find(id);
    return it == _Paths.end() ? std::string() : it->second;
}

bool ResGraphics::Exists(const std::string& path) const { return _Index.count(Normalize(path)) != 0; }

std::vector<uint8_t> ResGraphics::Raw(const std::string& path) const {
    if (!_Far) return {};
    auto it = _Index.find(Normalize(path));
    if (it == _Index.end()) return {};
    auto& e = _Far->GetAllFarEntries()[it->second];
    return _Far->GetEntry(e);
}

bool ResGraphics::Load(const std::string& nameOrPath, Canvas& out, bool keyMagenta) const {
    std::string path = nameOrPath;
    if (!nameOrPath.empty() && nameOrPath[0] == 'k' && nameOrPath.find('/') == std::string::npos && nameOrPath.find('.') == std::string::npos) {
        path = PathFor(Const(nameOrPath));
        if (path.empty()) return false;
    }
    auto raw = Raw(path);
    if (raw.empty()) return false;
    return DecodeImage(raw, out, keyMagenta);
}

bool ResGraphics::LoadScaled(const std::string& nameOrPath, float scale, Canvas& out) const {
    Canvas full;
    if (!Load(nameOrPath, full)) return false;
    if (scale >= 0.999f && scale <= 1.001f) { out = std::move(full); return true; }
    Resample(full, out, (int)(full.Width * scale + 0.5f), (int)(full.Height * scale + 0.5f));
    return true;
}

// ======================================================================= UIButton

bool UIButton::Load(const ResGraphics& ui, const std::string& nameOrPath, float scale, int states) {
    ImageStates = states;
    Canvas full;
    if (!ui.Load(nameOrPath, full)) { Texture = Canvas(); return false; }
    // scale each frame separately so frames do not bleed into each other
    int fw = full.Width / std::max(1, states), fh = full.Height;
    int dw = std::max(1, (int)(fw * scale + 0.5f)), dh = std::max(1, (int)(fh * scale + 0.5f));
    Texture.Resize(dw * states, dh);
    for (auto& p : Texture.Px) p = 0;
    for (int f = 0; f < states; f++) {
        Canvas frame, scaled;
        Crop(full, frame, f * fw, 0, fw, fh);
        Resample(frame, scaled, dw, dh);
        for (int y = 0; y < dh; y++)
            std::copy(scaled.Px.begin() + (size_t)y * dw, scaled.Px.begin() + (size_t)(y + 1) * dw, Texture.Px.begin() + (size_t)y * Texture.Width + f * dw);
    }
    return true;
}

bool UIButton::Update(const AppInput& in) {
    if (!Visible || Disabled || Texture.Width == 0) { _Down = false; return false; }
    Rect r = Bounds();
    bool inside = r.Contains(in.TouchX, in.TouchY);
    if (in.TouchDown && inside) { _Down = true; _Over = true; }
    if (_Down && in.Touching) _Over = inside;
    if (_Down && in.TouchUp) {
        _Down = false;
        bool click = inside;
        _Over = false;
        return click;
    }
    if (!in.Touching && !in.TouchUp) _Down = false;
    return false;
}

void UIButton::Draw(Canvas& c) const {
    if (!Visible || Texture.Width == 0) return;
    int frame = 0;
    if (_Down && _Over) frame = 2;  // C#: MouseOver/MouseDown frames; touch shows the lit frame while held
    if (Selected) frame = 1;
    if (Disabled) frame = 3;
    frame = std::min(ImageStates - 1, frame);
    int w = Width(), h = Height();
    for (int y = 0; y < h; y++) {
        int dy = Y + y;
        if (dy < 0 || dy >= c.Height) continue;
        const uint32_t* src = &Texture.Px[(size_t)y * Texture.Width + frame * w];
        for (int x = 0; x < w; x++) {
            int dx = X + x;
            if (dx < 0 || dx >= c.Width) continue;
            Canvas::BlendPx(c.Px[(size_t)dy * c.Width + dx], src[x]);
        }
    }
}

}}
