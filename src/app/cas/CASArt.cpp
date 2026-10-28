/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims — Create-A-Sim art + strings (see CASArt.h). TS1(sims).
#include "app/cas/CASArt.h"
#include "files/FAR1/FAR1Archive.h"
#include "files/formats/iff/IffFile.h"
#include "files/formats/iff/chunks/STR.h"
#include "vitaboy/Texture.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <memory>

namespace simapp { namespace cas {

static std::string Lower(std::string s) { for (auto& c : s) c = (char)tolower((unsigned char)c); return s; }

Canvas CASArt::Resample(const Canvas& src, const Rect& from, int w, int h) {
    Canvas out(w, h);
    if (w <= 0 || h <= 0 || from.W <= 0 || from.H <= 0) return out;
    double sx = (double)from.W / w, sy = (double)from.H / h;
    for (int y = 0; y < h; y++) {
        double y0 = from.Y + y * sy, y1 = y0 + sy;
        for (int x = 0; x < w; x++) {
            double x0 = from.X + x * sx, x1 = x0 + sx;
            double a = 0, r = 0, g = 0, b = 0, wsum = 0;
            for (int iy = (int)y0; iy < (int)std::ceil(y1); iy++) {
                if (iy < 0 || iy >= src.Height) continue;
                double wy = std::min(y1, (double)iy + 1) - std::max(y0, (double)iy);
                if (wy <= 0) continue;
                for (int ix = (int)x0; ix < (int)std::ceil(x1); ix++) {
                    if (ix < 0 || ix >= src.Width) continue;
                    double wx = std::min(x1, (double)ix + 1) - std::max(x0, (double)ix);
                    if (wx <= 0) continue;
                    double wgt = wx * wy;
                    uint32_t p = src.Px[(size_t)iy * src.Width + ix];
                    double al = (p >> 24) / 255.0;
                    a += al * wgt;
                    r += (p & 255) * al * wgt; g += ((p >> 8) & 255) * al * wgt; b += ((p >> 16) & 255) * al * wgt;
                    wsum += wgt;
                }
            }
            if (wsum <= 0 || a <= 0) { out.Px[(size_t)y * w + x] = 0; continue; }
            int A = (int)std::lround(a / wsum * 255.0);
            out.Px[(size_t)y * w + x] = RGBA((int)std::lround(r / a), (int)std::lround(g / a), (int)std::lround(b / a), A);
        }
    }
    return out;
}

namespace {
// Decodes an 8-bit UI bitmap; magenta (255,0,255) is transparent.
bool Decode(const std::vector<uint8_t>& data, Canvas& out) {
    FSO::Vitaboy::Texture t;
    if (data.empty() || !t.LoadBMP(data)) return false;
    out.Resize(t.Width, t.Height);
    for (int i = 0; i < t.Width * t.Height; i++) {
        uint32_t p = t.Paletted ? t.Palette[t.Indices[(size_t)i]] : t.Pixels[(size_t)i];
        uint32_t rgb = p & 0x00FFFFFFu;
        out.Px[(size_t)i] = (rgb == 0x00FF00FFu) ? 0u : (rgb | 0xFF000000u);
    }
    return true;
}

struct Far {
    std::unique_ptr<FSO::Files::FAR1::FAR1Archive> Archive;
    std::map<std::string, FSO::Files::FAR1::FarEntry> Entries;
    bool Open(const std::string& path) {
        try { Archive.reset(new FSO::Files::FAR1::FAR1Archive(path, false)); } catch (...) { return false; }
        for (auto& e : Archive->GetAllFarEntries()) {
            std::string n = Lower(e.Filename);
            for (auto& c : n) if (c == '\\') c = '/';
            Entries[n] = e;
        }
        return !Entries.empty();
    }
    bool Get(const std::string& name, Canvas& out) {
        auto it = Entries.find(Lower(name));
        if (it == Entries.end()) return false;
        bool found = false;
        auto data = Archive->GetEntry(it->second, &found);
        return found && Decode(data, out);
    }
};

// cuts a strip of `frames` states and scales each by `scale`
CASSprite Strip(const Canvas& img, int frames, float scale) {
    CASSprite s;
    if (img.Width <= 0 || frames <= 0) return s;
    int fw = img.Width / frames;
    int w = std::max(1, (int)std::lround(fw * scale)), h = std::max(1, (int)std::lround(img.Height * scale));
    for (int i = 0; i < frames; i++) s.Frames.push_back(CASArt::Resample(img, Rect(i * fw, 0, fw, img.Height), w, h));
    return s;
}
}

void CASArt::LoadStrings(const std::string& gameData) {
    using FSO::Files::Formats::IFF::IffFile;
    using FSO::Files::Formats::IFF::Chunks::STR;
    try {
        IffFile ui(gameData + "/UIText.iff");
        for (int t : { 128, 129, 130, 152, 164 }) {
            auto s = ui.Get<STR>((uint16_t)t);
            if (!s) continue;
            for (int i = 0; i < s->Length() && i < 100; i++) _Strings[t * 1000 + i] = s->GetString(i);
        }
    } catch (...) {}
    try {
        IffFile live(gameData + "/Live.iff");
        if (auto s = live.Get<STR>(131))
            for (int i = 0; i < s->Length() && i < 64; i++) _Live131[i] = s->GetString(i);
    } catch (...) {}
}

std::string CASArt::Str(int table, int index, const char* fallback) const {
    auto it = _Strings.find(table * 1000 + index);
    return (it == _Strings.end() || it->second.empty()) ? std::string(fallback) : it->second;
}

std::string CASArt::Personality(int index, const char* fallback) const {
    auto it = _Live131.find(index);
    return (it == _Live131.end() || it->second.empty()) ? std::string(fallback) : it->second;
}

bool CASArt::Load(const std::string& dataRoot) {
    LoadStrings(dataRoot + "/GameData");
    TopBg.Resize(400, 240); BottomBg.Resize(320, 240);
    // fallback backgrounds (no UIGraphics): the CAS blue
    TopBg.GradientV(0, 0, 400, 240, RGBA(18, 40, 128), RGBA(0, 18, 86));
    BottomBg.GradientV(0, 0, 320, 240, RGBA(18, 40, 128), RGBA(0, 18, 86));
    ClusterScale = 0.5f;
    // cluster button positions in CreateACharBack (800x600), measured against the background's cells
    const Rect origin(392, 98, 168, 297);
    const Rect btn[7] = { Rect(397, 272, 98, 111), Rect(499, 272, 57, 121), Rect(499, 100, 57, 100), Rect(402, 104, 92, 90),
                          Rect(394, 201, 55, 71), Rect(449, 201, 46, 71), Rect(499, 201, 57, 71) };
    for (int i = 0; i < 7; i++)
        ClusterPos[i] = Rect((int)std::lround((btn[i].X - origin.X) * ClusterScale), (int)std::lround((btn[i].Y - origin.Y) * ClusterScale),
                             (int)std::lround(btn[i].W * ClusterScale), (int)std::lround(btn[i].H * ClusterScale));
    // Add / Delete / Edit on DsgnFamBkg (800x600) at 0.4
    const Rect fam[3] = { Rect(596, 102, 139, 103), Rect(596, 208, 149, 99), Rect(596, 311, 137, 99) };
    for (int i = 0; i < 3; i++)
        FamilyBtnPos[i] = Rect((int)std::lround(fam[i].X * 0.4), (int)std::lround(fam[i].Y * 0.4), (int)std::lround(fam[i].W * 0.4), (int)std::lround(fam[i].H * 0.4));

    Far far;
    if (!far.Open(dataRoot + "/UIGraphics/UIGraphics.far")) return false;
    Canvas charBack, famBack, img;
    if (!far.Get("nbhd/createacharback.bmp", charBack) || !far.Get("nbhd/dsgnfambkg.bmp", famBack)) return false;

    // backdrop texture: the panel-free bottom band of CreateACharBack, stretched (keeps the original grain and
    // bevelled screen frame on the left/right edges)
    TopBg = Resample(charBack, Rect(0, 524, 800, 76), 400, 240);
    BottomBg = Resample(charBack, Rect(0, 524, 800, 76), 320, 240);
    // the edges of the band are the screen frame's bottom bevel: give both screens a top bevel too
    for (auto* c : { &TopBg, &BottomBg }) {
        c->FillRect(0, 0, c->Width, 1, RGBA(120, 130, 210));
        c->FillRect(0, 1, c->Width, 1, RGBA(60, 76, 160));
    }
    FamilyBottom = Resample(famBack, Rect(0, 0, 800, 600), 320, 240);
    FamilyTop = Resample(famBack, Rect(66, 94, 526, 322), 400, 240);
    PreviewPanel = Resample(charBack, Rect(563, 102, 209, 293), 166, 233);
    Cluster = Resample(charBack, origin, (int)std::lround(origin.W * ClusterScale), (int)std::lround(origin.H * ClusterScale));
    if (far.Get("nbhd/designcharpersled.bmp", img)) LEDOn = Resample(img, Rect(0, 0, img.Width, img.Height), 86, 13);
    LEDOff = Resample(charBack, Rect(228, 131, 59, 25), 86, 13);
    if (far.Get("nbhd/cacpointremainingbars.bmp", img)) PointsOn = Resample(img, Rect(0, 0, img.Width, img.Height), 140, 13);
    PointsOff = Resample(charBack, Rect(115, 343, 149, 25), 140, 13);

    struct { const char* File; CASSprite* Out; int Frames; float Scale; } sprites[] = {
        { "nbhd/designcharmalebtn.bmp", &Male, 4, ClusterScale }, { "nbhd/designcharfemalebtn.bmp", &Female, 4, ClusterScale },
        { "nbhd/designcharadultbtn.bmp", &Adult, 4, ClusterScale }, { "nbhd/designcharchildbtn.bmp", &Child, 4, ClusterScale },
        { "nbhd/designcharlightbtn.bmp", &Light, 4, ClusterScale }, { "nbhd/designcharmediumbtn.bmp", &Medium, 4, ClusterScale },
        { "nbhd/designchardarkbtn.bmp", &Dark, 4, ClusterScale },
        { "nbhd/designcharskinsleftbtn.bmp", &ArrowL, 4, 0.8f }, { "nbhd/designcharskinsrightbtn.bmp", &ArrowR, 4, 0.8f },
        { "nbhd/designchardonebtn.bmp", &SimDone, 6, 0.6f }, { "nbhd/designcharcancelbtn.bmp", &SimCancel, 6, 0.6f },
        { "nbhd/designfamdone.bmp", &FamDone, 6, 0.5f }, { "nbhd/designfamcancel.bmp", &FamCancel, 6, 0.5f },
        { "nbhd/addpersonbtn.bmp", &Add, 4, 0.4f }, { "nbhd/deletepersonbtn.bmp", &Delete, 4, 0.4f },
        { "nbhd/editpersonbtn.bmp", &Edit, 4, 0.4f },
    };
    for (auto& s : sprites) if (far.Get(s.File, img)) *s.Out = Strip(img, s.Frames, s.Scale);
    HaveArt = true;
    return true;
}

}}
