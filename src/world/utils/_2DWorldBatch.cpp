/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.LotView.Utils._2DWorldBatch — software translation, see _2DWorldBatch.h
#include "world/utils/_2DWorldBatch.h"
#include "world/WorldState.h"
#include "files/formats/iff/chunks/SPR.h"
#include "files/formats/iff/chunks/SPR2.h"
#include <climits>
#include <type_traits>
#include <cmath>
#include <algorithm>

namespace FSO { namespace LotView { namespace Utils {

using FSO::Files::Formats::IFF::Chunks::SPRFrame;
using FSO::Files::Formats::IFF::Chunks::SPR2Frame;

_2DWorldBatch::_2DWorldBatch() {}

void _2DWorldBatch::OffsetTile(const st::Vector3& tileOffset) {
    this->TileOffset = tileOffset;
    this->WorldOffset = WorldSpace::GetWorldFromTile(tileOffset);
}

_2DSprite* _2DWorldBatch::NewSprite(_2DBatchRenderMode mode) {
    if (SpriteIndex >= SpritePool.size()) {
        SpritePool.emplace_back(new _2DSprite());
        auto spr = SpritePool.back().get();
        spr->RenderMode = mode;
        SpriteIndex++;
        return spr;
    } else {
        auto spr = SpritePool[SpriteIndex++].get();
        spr->Repurpose();
        spr->RenderMode = mode;
        return spr;
    }
}

void _2DWorldBatch::Draw(_2DSprite* sprite) {
    auto x = sprite->DestRect.X;
    auto y = sprite->DestRect.Y;
    auto width = sprite->DestRect.Width;
    auto height = sprite->DestRect.Height;

    sprite->AbsoluteDestRect = st::Rectangle((int32_t)(x + PxOffset.X), (int32_t)(y + PxOffset.Y), width, height);
    sprite->AbsoluteWorldPosition = sprite->WorldPosition + WorldOffset;
    sprite->AbsoluteTilePosition = sprite->TilePosition + TileOffset;
    sprite->ObjectID = ObjectID;
    sprite->DrawOrder = DrawOrder;

    // C# (FSOEnvironment.SoftwareDepth == false): one sprite group, lists per render mode.
    Sprites[(int)sprite->RenderMode].push_back(sprite);
    DrawOrder++;
}

void _2DWorldBatch::Begin(WorldCamera* worldCamera) {
    this->WorldCamera_ = worldCamera;
    for (auto& l : Sprites) l.clear();
    SpriteIndex = 0;
    this->DrawOrder = 0;
}

st::Vector3 _2DWorldBatch::FrontDirForRot(WorldRotation rot) {
    switch (rot) {
        case WorldRotation::TopLeft: return st::Vector3(3, 0, 3);
        case WorldRotation::TopRight: return st::Vector3(3, 0, -3);
        case WorldRotation::BottomRight: return st::Vector3(-3, 0, -3);
        case WorldRotation::BottomLeft: return st::Vector3(-3, 0, 3);
    }
    return st::Vector3(3, 0, 3);
}

st::Vector3 _2DWorldBatch::BackOffForRot(WorldRotation rot) {
    switch (rot) {
        case WorldRotation::TopLeft: return st::Vector3(0, 0, 0);
        case WorldRotation::TopRight: return st::Vector3(0, 0, 3);
        case WorldRotation::BottomRight: return st::Vector3(3, 0, 3);
        case WorldRotation::BottomLeft: return st::Vector3(3, 0, 0);
    }
    return st::Vector3(0, 0, 0);
}

void _2DWorldBatch::End() {
    SpritesDrawn = 0;
    if (WorldCamera_ == nullptr || Target == nullptr) { for (auto& l : Sprites) l.clear(); return; }
    // C# draw order inside one sprite group: NO_DEPTH, Z_BUFFER, WALL, RESTORE_DEPTH (scroll buffers are
    // composited by World2D directly, see World2D.cpp).
    static const int order[3] = { (int)_2DBatchRenderMode::NO_DEPTH, (int)_2DBatchRenderMode::Z_BUFFER, (int)_2DBatchRenderMode::WALL };
    for (int m : order) {
        for (auto spr : Sprites[m]) RenderSprite(spr);
        SpritesDrawn += (int32_t)Sprites[m].size();
    }
    for (auto& l : Sprites) l.clear();
}

st::Rectangle _2DWorldBatch::GetSpriteListBounds() {
    int smallX = INT_MAX, smallY = INT_MAX, bigX = INT_MIN, bigY = INT_MIN;
    for (auto& l : Sprites) for (auto sprite : l) {
        auto rect = sprite->AbsoluteDestRect;
        if (rect.X < smallX) smallX = rect.X;
        if (rect.Y < smallY) smallY = rect.Y;
        if (rect.X + rect.Width > bigX) bigX = rect.X + rect.Width;
        if (rect.Y + rect.Height > bigY) bigY = rect.Y + rect.Height;
    }
    return st::Rectangle(smallX, smallY, bigX - smallX, bigY - smallY);
}

// Point sampling of a texture with the sprite's (normalised) texture coordinates, like the CLAMP/POINT
// samplers of 2DWorldBatch.fx: destination pixel i of n samples at t = start + (i + 0.5) / n * span (span is
// negative when flipped), the pixel texture at floor(t) and any other texture at floor(t / texSize * otherSize).
static void BuildLookup(std::vector<int32_t>& outPx, std::vector<int32_t>& outOther1, std::vector<int32_t>& outOther2,
                        int32_t n, int32_t srcStart, int32_t srcLen, bool flip, int32_t texSize, int32_t size1, int32_t size2) {
    outPx.resize((size_t)n);
    outOther1.resize((size_t)n);
    outOther2.resize((size_t)n);
    if (texSize <= 0) texSize = 1;
    if (n == srcLen && size1 == texSize && size2 == texSize) {
        // FSO_CPP: unscaled sprite with same-size z-buffer/mask (the usual case): the sampling is the identity
        for (int32_t i = 0; i < n; i++) {
            int32_t p = flip ? (srcStart + srcLen - 1 - i) : (srcStart + i);
            p = std::min<int32_t>(texSize - 1, std::max<int32_t>(0, p));
            outPx[(size_t)i] = outOther1[(size_t)i] = outOther2[(size_t)i] = p;
        }
        return;
    }
    float start = flip ? (float)(srcStart + srcLen) : (float)srcStart;
    float span = flip ? -(float)srcLen : (float)srcLen;
    for (int32_t i = 0; i < n; i++) {
        float t = start + ((i + 0.5f) / (float)n) * span;
        int32_t p = (int32_t)std::floor(t);
        outPx[(size_t)i] = std::min<int32_t>(texSize - 1, std::max<int32_t>(0, p));
        float uv = t / (float)texSize;
        int32_t o1 = (int32_t)std::floor(uv * size1);
        int32_t o2 = (int32_t)std::floor(uv * size2);
        outOther1[(size_t)i] = std::min<int32_t>(std::max<int32_t>(size1 - 1, 0), std::max<int32_t>(0, o1));
        outOther2[(size_t)i] = std::min<int32_t>(std::max<int32_t>(size2 - 1, 0), std::max<int32_t>(0, o2));
    }
}

void _2DWorldBatch::RenderSprite(_2DSprite* sprite) {
    Texture2D* px = sprite->Pixel;
    if (px == nullptr || !px->HasColor() || px->Width <= 0 || px->Height <= 0) return;
    auto& dst = sprite->AbsoluteDestRect;
    if (dst.Width <= 0 || dst.Height <= 0 || sprite->SrcRect.Width <= 0 || sprite->SrcRect.Height <= 0) return;

    Framebuffer& fb = *Target;
    // the view matrix maps world pixel (x,y) to target pixel (x - scroll.x, y - scroll.y)
    int32_t sx0 = dst.X - (int32_t)Scroll.X;
    int32_t sy0 = dst.Y - (int32_t)Scroll.Y;
    int32_t cx0 = std::max<int32_t>(std::max<int32_t>(0, fb.ClipX0), sx0), cy0 = std::max<int32_t>(std::max<int32_t>(0, fb.ClipY0), sy0);
    int32_t cx1 = std::min<int32_t>(std::min<int32_t>(fb.Width, fb.ClipX1), sx0 + dst.Width), cy1 = std::min<int32_t>(std::min<int32_t>(fb.Height, fb.ClipY1), sy0 + dst.Height);
    if (cx0 >= cx1 || cy0 >= cy1) return;

    auto mode = sprite->RenderMode;
    bool useDepth = (mode == _2DBatchRenderMode::Z_BUFFER || mode == _2DBatchRenderMode::WALL);
    Texture2D* zt = useDepth ? sprite->Depth : nullptr;
    Texture2D* mt = (mode == _2DBatchRenderMode::WALL) ? sprite->Mask : nullptr;
    if (zt != nullptr && zt->Alpha8 == nullptr) zt = nullptr;
    if (mt != nullptr && !mt->HasColor()) mt = nullptr;

    BuildLookup(ColPx, ColZ, ColM, dst.Width, sprite->SrcRect.X, sprite->SrcRect.Width, sprite->FlipHorizontally,
                px->Width, zt ? zt->Width : px->Width, mt ? mt->Width : px->Width);
    BuildLookup(RowPx, RowZ, RowM, dst.Height, sprite->SrcRect.Y, sprite->SrcRect.Height, sprite->FlipVertically,
                px->Height, zt ? zt->Height : px->Height, mt ? mt->Height : px->Height);

    // vsZSprite: depth of the sprite's back corner and of the step towards the camera.
    float backDepth = 0, frontStep = 0;
    if (useDepth) {
        auto rot = WorldCamera_->Rotation;
        st::Vector3 backPosition = sprite->AbsoluteWorldPosition + BackOffForRot(rot);
        st::Vector3 frontPosition = backPosition + FrontDirForRot(rot);
        backDepth = WorldCamera_->Depth(backPosition);
        float frontDepth = WorldCamera_->Depth(frontPosition) - backDepth;
        // difference = (1 - z/255) / 0.4 ; depth = back + difference * front  ==  back + (255 - z) * frontStep
        frontStep = frontDepth / (255.0f * 0.4f);
    }

    // ambient light (psZDepthSprite / psZDepthWall multiply by tex2D(ambientSampler, roomVec))
    uint32_t lr = 256, lg = 256, lb = 256, la = 256;
    if (useDepth && AmbientLight != nullptr) {
        auto& amb = AmbientLight[sprite->Room];
        lr = amb.R + (amb.R >> 7); lg = amb.G + (amb.G >> 7); lb = amb.B + (amb.B >> 7); la = amb.A + (amb.A >> 7);
    }
    bool grayscale = useDepth && sprite->Room == 65533;
    // FSO_CPP: deferred room light (see Framebuffer::Room): write unlit colour + room id
    uint16_t* roomBuf = (useDepth && !fb.Room.empty()) ? fb.Room.data() : nullptr;
    const uint16_t roomId = grayscale ? (uint16_t)65535 : sprite->Room;
    if (roomBuf) { lr = lg = lb = la = 256; }

    // FIX(sims) memory: RGBA texels (Data) or palettised ones (Index/Alpha/Palette: SPR#/SPR2 frames, see SPR.h /
    // SPR2.h): the same colour and alpha per texel; the palette is only looked up for pixels that are drawn. The
    // loop is instantiated once per texel format (no per-pixel format test).
    const st::Color* pdata = px->Data;
    const uint8_t* pidx = px->Index;
    const uint8_t* palpha = px->Alpha;
    const st::Color* ppal = px->Palette;
    const int32_t pw = px->Width;
    auto rows = [&](auto palTag) {
        constexpr bool PAL = decltype(palTag)::value;
    for (int32_t y = cy0; y < cy1; y++) {
        int32_t iy = y - sy0;
        size_t poff = (size_t)RowPx[(size_t)iy] * pw;
        const st::Color* prow = PAL ? nullptr : pdata + poff;
        const uint8_t* irow = PAL ? pidx + poff : nullptr;
        const uint8_t* arow = PAL ? palpha + poff : nullptr;
        const uint8_t* zrow = zt ? zt->Alpha8 + (size_t)RowZ[(size_t)iy] * zt->Width : nullptr;
        size_t moff = mt ? (size_t)RowM[(size_t)iy] * mt->Width : 0;
        const st::Color* mrow = (mt && mt->Data) ? mt->Data + moff : nullptr;
        const uint8_t* marow = (mt && !mt->Data) ? mt->Alpha + moff : nullptr;
        uint32_t* crow = &fb.Color[(size_t)y * fb.Width];
        float* drow = &fb.Depth[(size_t)y * fb.Width];
        uint16_t* rrow = roomBuf ? roomBuf + (size_t)y * fb.Width : nullptr;
        for (int32_t x = cx0; x < cx1; x++) {
            int32_t ix = x - sx0;
            int32_t ci = ColPx[(size_t)ix];
            uint32_t a = PAL ? arow[ci] : prow[ci].A;
            if (mrow) a = mrow[ColM[(size_t)ix]].A; //walls: alpha comes from the style mask
            else if (marow) a = marow[ColM[(size_t)ix]];
            float d = 0;
            if (useDepth) {
                if (a <= 2) continue; // pixel.a <= 0.01: discard
                uint32_t z = zrow ? zrow[ColZ[(size_t)ix]] : 0;
                d = backDepth + (float)(255 - (int32_t)z) * frontStep;
                if (d > drow[x]) continue; // LessEqual depth test
                drow[x] = d;
                if (rrow) rrow[x] = roomId;
            } else {
                if (a == 0) continue;
            }
            const st::Color& c = PAL ? ppal[irow[ci]] : prow[ci];
            uint32_t r = c.R, g = c.G, b = c.B;
            if (grayscale) { uint32_t l = (r * 77 + g * 151 + b * 28) >> 8; r = g = b = l; }
            if (useDepth) { r = (r * lr) >> 8; g = (g * lg) >> 8; b = (b * lb) >> 8; a = (a * la) >> 8; }
            if (a >= 255) {
                crow[x] = Framebuffer::Pack((uint8_t)r, (uint8_t)g, (uint8_t)b, 255);
            } else {
                uint32_t dc = crow[x];
                uint32_t ia = 255 - a;
                uint32_t nr = (r * a + (dc & 0xFF) * ia) / 255;
                uint32_t ng = (g * a + ((dc >> 8) & 0xFF) * ia) / 255;
                uint32_t nb = (b * a + ((dc >> 16) & 0xFF) * ia) / 255;
                uint32_t na = a + (((dc >> 24) & 0xFF) * ia) / 255;
                crow[x] = Framebuffer::Pack((uint8_t)nr, (uint8_t)ng, (uint8_t)nb, (uint8_t)std::min<uint32_t>(255, na));
            }
        }
    }
    };
    if (pdata) rows(std::false_type());
    else rows(std::true_type());
}

Texture2D* _2DWorldBatch::GetTexture(SPRFrame* item) {
    if (item == nullptr) return nullptr;
    auto it = _TextureCache.find(item);
    if (it != _TextureCache.end()) return &it->second;
    item->DecodeIfRequired(); // decodes on first use
    Texture2D tex;
    tex.Width = item->Width;
    tex.Height = item->Height;
    if (item->HasColor()) { // FIX(sims) memory: palettised frame (see SPR.h)
        tex.Index = item->Index.data();
        tex.Alpha = item->Alpha.data();
        tex.Palette = item->PalColors;
    }
    auto& res = _TextureCache[item];
    res = tex;
    return &res;
}

Texture2D* _2DWorldBatch::GetTexture(SPR2Frame* item) {
    if (item == nullptr) return nullptr;
    auto it = _TextureCache.find(item);
    if (it != _TextureCache.end()) return &it->second;
    item->DecodeIfRequired();
    Texture2D tex;
    tex.Width = item->Width;
    tex.Height = item->Height;
    tex.Data = item->PixelData.empty() ? nullptr : item->PixelData.data();
    if (tex.Data == nullptr && item->HasColor()) { // FIX(sims) memory: palettised frame (see SPR2.h)
        tex.Index = item->PalData.data();
        tex.Alpha = item->AlphaData.data();
        tex.Palette = item->PalColors;
    }
    auto& res = _TextureCache[item];
    res = tex;
    return &res;
}

WorldTexture _2DWorldBatch::GetWorldTexture(SPR2Frame* item) {
    WorldTexture result;
    result.Pixel = GetTexture(item);
    if (item == nullptr) return result;
    auto it = _ZTextureCache.find(item);
    if (it == _ZTextureCache.end()) {
        Texture2D tex;
        if (!item->ZBufferData.empty() && item->Width > 0 && item->Height > 0) {
            tex.Width = item->Width;
            tex.Height = item->Height;
            tex.Alpha8 = item->ZBufferData.data();
        }
        it = _ZTextureCache.emplace(item, tex).first;
    }
    result.ZBuffer = (it->second.Alpha8 != nullptr) ? &it->second : nullptr;
    return result;
}

void _2DWorldBatch::ClearTextureCache() {
    // C# drops the GPU textures; ours are views onto the decoded frames, so just forget them.
    _TextureCache.clear();
    _ZTextureCache.clear();
}

}}}
