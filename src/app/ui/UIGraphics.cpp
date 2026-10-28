/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims app — see UIGraphics.h
#include "app/ui/UIGraphics.h"
#include "files/FAR1/FAR1Archive.h"
#include "vitaboy/Texture.h"
#include <algorithm>
#include <cctype>
#include <cstdio>

namespace simapp {

UIGraphics& UIGraphics::Get() {
    static UIGraphics instance;
    return instance;
}

UIGraphics::~UIGraphics() {}

std::string UIGraphics::Key(const std::string& path) {
    std::string k = path;
    for (auto& ch : k) { ch = (char)std::tolower((unsigned char)ch); if (ch == '\\') ch = '/'; }
    return k;
}

void UIGraphics::Init(const std::string& dataRoot) {
    if (_Far) return;
    const char* candidates[] = { "/UIGraphics/UIGraphics.far", "/uigraphics/uigraphics.far", "/UIGraphics/uigraphics.far",
                                 "/GameData/UIGraphics/UIGraphics.far" };
    for (auto c : candidates) {
        std::string p = dataRoot + c;
        FILE* f = fopen(p.c_str(), "rb");
        if (!f) continue;
        fclose(f);
        try {
            _Far.reset(new FSO::Files::FAR1::FAR1Archive(p, false));
        } catch (...) { _Far.reset(); continue; }
        auto& entries = _Far->GetAllFarEntries();
        for (size_t i = 0; i < entries.size(); i++) _Index[Key(entries[i].Filename)] = (int)i;
        printf("[ui] UIGraphics: %s (%zu images)\n", p.c_str(), entries.size());
        return;
    }
    printf("[ui] UIGraphics.far not found: the control panel uses the built-in look\n");
}

// ---------------------------------------------------------------- decoders

static void KeyMagenta(Canvas& out) {
    // UIGraphicsProvider.MASK_COLORS: (255,0,255), (254,2,254), (255,1,255); 16-bit sourced BMPs give (248,0,248)
    for (auto& p : out.Px) {
        uint32_t r = p & 255, g = (p >> 8) & 255, b = (p >> 16) & 255;
        if (r >= 248 && g <= 4 && b >= 248) p = 0;
    }
}

bool UIGraphics::DecodeBMP(const std::vector<uint8_t>& data, Canvas& out) {
    if (data.size() < 54) return false;
    FSO::Vitaboy::Texture tex;
    if (!tex.LoadBMP(data) || tex.Width <= 0 || tex.Height <= 0) return false;
    out.Resize(tex.Width, tex.Height);
    for (int y = 0; y < tex.Height; y++)
        for (int x = 0; x < tex.Width; x++) out.Px[(size_t)y * tex.Width + x] = tex.Fetch(x, y) | 0xFF000000u;
    KeyMagenta(out);
    return true;
}

bool UIGraphics::DecodeTGA(const std::vector<uint8_t>& d, Canvas& out) {
    if (d.size() < 18) return false;
    int idLen = d[0], cmapType = d[1], type = d[2];
    int w = d[12] | (d[13] << 8), h = d[14] | (d[15] << 8), bpp = d[16], desc = d[17];
    if (cmapType != 0 || (type != 2 && type != 10) || (bpp != 24 && bpp != 32) || w <= 0 || h <= 0) return false;
    size_t p = 18 + (size_t)idLen;
    int bytes = bpp / 8;
    std::vector<uint32_t> px((size_t)w * h);
    auto read = [&](uint32_t& c) -> bool {
        if (p + bytes > d.size()) return false;
        uint32_t b = d[p], g = d[p + 1], r = d[p + 2], a = bytes == 4 ? d[p + 3] : 255;
        p += bytes;
        c = r | (g << 8) | (b << 16) | (a << 24);
        return true;
    };
    size_t i = 0, n = px.size();
    if (type == 2) { while (i < n) { if (!read(px[i])) return false; i++; } }
    else {
        while (i < n) {
            if (p >= d.size()) return false;
            int hdr = d[p++], cnt = (hdr & 127) + 1;
            if (hdr & 128) { uint32_t c; if (!read(c)) return false; for (int k = 0; k < cnt && i < n; k++) px[i++] = c; }
            else for (int k = 0; k < cnt && i < n; k++) if (!read(px[i++])) return false;
        }
    }
    out.Resize(w, h);
    bool topDown = (desc & 0x20) != 0;
    for (int y = 0; y < h; y++) {
        int sy = topDown ? y : h - 1 - y;
        std::copy(px.begin() + (size_t)sy * w, px.begin() + (size_t)sy * w + w, out.Px.begin() + (size_t)y * w);
    }
    if (bpp == 24) KeyMagenta(out);
    return true;
}

void UIGraphics::Resample(const Canvas& src, const Rect& sr, Canvas& out, int w, int h) {
    out.Resize(w, h);
    if (sr.W <= 0 || sr.H <= 0 || w <= 0 || h <= 0) return;
    // box filter in 1/256 source pixels, premultiplied alpha
    for (int y = 0; y < h; y++) {
        int64_t fy0 = (int64_t)y * sr.H * 256 / h, fy1 = (int64_t)(y + 1) * sr.H * 256 / h;
        for (int x = 0; x < w; x++) {
            int64_t fx0 = (int64_t)x * sr.W * 256 / w, fx1 = (int64_t)(x + 1) * sr.W * 256 / w;
            uint64_t sa = 0, sr_ = 0, sg = 0, sb = 0, area = 0;
            for (int64_t yy = fy0 >> 8; yy <= ((fy1 - 1) >> 8); yy++) {
                int64_t cy = std::min<int64_t>(fy1, (yy + 1) * 256) - std::max<int64_t>(fy0, yy * 256);
                if (cy <= 0) continue;
                const uint32_t* row = &src.Px[(size_t)(sr.Y + yy) * src.Width + sr.X];
                for (int64_t xx = fx0 >> 8; xx <= ((fx1 - 1) >> 8); xx++) {
                    int64_t cx = std::min<int64_t>(fx1, (xx + 1) * 256) - std::max<int64_t>(fx0, xx * 256);
                    if (cx <= 0) continue;
                    uint64_t wgt = (uint64_t)(cx * cy);
                    uint32_t p = row[xx];
                    uint64_t a = p >> 24;
                    sa += a * wgt; sr_ += (p & 255) * a * wgt; sg += ((p >> 8) & 255) * a * wgt; sb += ((p >> 16) & 255) * a * wgt;
                    area += wgt;
                }
            }
            if (area == 0 || sa == 0) { out.Px[(size_t)y * w + x] = 0; continue; }
            uint32_t a = (uint32_t)(sa / area);
            uint32_t r = (uint32_t)(sr_ / sa), g = (uint32_t)(sg / sa), b = (uint32_t)(sb / sa);
            out.Px[(size_t)y * w + x] = r | (g << 8) | (b << 16) | (a << 24);
        }
    }
}

// ---------------------------------------------------------------- cache

const Canvas* UIGraphics::Image(const std::string& path) {
    std::string k = Key(path);
    auto it = _Images.find(k);
    if (it != _Images.end()) return it->second.get();
    std::unique_ptr<Canvas> img;
    if (_Far) {
        auto ie = _Index.find(k);
        if (ie != _Index.end()) {
            auto& entry = _Far->GetAllFarEntries()[(size_t)ie->second];
            bool found = false;
            auto data = _Far->GetEntry(entry, &found);
            img.reset(new Canvas());
            bool ok = false;
            if (k.size() > 4 && k.compare(k.size() - 4, 4, ".tga") == 0) ok = DecodeTGA(data, *img);
            else ok = DecodeBMP(data, *img);
            if (!ok) img.reset();
        }
    }
    auto res = img.get();
    _Images[k] = std::move(img);
    return res;
}

const Canvas* UIGraphics::Scaled(const std::string& path, const Rect& src, int w, int h) {
    char buf[96];
    snprintf(buf, sizeof buf, "|%d,%d,%d,%d>%dx%d", src.X, src.Y, src.W, src.H, w, h);
    std::string k = Key(path) + buf;
    auto it = _Scaled.find(k);
    if (it != _Scaled.end()) return it->second.get();
    auto img = Image(path);
    std::unique_ptr<Canvas> out;
    if (img != nullptr && w > 0 && h > 0) {
        Rect sr = src.W > 0 ? src : Rect(0, 0, img->Width, img->Height);
        sr.W = std::min(sr.W, img->Width - sr.X); sr.H = std::min(sr.H, img->Height - sr.Y);
        out.reset(new Canvas());
        if (sr.W == w && sr.H == h) {
            out->Resize(w, h);
            for (int y = 0; y < h; y++) std::copy(&img->Px[(size_t)(sr.Y + y) * img->Width + sr.X], &img->Px[(size_t)(sr.Y + y) * img->Width + sr.X] + w, &out->Px[(size_t)y * w]);
        } else Resample(*img, sr, *out, w, h);
    }
    auto res = out.get();
    _Scaled[k] = std::move(out);
    return res;
}

const Canvas* UIGraphics::Frame(const std::string& path, int frames, int frame, int w, int h) {
    auto img = Image(path);
    if (img == nullptr || frames <= 0) return nullptr;
    int fw = img->Width / frames;
    frame = std::max(0, std::min(frames - 1, frame));
    return Scaled(path, Rect(fw * frame, 0, fw, img->Height), w, h);
}

bool UIGraphics::DrawButton(Canvas& c, const std::string& path, ButtonState st, const Rect& r, int frames) {
    auto f = Frame(path, frames, (int)st, r.W, r.H);
    if (f == nullptr) return false;
    c.Blit(f->Px.data(), f->Width, f->Height, r.X, r.Y, true);
    return true;
}

bool UIGraphics::DrawButtonFit(Canvas& c, const std::string& path, ButtonState st, const Rect& r, int frames) {
    auto img = Image(path);
    if (img == nullptr || frames <= 0) return false;
    int fw = img->Width / frames, fh = img->Height;
    if (fw <= 0 || fh <= 0) return false;
    float sc = std::min((float)r.W / fw, (float)r.H / fh);
    int w = std::max(1, (int)(fw * sc)), h = std::max(1, (int)(fh * sc));
    return DrawButton(c, path, st, Rect(r.X + (r.W - w) / 2, r.Y + (r.H - h) / 2, w, h), frames);
}

bool UIGraphics::DrawImage(Canvas& c, const std::string& path, const Rect& r, const Rect& src) {
    auto f = Scaled(path, src, r.W, r.H);
    if (f == nullptr) return false;
    c.Blit(f->Px.data(), f->Width, f->Height, r.X, r.Y, true);
    return true;
}

const Canvas* UIGraphics::Cursor(const std::string& path, int& hotX, int& hotY) {
    std::string k = Key(path);
    auto it = _Cursors.find(k);
    if (it == _Cursors.end()) {
        Cur cur;
        auto ie = _Far ? _Index.find(k) : _Index.end();
        if (_Far && ie != _Index.end()) {
            auto data = _Far->GetEntry(_Far->GetAllFarEntries()[(size_t)ie->second]);
            auto u16 = [&](size_t o) { return (int)(data[o] | (data[o + 1] << 8)); };
            auto u32 = [&](size_t o) { return (uint32_t)(data[o] | (data[o + 1] << 8) | (data[o + 2] << 16) | ((uint32_t)data[o + 3] << 24)); };
            if (data.size() >= 22 && u16(2) == 2 && u16(4) >= 1) {
                int w = data[6] ? data[6] : 256, h = data[7] ? data[7] : 256;
                cur.HX = u16(10); cur.HY = u16(12);
                size_t o = u32(18);
                if (o + 40 <= data.size() && u16(o + 14) == 1) {
                    // monochrome DIB: header, 2-colour palette, XOR rows then AND rows (bottom-up, 4-byte aligned)
                    size_t pal = o + u32(o), xorOff = pal + 8;
                    size_t stride = (size_t)((w + 31) / 32) * 4, andOff = xorOff + stride * (size_t)h;
                    if (andOff + stride * (size_t)h <= data.size()) {
                        cur.Img.reset(new Canvas(w, h));
                        for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
                            size_t row = (size_t)(h - 1 - y) * stride;
                            int xr = (data[xorOff + row + (size_t)(x >> 3)] >> (7 - (x & 7))) & 1;
                            int an = (data[andOff + row + (size_t)(x >> 3)] >> (7 - (x & 7))) & 1;
                            uint32_t c = 0;
                            if (!an) c = xr ? RGBA(255, 255, 255) : RGBA(0, 0, 0);
                            else if (xr) c = RGBA(0, 0, 0, 200); // "invert" pixels: dark
                            cur.Img->Px[(size_t)y * w + x] = c;
                        }
                    }
                }
            }
        }
        it = _Cursors.emplace(k, std::move(cur)).first;
    }
    hotX = it->second.HX; hotY = it->second.HY;
    return it->second.Img.get();
}

}
