/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// simshot — render what the translated FreeSims engine has loaded, as a game-view screenshot.
//   simshot <gameDataDir> <cacheDir> <House.iff> <floors.iff> <out.png> [ticks]
// Placement follows FreeSims' World renderer at Medium zoom, rotation TopLeft:
//   tile -> screen: x=(tx-ty)*32, y=(tx+ty)*16  (GetScreenFromTile)
//   floors: SPR2 (pattern+256) frame 0 at cadge offset (3,158)            (FloorComponent, FLOORDEST_MED)
//   objects: DGRP image (direction, zoom 2, rotation 0); each sprite at
//            (CadgeWidth/2 + off.X, CadgeBaseLine - h + off.Y), CadgeWidth=68, CadgeBaseLine=174 (DGRPRenderer)
// Walls are drawn "walls down" style as short shaded stubs (WallComponent's textured walls aren't ported yet).
// Depth uses painter's order per tile instead of the GPU z-buffer.
#include "content/ContentManager.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"
#include "simantics/VMArchitecture.h"
#include "simantics/utils/VMWorldActivator.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMGameObject.h"
#include "simantics/entities/VMAvatar.h"
#include "files/formats/iff/IffFile.h"
#include "files/formats/iff/chunks/DGRP.h"
#include "files/formats/iff/chunks/SPR2.h"
#include "files/formats/iff/chunks/OBJD.h"
#include "core/png.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <map>
using namespace FSO::SimAntics;
using namespace FSO::Files::Formats::IFF;
using namespace FSO::Files::Formats::IFF::Chunks;

struct Img {
    int w, h; std::vector<uint8_t> px; // RGBA
    Img(int W, int H) : w(W), h(H), px((size_t)W * H * 4, 0) {}
    void put(int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        if (x < 0 || y < 0 || x >= w || y >= h || a == 0) return;
        uint8_t* d = &px[((size_t)y * w + x) * 4];
        if (a == 255 || d[3] == 0) { d[0]=r; d[1]=g; d[2]=b; d[3]=255; return; }
        d[0] = (uint8_t)((r * a + d[0] * (255 - a)) / 255); d[1] = (uint8_t)((g * a + d[1] * (255 - a)) / 255);
        d[2] = (uint8_t)((b * a + d[2] * (255 - a)) / 255);
    }
};

static void blitFrame(Img& img, SPR2Frame* f, int x0, int y0, bool flip) {
    if (!f) return;
    f->DecodeIfRequired();
    if (!f->HasColor()) return;
    int w = f->Width, h = f->Height;
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        auto c = f->ColorAt((size_t)y * w + x); // FIX(sims): palettised frames
        img.put(x0 + (flip ? (w - 1 - x) : x), y0 + y, c.R, c.G, c.B, c.A);
    }
}

