/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.LotView.Components.FloorComponent (World/components/FloorComponent.cs)
#include "world/components/FloorComponent.h"
#include "world/model/Blueprint.h"
#include "world/WorldState.h"
#include "world/utils/_2DWorldBatch.h"
#include "world/utils/TextureGenerator.h"
#include "content/ContentManager.h"
#include "files/formats/iff/chunks/SPR2.h"

namespace FSO { namespace LotView { namespace Components {

using namespace FSO::LotView::Model;
using namespace FSO::LotView::Utils;
using FSO::Files::Formats::IFF::Chunks::SPR2;
using FSO::Common::Utils::TextureGenerator;

const st::Rectangle FloorComponent::FLOORDEST_NEAR(5, 316, 127, 64);
const st::Rectangle FloorComponent::FLOORDEST_MED(3, 158, 63, 32);
const st::Rectangle FloorComponent::FLOORDEST_FAR(2, 79, 31, 16);

const st::Point FloorComponent::PoolDirections[8] = {
    st::Point(0, -1),
    st::Point(1, -1),
    st::Point(1, 0),
    st::Point(1, 1),
    st::Point(0, 1),
    st::Point(-1, 1),
    st::Point(-1, 0),
    st::Point(-1, -1),
};

static inline bool Has(PoolSegments a, PoolSegments b) { return ((int)a & (int)b) > 0; }
static inline st::Point RectCenter(const st::Rectangle& r) { return st::Point(r.X + r.Width / 2, r.Y + r.Height / 2); }

void FloorComponent::Draw(WorldState& world) {
    if (ArchZBuffers == nullptr) ArchZBuffers = &TextureGenerator::GetWallZBuffer();
    auto& Z = *ArchZBuffers;

    auto floorContent = FSO::Content::Content::Get()->WorldFloors.get();
    if (blueprint == nullptr || blueprint->Floors == nullptr) return;

    st::Rectangle db;
    if (!HasDrawBound) db = st::Rectangle(0, 0, blueprint->Width, blueprint->Height);
    else db = DrawBound;

    for (int8_t level = 1; level <= world.Level(); level++) {
        if ((size_t)(level - 1) >= blueprint->Floors->size()) break;
        auto rMap = blueprint->RoomMap[(size_t)(level - 1)];
        for (int16_t y = (int16_t)db.Top(); y < db.Bottom(); y++) { //ill decide on a reasonable system for components when it's finished ok pls :(
            for (int16_t x = (int16_t)db.Left(); x < db.Right(); x++) {
                auto comp = blueprint->GetFloor(x, y, level);
                if (comp.Pattern != 0) {
                    uint16_t room = (uint16_t)((rMap == nullptr) ? 0 : (*rMap)[(size_t)(x + y * blueprint->Width)]);
                    st::Vector3 tilePosition(x, y, (level - 1) * 2.95f + blueprint->GetAltitude(x, y)); // TS1(sims): FreeSO altitude
                    world._2D->OffsetPixel(world.WorldSpace.GetScreenFromTile(tilePosition));
                    world._2D->OffsetTile(tilePosition);

                    if (comp.Pattern > 65533) {
                        //determine adjacent pool tiles
                        int poolAdj = 0;
                        for (int i = 0; i < 8; i++) {
                            auto testTile = st::Point(x, y) + PoolDirections[i];
                            if ((testTile.X <= 0 || testTile.X >= blueprint->Width - 1) || (testTile.Y <= 0 || testTile.Y >= blueprint->Height - 1)
                                || blueprint->GetFloor((int16_t)testTile.X, (int16_t)testTile.Y, level).Pattern == comp.Pattern) poolAdj |= 1 << i;
                        }

                        auto adj = RotatePoolSegs((PoolSegments)poolAdj, (int)world.Rotation());

                        //get and draw the base tile
                        int spriteNum = 0;
                        if (Has(adj, PoolSegments::TopRight)) spriteNum |= 1;
                        if (Has(adj, PoolSegments::TopLeft)) spriteNum |= 2;
                        if (Has(adj, PoolSegments::BottomLeft)) spriteNum |= 4;
                        if (Has(adj, PoolSegments::BottomRight)) spriteNum |= 8;

                        auto _Sprite = world._2D->NewSprite(_2DBatchRenderMode::Z_BUFFER);

                        SPR2* sprite = nullptr;
                        int baseSPR;
                        int frameNum = 0;
                        switch (world.Zoom()) {
                            case WorldZoom::Far:
                                baseSPR = (comp.Pattern == 65535) ? 0x400 : 0x800;
                                frameNum = (comp.Pattern == 65535) ? 0 : 2;
                                sprite = floorContent->GetGlobalSPR((uint16_t)(baseSPR + spriteNum));
                                _Sprite->DestRect = FLOORDEST_FAR;
                                _Sprite->Depth = &Z[14];
                                break;
                            case WorldZoom::Medium:
                                baseSPR = (comp.Pattern == 65535) ? 0x410 : 0x800;
                                frameNum = (comp.Pattern == 65535) ? 0 : 1;
                                sprite = floorContent->GetGlobalSPR((uint16_t)(baseSPR + spriteNum));
                                _Sprite->DestRect = FLOORDEST_MED;
                                _Sprite->Depth = &Z[13];
                                break;
                            case WorldZoom::Near:
                                baseSPR = (comp.Pattern == 65535) ? 0x420 : 0x800;
                                sprite = floorContent->GetGlobalSPR((uint16_t)(baseSPR + spriteNum));
                                _Sprite->DestRect = FLOORDEST_NEAR;
                                _Sprite->Depth = &Z[12];
                                break;
                        }
                        if (sprite == nullptr || (size_t)frameNum >= sprite->Frames.size()) continue;

                        _Sprite->Pixel = (comp.Pattern == 65534) ? GetWaterTexture(sprite->Frames[(size_t)frameNum].get())
                                                                  : world._2D->GetTexture(sprite->Frames[(size_t)frameNum].get());
                        if (_Sprite->Pixel == nullptr) continue;
                        _Sprite->SrcRect = st::Rectangle(0, 0, _Sprite->Pixel->Width, _Sprite->Pixel->Height);
                        _Sprite->Room = room;
                        world._2D->Draw(_Sprite);

                        //draw any corners on top
                        if (comp.Pattern == 65535) {
                            PoolSegments CornerChecks[4] = {
                                (PoolSegments)((int)PoolSegments::TopLeft | (int)PoolSegments::TopRight),
                                (PoolSegments)((int)PoolSegments::TopLeft | (int)PoolSegments::BottomLeft),
                                (PoolSegments)((int)PoolSegments::BottomLeft | (int)PoolSegments::BottomRight),
                                (PoolSegments)((int)PoolSegments::TopRight | (int)PoolSegments::BottomRight)
                            };
                            int zoomI = (int)world.Zoom();
                            auto base = _Sprite->DestRect;
                            auto ctr = RectCenter(base);
                            auto corner = [&](int sprOff, int zbuf, int which) {
                                auto cs = floorContent->GetGlobalSPR((uint16_t)(0x430 + sprOff + (zoomI - 1) * 4));
                                if (cs == nullptr || cs->Frames.empty()) return;
                                auto tcS = world._2D->NewSprite(_2DBatchRenderMode::Z_BUFFER);
                                //base sprite position on base tile
                                tcS->Pixel = world._2D->GetTexture(cs->Frames[0].get());
                                int w = tcS->Pixel->Width, h = tcS->Pixel->Height;
                                switch (which) {
                                    case 0: tcS->DestRect = st::Rectangle(ctr.X - w / 2, base.Y, w, h); break; //top corner
                                    case 1: tcS->DestRect = st::Rectangle(base.X, ctr.Y + 1 - h / 2, w, h); break; //left corner
                                    case 2: tcS->DestRect = st::Rectangle(ctr.X - w / 2, base.Bottom() - h, w, h); break; //bottom corner
                                    case 3: tcS->DestRect = st::Rectangle(base.Right() - w, ctr.Y + 1 - h / 2, w, h); break; //right corner
                                }
                                tcS->SrcRect = st::Rectangle(0, 0, w, h);
                                tcS->Depth = &Z[(size_t)(zbuf + (3 - zoomI))];
                                tcS->Room = room;
                                world._2D->Draw(tcS);
                            };

                            if (((int)adj & (int)CornerChecks[0]) == (int)CornerChecks[0] && !Has(adj, PoolSegments::Top)) corner(0, 21, 0);
                            if (((int)adj & (int)CornerChecks[1]) == (int)CornerChecks[1] && !Has(adj, PoolSegments::Left)) corner(1, 24, 1);
                            if (((int)adj & (int)CornerChecks[2]) == (int)CornerChecks[2] && !Has(adj, PoolSegments::Bottom)) corner(2, 27, 2);
                            if (((int)adj & (int)CornerChecks[3]) == (int)CornerChecks[3] && !Has(adj, PoolSegments::Right)) corner(3, 24, 3);
                        }
                    } else {
                        auto floor = GetFloorSprite(floorContent->Get(comp.Pattern, false), 0, world);
                        floor->Room = room;
                        if (floor->Pixel != nullptr) world._2D->Draw(floor);
                    }
                } else if (world.BuildMode() > 1 && level > 1 && blueprint->Supported != nullptr
                           && (*blueprint->Supported)[(size_t)(level - 2)][(size_t)(y * blueprint->Height + x)]) {
                    st::Vector3 tilePosition(x, y, (level - 1) * 2.95f + blueprint->GetAltitude(x, y)); // TS1(sims): FreeSO altitude
                    world._2D->OffsetPixel(world.WorldSpace.GetScreenFromTile(tilePosition));
                    world._2D->OffsetTile(tilePosition);

                    auto floor = GetAirSprite(world);
                    floor->Room = 65535;
                    if (floor->Pixel != nullptr) world._2D->Draw(floor);
                }
            }
        }
    }
}

Texture2D* FloorComponent::GetWaterTexture(void* framePtr) {
    auto frame = (FSO::Files::Formats::IFF::Chunks::SPR2Frame*)framePtr;
    if (frame == nullptr) return nullptr;
    auto it = WaterTextures.find(frame);
    if (it != WaterTextures.end()) return &it->second->Tex;
    frame->DecodeIfRequired();
    std::unique_ptr<KeyedTexture> k(new KeyedTexture());
    k->Data = frame->ExpandColors(); // FIX(sims): palettised frames (see SPR2.h)
    for (size_t i = 0; i < k->Data.size() && i < frame->PalData.size(); i++)
        if (frame->PalData[i] == frame->TransparentColorIndex) k->Data[i] = st::Color(0, 0, 0, 0);
    k->Tex.Width = frame->Width;
    k->Tex.Height = frame->Height;
    k->Tex.Data = k->Data.empty() ? nullptr : k->Data.data();
    auto res = &k->Tex;
    WaterTextures[frame] = std::move(k);
    return res;
}

_2DSprite* FloorComponent::GetAirSprite(WorldState& world) {
    auto _Sprite = world._2D->NewSprite(_2DBatchRenderMode::Z_BUFFER);
    auto& airTiles = TextureGenerator::GetAirTiles();
    auto& Z = *ArchZBuffers;
    Texture2D* sprite = nullptr;
    switch (world.Zoom()) {
        case WorldZoom::Far:
            sprite = &airTiles[2];
            _Sprite->DestRect = FLOORDEST_FAR;
            _Sprite->Depth = &Z[14];
            break;
        case WorldZoom::Medium:
            sprite = &airTiles[1];
            _Sprite->DestRect = FLOORDEST_MED;
            _Sprite->Depth = &Z[13];
            break;
        case WorldZoom::Near:
            sprite = &airTiles[0];
            _Sprite->DestRect = FLOORDEST_NEAR;
            _Sprite->Depth = &Z[12];
            break;
    }
    _Sprite->Pixel = sprite;
    _Sprite->SrcRect = st::Rectangle(0, 0, _Sprite->Pixel->Width, _Sprite->Pixel->Height);
    return _Sprite;
}

PoolSegments FloorComponent::RotatePoolSegs(PoolSegments ps, int rotate) {
    int poolSides = (int)ps;
    int rotPart = ((poolSides << (rotate * 2) % 8) & 255) | ((poolSides & 255) >> (8 - rotate * 2) % 8);
    return (PoolSegments)rotPart;
}

_2DSprite* FloorComponent::GetFloorSprite(FSO::Content::Floor* pattern, int rotation, WorldState& world) {
    auto _Sprite = world._2D->NewSprite(_2DBatchRenderMode::Z_BUFFER);
    if (pattern == nullptr) return _Sprite;
    auto& Z = *ArchZBuffers;
    SPR2* sprite = nullptr;
    bool vertFlip = world.Rotation() == WorldRotation::TopRight || world.Rotation() == WorldRotation::BottomRight;
    int bufOff = (vertFlip) ? 3 : 0;
    switch (world.Zoom()) {
        case WorldZoom::Far:
            sprite = pattern->Far;
            _Sprite->DestRect = FLOORDEST_FAR;
            _Sprite->Depth = &Z[(size_t)(14 + bufOff)];
            break;
        case WorldZoom::Medium:
            sprite = pattern->Medium;
            _Sprite->DestRect = FLOORDEST_MED;
            _Sprite->Depth = &Z[(size_t)(13 + bufOff)];
            break;
        case WorldZoom::Near:
            sprite = pattern->Near;
            _Sprite->DestRect = FLOORDEST_NEAR;
            _Sprite->Depth = &Z[(size_t)(12 + bufOff)];
            break;
    }
    if (sprite != nullptr && (size_t)rotation < sprite->Frames.size()) {
        _Sprite->Pixel = world._2D->GetTexture(sprite->Frames[(size_t)rotation].get());
        _Sprite->SrcRect = st::Rectangle(0, 0, _Sprite->Pixel->Width, _Sprite->Pixel->Height);

        if (vertFlip) _Sprite->FlipVertically = true;
        if ((int)world.Rotation() > 1) _Sprite->FlipHorizontally = true;
    }
    return _Sprite;
}

}}}
