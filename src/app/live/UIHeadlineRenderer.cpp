/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims app — see UIHeadlineRenderer.h (FreeSims UI/Panels/WorldUI/UIHeadlineRenderer.cs)
#include "app/live/UIHeadlineRenderer.h"
#include "files/formats/iff/IffFile.h"
#include "files/formats/iff/chunks/SPR.h"
#include <map>
#include <memory>
#include <cstdio>

namespace simapp {

using namespace FSO::SimAntics::Model;
using FSO::SimAntics::Primitives::VMSetBalloonHeadlineOperandGroup;
using FSO::Files::Formats::IFF::IffFile;
using FSO::Files::Formats::IFF::Chunks::SPR;

const int UIHeadlineRenderer::GroupOffsets[10] = {
    0x000,
    0x064,
    0x190,
    0x0C8,
    0x12C,
    0x1F4,
    0x258,
    0x000, //algorithmic
    0x2BC,
    0x320
};
const int UIHeadlineRenderer::ZoomToDiv[4] = { 0, 4, 2, 1 };

std::function<const Canvas*(FSO::SimAntics::VMEntity*)> UIHeadlineRenderer::IconProvider;

static std::unique_ptr<IffFile> Sprites;
// ADDED(sims): decoded SPR# frames (C# SPRFrame.GetTexture), key = sprite id << 8 | frame
static std::map<uint32_t, std::unique_ptr<Canvas>> FrameCache;

void UIHeadlineRenderer::LoadSprites(const std::string& gameDataDir) {
    if (Sprites) return;
    const char* names[] = { "/Sprites.iff", "/sprites.iff", "/SPRITES.IFF" };
    for (auto n : names) {
        std::string p = gameDataDir + n;
        FILE* f = fopen(p.c_str(), "rb");
        if (!f) continue;
        fclose(f);
        try { Sprites = IffFile::OpenReadOnly(p); } catch (...) { Sprites.reset(); } // TS1(sims): balloons read on first use
        if (Sprites) return;
    }
    printf("[ui] Sprites.iff not found: no balloons over Sims\n");
}

void UIHeadlineRenderer::Unload() { FrameCache.clear(); Sprites.reset(); }

static const Canvas* GetFrame(int sprID, int frame) {
    if (!Sprites || sprID < 0) return nullptr;
    uint32_t key = ((uint32_t)sprID << 8) | (uint32_t)(frame & 255);
    auto it = FrameCache.find(key);
    if (it != FrameCache.end()) return it->second.get();
    std::unique_ptr<Canvas> c;
    auto spr = Sprites->Get<SPR>((uint16_t)sprID);
    if (spr != nullptr && frame >= 0 && frame < (int)spr->Frames.size()) {
        auto& f = spr->Frames[(size_t)frame];
        auto data = f->ExpandColors(); // FIX(sims): palettised frames
        if (f->Width > 0 && f->Height > 0 && data.size() >= (size_t)f->Width * f->Height) {
            c.reset(new Canvas(f->Width, f->Height));
            for (size_t i = 0; i < data.size() && i < c->Px.size(); i++) c->Px[i] = data[i].PackedValue();
        }
    }
    auto res = c.get();
    FrameCache[key] = std::move(c);
    return res;
}

static int FrameCount(int sprID) {
    if (!Sprites || sprID < 0) return 0;
    auto spr = Sprites->Get<SPR>((uint16_t)sprID);
    return spr ? (int)spr->Frames.size() : 0;
}

UIHeadlineRenderer::UIHeadlineRenderer(VMRuntimeHeadline* headline) : VMHeadlineRenderer(headline) {
    if (Headline->Operand.Group != VMSetBalloonHeadlineOperandGroup::Algorithmic)
        SpriteID = GroupOffsets[(int)Headline->Operand.Group] + Headline->Index;
    if (Headline->Operand.Type != 255 && Headline->Operand.Type != 3)
        BGSpriteID = GroupOffsets[(int)VMSetBalloonHeadlineOperandGroup::Balloon] + Headline->Operand.Type;
    if (FrameCount(SpriteID) == 0) SpriteID = -1;
    if (FrameCount(BGSpriteID) == 0) BGSpriteID = -1;
}

void UIHeadlineRenderer::Size(int zoom, int& w, int& h) {
    w = h = 0;
    int zoomFrame = 3 - zoom;
    const Canvas* big = (BGSpriteID >= 0) ? GetFrame(BGSpriteID, zoomFrame) : GetFrame(SpriteID, zoomFrame);
    if (big != nullptr) { w = big->Width; h = big->Height; }
    else if (zoom != 1 && IconProvider) { // algorithmic without a balloon
        w = h = 24 * zoom / 2;
    }
}

void UIHeadlineRenderer::Draw(Canvas& c, int cx, int cy, int zoom) {
    if (zoom < 1 || zoom > 3) return;
    int zoomFrame = 3 - zoom;
    int w = 0, h = 0;
    Size(zoom, w, h);
    if (w <= 0) return;
    int x0 = cx - w / 2, y0 = cy - h / 2;

    if (BGSpriteID >= 0) {
        auto bg = GetFrame(BGSpriteID, zoomFrame);
        if (bg) c.Blit(bg->Px.data(), bg->Width, bg->Height, x0, y0, true);
    }

    int offX = 0, offY = 0;
    if (SpriteID >= 0) {
        int n = FrameCount(SpriteID);
        int animFrame = (n >= 3) ? (Headline->Anim / 15) % (n / 3) : 0;
        auto main = GetFrame(SpriteID, zoomFrame + animFrame * 3);
        offY = 4 / ZoomToDiv[zoom];
        if (main) c.Blit(main->Px.data(), main->Width, main->Height, x0 + w / 2 - main->Width / 2 + offX, y0 + h / 2 - main->Height / 2 + offY, true);
    } else if (Headline->Operand.Group == VMSetBalloonHeadlineOperandGroup::Algorithmic && zoom != 1 && IconProvider) {
        // C#: AlgTex = IconTarget.GetIcon(gd, zoom - 1); offset (0, -6) / ZoomToDiv
        auto icon = IconProvider(Headline->IconTarget);
        if (icon != nullptr && icon->Width > 0) {
            offY = -6 / ZoomToDiv[zoom];
            // TS1 catalog icons are ~ the balloon interior; medium zoom draws them at half size
            int iw = (zoom == 3) ? icon->Width : icon->Width / 2, ih = (zoom == 3) ? icon->Height : icon->Height / 2;
            c.BlitScaled(icon->Px.data(), icon->Width, icon->Height, Rect(x0 + w / 2 - iw / 2, y0 + h / 2 - ih / 2 + offY, iw, ih), true);
        }
    }

    if (Headline->Operand.Crossed()) {
        auto cross = GetFrame(67, zoomFrame);
        if (cross) c.Blit(cross->Px.data(), cross->Width, cross->Height, x0 + w / 2 - cross->Width / 2, y0 + h / 2 - cross->Height / 2, true);
    }
}

}