int main(int argc, char** argv) {
    if (argc < 6) { printf("usage: simshot <gameDataDir> <cacheDir> <House.iff> <floors.iff> <out.png> [ticks]\n"); return 2; }
    int ticks = argc > 6 ? atoi(argv[6]) : 120;
    FSO::Content::Content::Init(argv[1], argv[2]);
    VM::UseWorld(false);
    VMContext ctx(nullptr);
    Model::VMNullHeadlineProvider hp;
    VM vm(&ctx, &hp);
    vm.Init();
    vm.OnDialog = [](Model::VMDialogInfo&) {};
    IffFile house(argv[3]);
    VMWorldActivator act(&vm, nullptr);
    act.LoadFromIff(&house);
    for (int i = 0; i < ticks; i++) vm.Update();
    IffFile floors(argv[4]);
    auto arch = ctx.Architecture();
    int W = arch->Width, H = arch->Height;
    printf("lot %dx%d, entities %zu\n", W, H, vm.Entities.size());

    const int TWH = 32, THH = 16;              // medium zoom tile half sizes
    const int CW2 = 34, CBL = 174;             // CadgeWidth/2, CadgeBaseLine (medium)
    int imgW = (W + H) * TWH + 400, imgH = (W + H) * THH + 600;
    int ox = H * TWH + 200, oy = 400;          // screen origin of tile (0,0)
    Img img(imgW, imgH);
    auto scr = [&](float tx, float ty, int& sx, int& sy) { sx = ox + (int)std::lround((tx - ty) * TWH); sy = oy + (int)std::lround((tx + ty) * THH); };

    // --- terrain + floors (level 1) ---
    auto& fl = arch->Floors[0];
    std::map<uint16_t,int> patternCount;
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        int sx, sy; scr((float)x, (float)y, sx, sy);
        uint16_t p = fl[(size_t)y * W + x].Pattern;
        patternCount[p]++;
        SPR2* spr = nullptr;
        if (p != 0 && p < 256) spr = floors.Get<SPR2>((uint16_t)(p + 256));
        if (spr && !spr->Frames.empty()) { blitFrame(img, spr->Frames[0].get(), sx - CW2 + 3, sy - CBL + 158, false); continue; }
        // grass (pattern 0) or a custom .flr pattern we don't have: flat diamond
        uint8_t r = 72, g = 118, b = 54;
        if (p == 65535 || p == 65534) { r = 70; g = 150; b = 210; }          // pool / water (FloorComponent draws these specially)
        else if (p != 0) { r = (uint8_t)(150 + (p * 53) % 90); g = (uint8_t)(130 + (p * 97) % 90); b = (uint8_t)(100 + (p * 29) % 110); }
        for (int dy = 0; dy <= 2 * THH; dy++) { int span = TWH * (THH - std::abs(dy - THH)) / THH;
            for (int dx = -span; dx <= span; dx++) img.put(sx + dx, sy + dy, r, g, b, 255); }
    }

    // --- walls + objects, painter's order by tile depth ---
    std::multimap<int, VMEntity*> byDepth;
    for (auto e : vm.Entities) {
        if (dynamic_cast<VMAvatar*>(e)) continue;
        auto pos = e->Position();
        if (getenv("SHOT_DEBUG")) printf("ent #%d %-34s pos(%d,%d,L%d) dir=%d gfx=%d\n", e->ObjectID, e->ToString().c_str(), pos.x, pos.y, pos.Level, (int)e->Direction(), (int)e->GetValue(Model::VMStackObjectVariable::Graphic));
        if (pos.x < 0 || pos.y < 0 || pos.Level != 1) continue;
        int tx = pos.x / 16, ty = pos.y / 16;
        byDepth.insert({ (tx + ty) * 4096 + tx, e });
    }
    auto& walls = arch->Walls[0];
    auto wallStub = [&](int ax, int ay, int bx, int by, int hgt, uint8_t shade) {
        int steps = std::max(std::abs(bx - ax), std::abs(by - ay)); if (steps < 1) steps = 1;
        for (int i = 0; i <= steps; i++) { int x = ax + (bx - ax) * i / steps, y = ay + (by - ay) * i / steps;
            for (int k = 0; k <= hgt; k++) { bool cap = k >= hgt - 1; img.put(x, y - k, cap ? 230 : shade, cap ? 225 : (uint8_t)(shade - 6), cap ? 214 : (uint8_t)(shade - 18), 255); } }
    };
    auto it = byDepth.begin();
    int drawnObjs = 0, drawnSprites = 0;
    for (int depth = 0; depth <= W + H; depth++) {
        for (int tx = 0; tx <= depth; tx++) { int ty = depth - tx; if (tx >= W || ty >= H) continue;
            auto& wt = walls[(size_t)ty * W + tx];
            int sx, sy; scr((float)tx, (float)ty, sx, sy);
            const int WH = 22; // "walls down" stub height at medium zoom
            if ((int)wt.Segments & 1) wallStub(sx - TWH, sy + THH, sx, sy, WH, 196);   // TopLeft
            if ((int)wt.Segments & 2) wallStub(sx, sy, sx + TWH, sy + THH, WH, 170);   // TopRight
        }
        for (; it != byDepth.end() && it->first / 4096 <= depth; ++it) {
            VMEntity* e = it->second;
            if (!e->Object || !e->Object->OBJ || !e->Object->Resource) continue;
            auto pos = e->Position(); int tx = pos.x / 16, ty = pos.y / 16;
            uint16_t graphic = (uint16_t)e->GetValue(Model::VMStackObjectVariable::Graphic);
            auto dgrp = e->Object->Resource->Get<DGRP>((uint16_t)(e->Object->OBJ->BaseGraphicID + graphic));
            if (!dgrp) { if (getenv("SHOT_DEBUG")) printf("  no DGRP for #%d (base %u + gfx %u)\n", e->ObjectID, e->Object->OBJ->BaseGraphicID, graphic); continue; }
            auto image = dgrp->GetImage((uint32_t)e->Direction(), 2u, 0u);
            if (!image) { if (getenv("SHOT_DEBUG")) { printf("  no image #%d dir %d; images:", e->ObjectID, (int)e->Direction()); for (auto& im : dgrp->Images) printf(" (d%u z%u)", im.Direction, im.Zoom); printf("\n"); } continue; }
            int sx, sy; scr((float)tx, (float)ty, sx, sy);
            uint16_t dynBase = e->Object->OBJ->DynamicSpriteBaseId;
            for (auto& sp : image->Sprites) {
                if (dynBase != 0 && sp.SpriteID >= dynBase) {
                    uint32_t di = sp.SpriteID - dynBase;
                    bool vis = di > 63 ? ((e->DynamicSpriteFlags2 >> (di - 64)) & 1) : ((e->DynamicSpriteFlags >> di) & 1);
                    if (!vis) continue;
                }
                auto spr = e->Object->Resource->Get<SPR2>((uint16_t)sp.SpriteID);
                if (!spr || sp.SpriteFrameIndex >= spr->Frames.size()) continue;
                SPR2Frame* f = spr->Frames[sp.SpriteFrameIndex].get();
                f->DecodeIfRequired();
                int x0 = sx - CW2 + (CW2 + (int)sp.SpriteOffset.X);
                int y0 = sy - CBL + (CBL - f->Height + (int)sp.SpriteOffset.Y);
                blitFrame(img, f, x0, y0, sp.Flip());
                drawnSprites++;
            }
            drawnObjs++;
        }
    }
    printf("drew %d objects (%d sprites); floor patterns:", drawnObjs, drawnSprites);
    for (auto& kv : patternCount) printf(" %u:%d", kv.first, kv.second);
    printf("\n");
    // flatten onto a background and write
    for (size_t i = 0; i < img.px.size(); i += 4) if (img.px[i + 3] == 0) { img.px[i] = 24; img.px[i + 1] = 26; img.px[i + 2] = 34; img.px[i + 3] = 255; }
    if (!st::writePNG(argv[5], img.w, img.h, img.px.data())) { printf("png write failed\n"); return 1; }
    printf("wrote %s (%dx%d)\n", argv[5], img.w, img.h);
    return 0;
}
