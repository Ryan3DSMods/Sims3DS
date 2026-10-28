/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.LotView.Components.WallComponent (World/components/WallComponent.cs)
#include "world/components/WallComponent.h"
#include "world/model/Blueprint.h"
#include "world/WorldState.h"
#include "world/utils/_2DWorldBatch.h"
#include "world/utils/TextureGenerator.h"
#include "content/ContentManager.h"
#include "files/formats/iff/chunks/SPR.h"
#include "files/formats/iff/chunks/SPR2.h"
#include <cstring>

namespace FSO { namespace LotView { namespace Components {

using namespace FSO::LotView::Model;
using namespace FSO::LotView::Utils;
using FSO::Files::Formats::IFF::Chunks::SPR;
using FSO::Files::Formats::IFF::Chunks::SPR2;
using FSO::Common::Utils::TextureGenerator;
using FSO::Content::Wall;
using FSO::Content::WallStyle;
using FSO::Content::Floor;

static const int NEAR_YOFF = 1;
static const int MED_YOFF = 1;
static const int FAR_YOFF = 1;

//first is top left, second is top right, third is diag horiz, fourth is diag vert
const st::Rectangle WallComponent::DESTINATION_NEAR[4] = {
    st::Rectangle(4, 76 + NEAR_YOFF, 64, 271),
    st::Rectangle(68, 76 + NEAR_YOFF, 64, 271),
    st::Rectangle(4, 108 + NEAR_YOFF, 128, 240),
    st::Rectangle(60, 144 + NEAR_YOFF, 16, 232)
};
const st::Rectangle WallComponent::DESTINATION_MED[4] = {
    st::Rectangle(2, 38 + MED_YOFF, 32, 135),
    st::Rectangle(34, 38 + MED_YOFF, 32, 135),
    st::Rectangle(2, 54 + MED_YOFF, 64, 120),
    st::Rectangle(30, 72 + MED_YOFF, 8, 116)
};
const st::Rectangle WallComponent::DESTINATION_FAR[4] = {
    st::Rectangle(1, 19 + FAR_YOFF, 16, 67),
    st::Rectangle(17, 19 + FAR_YOFF, 16, 67),
    st::Rectangle(1, 27 + FAR_YOFF, 32, 60),
    st::Rectangle(15, 36 + FAR_YOFF, 4, 58)
};

const st::Rectangle WallComponent::JUNCDEST_NEAR(4, 316 + NEAR_YOFF, 128, 64);
const st::Rectangle WallComponent::JUNCDEST_MED(2, 158 + MED_YOFF, 64, 32);
const st::Rectangle WallComponent::JUNCDEST_FAR(1, 79 + FAR_YOFF, 32, 16);

const st::Rectangle WallComponent::FLRDEST_NEAR(5, 316, 127, 64);
const st::Rectangle WallComponent::FLRDEST_MED(3, 158, 63, 32);
const st::Rectangle WallComponent::FLRDEST_FAR(2, 79, 31, 16);

int8_t WallComponent::JunctionMap[256];

#define JF(x) ((int)JunctionFlags::x)

void WallComponent::InitJunctionMap() {
    static bool init = false;
    if (init) return;
    init = true;
    std::memset(JunctionMap, -1, sizeof(JunctionMap));
    JunctionMap[JF(TopRight)] = 0;
    JunctionMap[JF(TopLeft)] = 1;
    JunctionMap[JF(TopLeft) | JF(TopRight)] = 2;
    JunctionMap[JF(BottomLeft)] = 3;
    JunctionMap[JF(BottomLeft) | JF(TopRight)] = 4;
    JunctionMap[JF(BottomLeft) | JF(TopLeft)] = 5;
    JunctionMap[JF(BottomLeft) | JF(TopLeft) | JF(TopRight)] = 6;
    JunctionMap[JF(BottomRight)] = 7;
    JunctionMap[JF(BottomRight) | JF(TopRight)] = 8;
    JunctionMap[JF(BottomRight) | JF(TopLeft)] = 9;
    JunctionMap[JF(BottomRight) | JF(TopRight) | JF(TopLeft)] = 10;
    JunctionMap[JF(BottomRight) | JF(BottomLeft)] = 11;
    JunctionMap[JF(BottomRight) | JF(BottomLeft) | JF(TopRight)] = 12;
    JunctionMap[JF(BottomRight) | JF(BottomLeft) | JF(TopLeft)] = 13;
    JunctionMap[JF(AllNonDiag)] = 14;
    JunctionMap[JF(DiagTop)] = 15;
    JunctionMap[JF(DiagTop) | JF(BottomLeft)] = 16;
    JunctionMap[JF(DiagTop) | JF(BottomRight)] = 17;
    JunctionMap[JF(DiagTop) | JF(BottomLeft) | JF(BottomRight)] = 18;
    JunctionMap[JF(DiagLeft)] = 19;
    JunctionMap[JF(DiagLeft) | JF(TopRight)] = 20;
    JunctionMap[JF(DiagLeft) | JF(BottomRight)] = 21;
    JunctionMap[JF(DiagLeft) | JF(BottomRight) | JF(TopRight)] = 22;
    JunctionMap[JF(DiagLeft) | JF(DiagTop)] = 23;
    JunctionMap[JF(DiagLeft) | JF(DiagTop) | JF(BottomRight)] = 24;
    JunctionMap[JF(DiagBottom)] = 25;
    JunctionMap[JF(DiagBottom) | JF(TopRight)] = 26;
    JunctionMap[JF(DiagBottom) | JF(TopLeft)] = 27;
    JunctionMap[JF(DiagBottom) | JF(TopLeft) | JF(TopRight)] = 28;
    JunctionMap[JF(DiagBottom) | JF(DiagTop)] = 29;
    JunctionMap[JF(DiagBottom) | JF(DiagLeft)] = 30;
    JunctionMap[JF(DiagBottom) | JF(DiagLeft) | JF(TopRight)] = 31;
    JunctionMap[JF(DiagBottom) | JF(DiagLeft) | JF(DiagTop)] = 32;
    JunctionMap[JF(DiagRight)] = 33;
    JunctionMap[JF(DiagRight) | JF(TopLeft)] = 34;
    JunctionMap[JF(DiagRight) | JF(BottomLeft)] = 35;
    JunctionMap[JF(DiagRight) | JF(TopLeft) | JF(BottomLeft)] = 36;
    JunctionMap[JF(DiagRight) | JF(DiagTop)] = 37;
    JunctionMap[JF(DiagRight) | JF(DiagTop) | JF(BottomLeft)] = 38;
    JunctionMap[JF(DiagRight) | JF(DiagLeft)] = 39;
    JunctionMap[JF(DiagRight) | JF(DiagLeft) | JF(DiagTop)] = 40;
    JunctionMap[JF(DiagRight) | JF(DiagBottom)] = 41;
    JunctionMap[JF(DiagRight) | JF(DiagBottom) | JF(TopLeft)] = 42;
    JunctionMap[JF(DiagRight) | JF(DiagBottom) | JF(DiagTop)] = 43;
    JunctionMap[JF(DiagRight) | JF(DiagBottom) | JF(DiagLeft)] = 44;
    JunctionMap[JF(AllDiag)] = 45;
}

static inline bool HasSeg(WallSegments s, WallSegments f) { return ((int)s & (int)f) == (int)f; }

void WallComponent::Draw(WorldState& world) {
    InitJunctionMap();
    if (WallZBuffers == nullptr) WallZBuffers = &TextureGenerator::GetWallZBuffer();
    auto& Z = *WallZBuffers;
    if (blueprint == nullptr || blueprint->Walls == nullptr || blueprint->WallsAt == nullptr) return;

    auto wallContent = FSO::Content::Content::Get()->WorldWalls.get();
    auto floorContent = FSO::Content::Content::Get()->WorldFloors.get();
    auto rotation = world.Rotation();
    auto zoom = world.Zoom();

    //draw walls
    for (int8_t level = 1; level <= world.Level(); level++) {
        if ((size_t)(level - 1) >= blueprint->Walls->size()) break;
        int off = 0;
        bool canCut = !(level < world.Level());
        GenerateWallData((*blueprint->Walls)[(size_t)(level - 1)], (*blueprint->WallsAt)[(size_t)(level - 1)], canCut);
        auto rMap = blueprint->RoomMap[(size_t)(level - 1)];
        for (int16_t y = 0; y < blueprint->Height; y++) { //ill decide on a reasonable system for components when it's finished ok pls :(
            for (int16_t x = 0; x < blueprint->Height; x++) {
                TileRoom = (rMap == nullptr) ? 1 : (*rMap)[(size_t)(x + y * blueprint->Width)];
                auto comp = blueprint->GetWall(x, y, level);
                if (comp.Segments != 0) {
                    comp = RotateWall(rotation, comp, x, y, level);
                    st::Vector3 tilePosition(x, y, (level - 1) * 2.95f + blueprint->GetAltitude(x, y)); // TS1(sims): FreeSO altitude
                    world._2D->OffsetPixel(world.WorldSpace.GetScreenFromTile(tilePosition));
                    world._2D->OffsetTile(tilePosition);
                    auto myCuts = Cuts[(size_t)off];

                    if ((uint8_t)comp.Segments < 16) myCuts = RotateCuts(rotation, myCuts, x, y);
                    else if ((int)rotation > 1) { //for diagonals, just flip cuts on two furthest rotations
                        if (myCuts.TLCut == WallCut::DownRightUpLeft) myCuts.TLCut = WallCut::DownLeftUpRight;
                        else if (myCuts.TLCut == WallCut::DownLeftUpRight) myCuts.TLCut = WallCut::DownRightUpLeft;
                        if (myCuts.TRCut == WallCut::DownRightUpLeft) myCuts.TRCut = WallCut::DownLeftUpRight;
                        else if (myCuts.TRCut == WallCut::DownLeftUpRight) myCuts.TRCut = WallCut::DownRightUpLeft;
                    }

                    if (HasSeg(comp.Segments, WallSegments::TopLeft) && !((int)myCuts.TLCut > 0 && comp.TopLeftDoor)) {
                        //draw top left and top right relative to this rotation
                        auto tlPattern = GetPattern(comp.TopLeftPattern);

                        uint16_t styleID = 1;
                        uint16_t overlayID = 0;
                        bool down = false;

                        switch (myCuts.TLCut) {
                            case WallCut::Down:
                                styleID = comp.TopLeftStyle;
                                down = true; break;
                            case WallCut::DownLeftUpRight:
                                styleID = 7;
                                overlayID = 252; break;
                            case WallCut::DownRightUpLeft:
                                styleID = 8;
                                overlayID = 253; break;
                            default:
                                if (comp.TopLeftStyle != 1) styleID = comp.TopLeftStyle;
                                else if (comp.ObjSetTLStyle != 0) styleID = comp.ObjSetTLStyle; //use custom style set by object if no cut
                                else styleID = 1;
                                break;
                        }

                        auto tlStyle = GetStyle(styleID);

                        auto _Sprite = GetWallSprite(tlPattern, tlStyle, 0, down, world);
                        if (_Sprite->Pixel != nullptr) {
                            world._2D->Draw(_Sprite);

                            //draw overlay if exists
                            if (overlayID != 0) {
                                auto spr = GetWallSprite(GetPattern(overlayID), nullptr, 0, down, world);
                                spr->Room = 1;
                                if (spr->Pixel != nullptr) world._2D->Draw(spr);
                            }

                            auto contOff = tilePosition + RotateOffset(rotation, st::Vector3(0, -1, 0));

                            if (comp.TopLeftThick()) {
                                //check far side of wall for continuation. if there is none, round this part off
                                if (!comp.TopRightThick() && OffsetValid(contOff)) {
                                    auto comp2 = RotateWall(rotation, blueprint->GetWall((int16_t)(contOff.X), (int16_t)(contOff.Y), level), (int16_t)(contOff.X), (int16_t)(contOff.Y), level);
                                    if (!comp2.TopLeftThick()) {
                                        _Sprite = CopySprite(world, _Sprite);
                                        tlStyle = GetStyle(comp.TopLeftStyle); //return to normal if cutaway
                                        auto tilePosition2 = contOff;

                                        world._2D->OffsetPixel(world.WorldSpace.GetScreenFromTile(tilePosition2));
                                        world._2D->OffsetTile(tilePosition2);
                                        int newWidth = 0;
                                        bool downAtCont = canCut && WallsDownAt((int16_t)(contOff.X), (int16_t)(contOff.Y));

                                        SPR* mask = nullptr;
                                        switch (zoom) {
                                            case WorldZoom::Far:
                                                newWidth = 3;
                                                if (tlStyle) mask = downAtCont ? tlStyle->WallsDownFar : tlStyle->WallsUpFar;
                                                break;
                                            case WorldZoom::Medium:
                                                newWidth = 6;
                                                if (tlStyle) mask = downAtCont ? tlStyle->WallsDownMedium : tlStyle->WallsUpMedium;
                                                break;
                                            case WorldZoom::Near:
                                                newWidth = 12;
                                                if (tlStyle) mask = downAtCont ? tlStyle->WallsDownNear : tlStyle->WallsUpNear;
                                                break;
                                        }
                                        if (mask != nullptr && !mask->Frames.empty()) _Sprite->Mask = world._2D->GetTexture(mask->Frames[0].get());
                                        if (x > 0 && y > 0 && HasSeg(blueprint->GetWall((int16_t)(x - 1), (int16_t)(y - 1), level).Segments, WallSegments::VerticalDiag)) newWidth = (newWidth * 2) / 3;
                                        //if there is a diagonal behind the extension, make it a bit shorter.
                                        _Sprite->SrcRect.Width = newWidth;
                                        _Sprite->DestRect.Width = newWidth;
                                        world._2D->Draw(_Sprite);
                                    }
                                }

                                //check close side of wall for continuation
                                contOff = tilePosition + RotateOffset(rotation, st::Vector3(0, 1, 0)); //so we can check the most things with the least impact
                                auto contOff2 = tilePosition + RotateOffset(rotation, st::Vector3(-1, 1, 0)); //last check, unfortunately required
                                if (OffsetValid(contOff) && OffsetValid(contOff2)) {
                                    auto comp2 = RotateWall(rotation, blueprint->GetWall((int16_t)(contOff.X), (int16_t)(contOff.Y), level), (int16_t)(contOff.X), (int16_t)(contOff.Y), level);
                                    if (!comp2.TopLeftThick() && !comp2.TopRightThick()) {
                                        auto comp3 = RotateWall(rotation, blueprint->GetWall((int16_t)(contOff2.X), (int16_t)(contOff2.Y), level), (int16_t)(contOff2.X), (int16_t)(contOff2.Y), level);
                                        if (!comp3.TopRightThick()) {
                                            _Sprite = GetWallSprite(tlPattern, tlStyle, 1, down, world);
                                            auto tilePosition2 = tilePosition + RotateOffset(rotation, st::Vector3(-1, 1, 0));
                                            bool downAtCont = canCut && WallsDownAt((int)tilePosition.X, (int)tilePosition.Y) && styleID != 8;
                                            tlStyle = GetStyle(comp.TopLeftStyle);

                                            world._2D->OffsetPixel(world.WorldSpace.GetScreenFromTile(tilePosition2));
                                            world._2D->OffsetTile(tilePosition2);
                                            int newWidth = 0;

                                            SPR* mask = nullptr;
                                            switch (zoom) {
                                                case WorldZoom::Far:
                                                    newWidth = 3;
                                                    if (tlStyle) mask = downAtCont ? tlStyle->WallsDownFar : tlStyle->WallsUpFar;
                                                    break;
                                                case WorldZoom::Medium:
                                                    newWidth = 6;
                                                    if (tlStyle) mask = downAtCont ? tlStyle->WallsDownMedium : tlStyle->WallsUpMedium;
                                                    break;
                                                case WorldZoom::Near:
                                                    newWidth = 12;
                                                    if (tlStyle) mask = downAtCont ? tlStyle->WallsDownNear : tlStyle->WallsUpNear;
                                                    break;
                                            }
                                            if (mask != nullptr && mask->Frames.size() > 1) _Sprite->Mask = world._2D->GetTexture(mask->Frames[1].get());
                                            _Sprite->SrcRect.X += _Sprite->SrcRect.Width - newWidth;
                                            _Sprite->DestRect.X += _Sprite->DestRect.Width - newWidth;
                                            _Sprite->SrcRect.Width = newWidth;
                                            _Sprite->DestRect.Width = newWidth;
                                            if (_Sprite->Pixel != nullptr) world._2D->Draw(_Sprite);
                                        }
                                    }
                                }
                                world._2D->OffsetPixel(world.WorldSpace.GetScreenFromTile(tilePosition));
                                world._2D->OffsetTile(tilePosition);
                            }
                        }
                    }
                    //top right

                    if (HasSeg(comp.Segments, WallSegments::TopRight) && !((int)myCuts.TRCut > 0 && comp.TopRightDoor)) {
                        auto trPattern = GetPattern(comp.TopRightPattern);

                        uint16_t styleID = 1;
                        uint16_t overlayID = 0;
                        bool down = false;

                        switch (myCuts.TRCut) {
                            case WallCut::Down:
                                styleID = comp.TopRightStyle;
                                down = true; break;
                            case WallCut::DownLeftUpRight:
                                styleID = 7;
                                overlayID = 252; break;
                            case WallCut::DownRightUpLeft:
                                styleID = 8;
                                overlayID = 253; break;
                            default:
                                if (comp.TopRightStyle != 1) styleID = comp.TopRightStyle;
                                else if (comp.ObjSetTRStyle != 0) styleID = comp.ObjSetTRStyle; //use custom style set by object if no cut
                                else styleID = 1;
                                break;
                        }

                        auto trStyle = GetStyle(styleID);

                        auto _Sprite = GetWallSprite(trPattern, trStyle, 1, down, world);
                        if (_Sprite->Pixel != nullptr) {
                            world._2D->Draw(_Sprite);

                            if (overlayID != 0) {
                                auto spr = GetWallSprite(GetPattern(overlayID), nullptr, 1, down, world);
                                spr->Room = 1;
                                if (spr->Pixel != nullptr) world._2D->Draw(spr);
                            }
                            auto contOff = tilePosition + RotateOffset(rotation, st::Vector3(-1, 0, 0));
                            if (comp.TopRightThick()) {
                                //check far side of wall for continuation. if there is none, round this part off
                                if (!comp.TopLeftThick() && OffsetValid(contOff)) {
                                    auto comp2 = RotateWall(rotation, blueprint->GetWall((int16_t)(contOff.X), (int16_t)(contOff.Y), level), (int16_t)(contOff.X), (int16_t)(contOff.Y), level);
                                    if (!comp2.TopRightThick()) {
                                        _Sprite = CopySprite(world, _Sprite);
                                        trStyle = GetStyle(comp.TopRightStyle); //return to normal if cutaway

                                        auto tilePosition2 = contOff;
                                        world._2D->OffsetPixel(world.WorldSpace.GetScreenFromTile(tilePosition2));
                                        world._2D->OffsetTile(tilePosition2);
                                        int newWidth = 0;
                                        bool downAtCont = canCut && WallsDownAt((int16_t)(contOff.X), (int16_t)(contOff.Y));

                                        SPR* mask = nullptr;
                                        switch (zoom) {
                                            case WorldZoom::Far:
                                                newWidth = 3;
                                                if (trStyle) mask = downAtCont ? trStyle->WallsDownFar : trStyle->WallsUpFar;
                                                break;
                                            case WorldZoom::Medium:
                                                newWidth = 6;
                                                if (trStyle) mask = downAtCont ? trStyle->WallsDownMedium : trStyle->WallsUpMedium;
                                                break;
                                            case WorldZoom::Near:
                                                newWidth = 12;
                                                if (trStyle) mask = downAtCont ? trStyle->WallsDownNear : trStyle->WallsUpNear;
                                                break;
                                        }
                                        if (mask != nullptr && mask->Frames.size() > 1) _Sprite->Mask = world._2D->GetTexture(mask->Frames[1].get());
                                        if (y > 0 && x > 0 && HasSeg(blueprint->GetWall((int16_t)(x - 1), (int16_t)(y - 1), level).Segments, WallSegments::VerticalDiag)) newWidth = (newWidth * 2) / 3;
                                        //if there is a diagonal behind the extension, make it a bit shorter.
                                        _Sprite->SrcRect.X += _Sprite->SrcRect.Width - newWidth;
                                        _Sprite->DestRect.X += _Sprite->DestRect.Width - newWidth;
                                        _Sprite->SrcRect.Width = newWidth;
                                        _Sprite->DestRect.Width = newWidth;
                                        world._2D->Draw(_Sprite);
                                    }
                                }

                                //check close side of wall for continuation
                                contOff = tilePosition + RotateOffset(rotation, st::Vector3(1, 0, 0)); //so we can check the most things with the least impact
                                auto contOff2 = tilePosition + RotateOffset(rotation, st::Vector3(1, -1, 0)); //last check, unfortunately required
                                if (OffsetValid(contOff) && OffsetValid(contOff2)) {
                                    auto comp2 = RotateWall(rotation, blueprint->GetWall((int16_t)(contOff.X), (int16_t)(contOff.Y), level), (int16_t)(contOff.X), (int16_t)(contOff.Y), level);
                                    if (!comp2.TopLeftThick() && !comp2.TopRightThick()) {
                                        auto comp3 = RotateWall(rotation, blueprint->GetWall((int16_t)(contOff2.X), (int16_t)(contOff2.Y), level), (int16_t)(contOff2.X), (int16_t)(contOff2.Y), level);
                                        if (!comp3.TopLeftThick()) {
                                            _Sprite = GetWallSprite(trPattern, trStyle, 0, down, world);
                                            auto tilePosition2 = tilePosition + RotateOffset(rotation, st::Vector3(1, -1, 0));
                                            bool downAtCont = canCut && WallsDownAt((int)tilePosition.X, (int)tilePosition.Y) && styleID != 8;
                                            trStyle = GetStyle(comp.TopRightStyle);

                                            world._2D->OffsetPixel(world.WorldSpace.GetScreenFromTile(tilePosition2));
                                            world._2D->OffsetTile(tilePosition2);
                                            int newWidth = 0;

                                            SPR* mask = nullptr;
                                            switch (zoom) {
                                                case WorldZoom::Far:
                                                    newWidth = 3;
                                                    if (trStyle) mask = downAtCont ? trStyle->WallsDownFar : trStyle->WallsUpFar;
                                                    break;
                                                case WorldZoom::Medium:
                                                    newWidth = 6;
                                                    if (trStyle) mask = downAtCont ? trStyle->WallsDownMedium : trStyle->WallsUpMedium;
                                                    break;
                                                case WorldZoom::Near:
                                                    newWidth = 12;
                                                    if (trStyle) mask = downAtCont ? trStyle->WallsDownNear : trStyle->WallsUpNear;
                                                    break;
                                            }
                                            if (mask != nullptr && !mask->Frames.empty()) _Sprite->Mask = world._2D->GetTexture(mask->Frames[0].get());
                                            _Sprite->SrcRect.Width = newWidth;
                                            _Sprite->DestRect.Width = newWidth;
                                            if (_Sprite->Pixel != nullptr) world._2D->Draw(_Sprite);
                                        }
                                    }
                                }
                                world._2D->OffsetPixel(world.WorldSpace.GetScreenFromTile(tilePosition));
                                world._2D->OffsetTile(tilePosition);
                            }
                        }
                    }

                    //horizontal diag

                    if (comp.Segments == WallSegments::HorizontalDiag) {
                        auto trPattern = GetPattern(comp.BottomRightPattern); //bottom left is facing other way.

                        uint16_t styleID = 1;
                        uint16_t overlayID = 0;
                        bool down = false;

                        switch (myCuts.TRCut) {
                            case WallCut::Down:
                                styleID = comp.TopRightStyle;
                                down = true; break;
                            case WallCut::DownLeftUpRight:
                                styleID = 7;
                                overlayID = 252; break;
                            case WallCut::DownRightUpLeft:
                                styleID = 8;
                                overlayID = 253; break;
                            default:
                                if (comp.TopRightStyle != 1) styleID = comp.TopRightStyle;
                                else if (comp.ObjSetTRStyle != 0) styleID = comp.ObjSetTRStyle; //use custom style set by object if no cut
                                else styleID = 1;
                                break;
                        }

                        auto trStyle = GetStyle(styleID);

                        int roomSide = 16 - ((((int)rotation + 1) / 2) % 2) * 16;
                        auto _Sprite = GetWallSprite(trPattern, trStyle, 2, down, world);

                        _Sprite->Room = (uint16_t)(TileRoom >> roomSide);
                        if (_Sprite->Pixel != nullptr) {
                            world._2D->Draw(_Sprite);

                            if (overlayID != 0) {
                                auto spr = GetWallSprite(GetPattern(overlayID), nullptr, 2, down, world);
                                spr->Room = 1;
                                if (spr->Pixel != nullptr) world._2D->Draw(spr);
                            }

                            //draw diagonally cut floors
                            if (comp.TopLeftPattern != 0) {
                                auto floor = GetFloorSprite(floorContent->Get(comp.TopLeftPattern, false), 0, world, 3);
                                floor->Room = (uint16_t)(TileRoom >> roomSide);
                                if (floor->Pixel != nullptr) world._2D->Draw(floor);
                            }
                            if (comp.TopLeftStyle != 0) {
                                auto floor = GetFloorSprite(floorContent->Get(comp.TopLeftStyle, false), 0, world, 2);
                                floor->Room = (uint16_t)(TileRoom >> (16 - roomSide));
                                if (floor->Pixel != nullptr) world._2D->Draw(floor);
                            }
                        }
                    }

                    if (comp.Segments == WallSegments::VerticalDiag) {
                        auto trPattern = GetPattern(comp.BottomRightPattern); //choose right one here, not sure which is chosen in real game

                        uint16_t styleID = 1;
                        uint16_t overlayID = 0;
                        bool down = false;

                        switch (myCuts.TRCut) {
                            case WallCut::Down:
                                styleID = comp.TopRightStyle;
                                down = true; break;
                            case WallCut::DownLeftUpRight:
                                styleID = 7;
                                overlayID = 252; break;
                            case WallCut::DownRightUpLeft:
                                styleID = 8;
                                overlayID = 253; break;
                            default:
                                if (comp.TopRightStyle != 1) styleID = comp.TopRightStyle;
                                else if (comp.ObjSetTRStyle != 0) styleID = comp.ObjSetTRStyle; //use custom style set by object if no cut
                                else styleID = 1;
                                break;
                        }

                        auto trStyle = GetStyle(styleID);

                        int roomSide = ((int)rotation / 2) * 16;
                        auto _Sprite = GetWallSprite(trPattern, trStyle, 3, down, world);
                        _Sprite->Room = (uint16_t)(TileRoom >> roomSide);
                        if (_Sprite->Pixel != nullptr) {
                            world._2D->Draw(_Sprite);

                            if (overlayID != 0) {
                                auto spr = GetWallSprite(GetPattern(overlayID), nullptr, 3, down, world);
                                spr->Room = 1;
                                if (spr->Pixel != nullptr) world._2D->Draw(spr);
                            }
                            //draw diagonally cut floors
                            if (comp.TopLeftPattern != 0) {
                                auto floor = GetFloorSprite(floorContent->Get(comp.TopLeftPattern, false), 0, world, 1);
                                floor->Room = (uint16_t)(TileRoom >> roomSide);
                                if (floor->Pixel != nullptr) world._2D->Draw(floor);
                            }
                            if (comp.TopLeftStyle != 0) {
                                auto floor = GetFloorSprite(floorContent->Get(comp.TopLeftStyle, false), 0, world, 0);
                                floor->Room = (uint16_t)(TileRoom >> (16 - roomSide));
                                if (floor->Pixel != nullptr) world._2D->Draw(floor);
                            }
                        }
                    }
                }

                //draw junctions (part of this iteration to simplify things)

                uint8_t flags;
                float yOff;
                if (UpJunctions[(size_t)off] == 0) {
                    flags = DownJunctions[(size_t)off];
                    yOff = 0.3f + (level - 1) * 2.95f + blueprint->GetAltitude(x, y);
                } else {
                    flags = UpJunctions[(size_t)off];
                    yOff = level * 2.95f + blueprint->GetAltitude(x, y);
                }

                if (flags > 0 && JunctionMap[flags] >= 0) { //there is a junction here! if the junction map contains the unrotated junction, it will contain the rotated junction.
                    flags = (uint8_t)RotateJunction(rotation, (JunctionFlags)flags);
                    st::Vector3 tilePosition(x - 0.5f, y - 0.5f, yOff); //2.95 for walls up, 0.3 for walls down
                    world._2D->OffsetPixel(world.WorldSpace.GetScreenFromTile(tilePosition));
                    world._2D->OffsetTile(tilePosition);

                    auto _Sprite = world._2D->NewSprite(_2DBatchRenderMode::Z_BUFFER);

                    auto& Junctions = wallContent->Junctions;

                    SPR* sprite = nullptr;
                    switch (zoom) {
                        case WorldZoom::Far:
                            sprite = Junctions.Far;
                            _Sprite->DestRect = JUNCDEST_FAR;
                            _Sprite->Depth = &Z[20];
                            break;
                        case WorldZoom::Medium:
                            sprite = Junctions.Medium;
                            _Sprite->DestRect = JUNCDEST_MED;
                            _Sprite->Depth = &Z[19];
                            break;
                        case WorldZoom::Near:
                            sprite = Junctions.Near;
                            _Sprite->DestRect = JUNCDEST_NEAR;
                            _Sprite->Depth = &Z[18];
                            break;
                    }
                    int frame = JunctionMap[flags];
                    if (sprite != nullptr && frame >= 0 && (size_t)frame < sprite->Frames.size()) {
                        _Sprite->Pixel = world._2D->GetTexture(sprite->Frames[(size_t)frame].get());
                        _Sprite->SrcRect = st::Rectangle(0, 0, _Sprite->Pixel->Width, _Sprite->Pixel->Height);
                        _Sprite->Room = 1;
                        world._2D->Draw(_Sprite);
                    }
                }

                off++;
            }
        }
    }
}

bool WallComponent::OffsetValid(const st::Vector3& off) const {
    return off.X >= 0 && off.Y >= 0 && off.X < blueprint->Width && off.Y < blueprint->Height;
}

st::Vector3 WallComponent::RotateOffset(WorldRotation rot, const st::Vector3& off) {
    switch (rot) {
        case WorldRotation::TopLeft: return off;
        case WorldRotation::TopRight: return st::Vector3(off.Y, -off.X, off.Z);
        case WorldRotation::BottomRight: return st::Vector3(-off.X, -off.Y, off.Z);
        case WorldRotation::BottomLeft: return st::Vector3(-off.Y, off.X, off.Z);
    }
    return off;
}

#define CE(x) ((int)CutawayEdges::x)

void WallComponent::GenerateWallData(const std::vector<WallTile>& walls, const std::vector<int32_t>& wallsAt, bool notTop) {
    auto width = blueprint->Width;
    auto height = blueprint->Height;
    Cuts.assign((size_t)(width * height), WallCuts());
    DownJunctions.assign((size_t)(width * height), 0);
    UpJunctions.assign((size_t)(width * height), 0);

    for (auto off : wallsAt) {
        if (off < 0 || (size_t)off >= walls.size()) continue;
        auto& wall = walls[(size_t)off];
        auto x = off % width;
        auto y = off / width;
        WallCuts result;
        if (notTop && WallsDownAt(x, y)) {
            int cuts = GetCutEdges(off % width, off / width);

            if (wall.TopLeftThick() && wall.TopLeftStyle != 255) {
                if (cuts != 0) {
                    if ((cuts & CE(NegativeX)) == CE(NegativeX)) result.TLCut = WallCut::Up; //if we are on the very edge of the cut we're up
                    else if ((cuts & (CE(PositiveY) | CE(SpecialPositiveY))) > 0) {
                        if ((cuts & (CE(NegativeY) | CE(SpecialNegativeY))) > 0) {
                            result.TLCut = WallCut::Down; //special case, cuts at both sides... just put wall down
                        } else {
                            result.TLCut = WallCut::DownRightUpLeft;
                        }
                    } else if ((cuts & (CE(NegativeY) | CE(SpecialNegativeY))) > 0) {
                        result.TLCut = WallCut::DownLeftUpRight;
                    }
                    else result.TLCut = WallCut::Down;
                } else {
                    result.TLCut = WallCut::Down;
                }
            }

            if (wall.TopRightStyle == 1) { //NOTE: top right style also includes diagonals!
                if (wall.Segments == WallSegments::HorizontalDiag) {
                    if (cuts != 0) {
                        auto cutOnLeft = (cuts & (CE(PositiveY) | CE(NegativeX))) > 0;
                        auto cutOnRight = (cuts & (CE(NegativeY) | CE(PositiveX))) > 0;
                        if (cutOnLeft && cutOnRight) result.TRCut = WallCut::Down;
                        else if (cutOnLeft) result.TRCut = WallCut::DownRightUpLeft;
                        else if (cutOnRight) result.TRCut = WallCut::DownLeftUpRight;
                        else result.TRCut = WallCut::Down;
                    } else {
                        result.TRCut = WallCut::Down;
                    }
                } else if (wall.Segments == WallSegments::VerticalDiag) {
                    if (cuts != 0) { //this info is not useful for front rotation, but is useful for sides.
                        auto cutOnLeft = (cuts & (CE(PositiveY) | CE(PositiveX))) > 0;
                        auto cutOnRight = (cuts & (CE(NegativeY) | CE(NegativeX))) > 0;
                        if (cutOnLeft && cutOnRight) result.TRCut = WallCut::Down;
                        else if (cutOnLeft) result.TRCut = WallCut::DownRightUpLeft;
                        else if (cutOnRight) result.TRCut = WallCut::DownLeftUpRight;
                        else result.TRCut = WallCut::Down;
                    } else {
                        result.TRCut = WallCut::Down;
                    }
                } else {
                    if (cuts != 0) {
                        if ((cuts & CE(NegativeY)) == CE(NegativeY)) result.TRCut = WallCut::Up; //if we are on the very edge of the cut we're up
                        else if ((cuts & (CE(PositiveX) | CE(SpecialPositiveX))) > 0) {
                            if ((cuts & (CE(NegativeX) | CE(SpecialNegativeX))) > 0) { //special case, cuts at both sides... just put wall down
                                result.TRCut = WallCut::Down;
                            } else {
                                result.TRCut = WallCut::DownLeftUpRight;
                            }
                        } else if ((cuts & (CE(NegativeX) | CE(SpecialNegativeX))) > 0) {
                            result.TRCut = WallCut::DownRightUpLeft;
                        }
                        else result.TRCut = WallCut::Down;
                    } else {
                        result.TRCut = WallCut::Down;
                    }
                }
            }
        }

        bool hasXNext = DownJunctions.size() > (size_t)(off + 1);
        bool hasYNext = DownJunctions.size() > (size_t)(off + width);

        //add to relevant junctions
        if (((int)wall.Segments & (int)WallSegments::TopLeft) > 0 && !(wall.TopLeftDoor && (int)result.TLCut > 0) && wall.TopLeftThick()) {
            if ((int)result.TLCut > 0) {
                DownJunctions[(size_t)off] |= JF(BottomLeft);
                if (y < height && hasYNext) DownJunctions[(size_t)(off + width)] |= JF(TopRight);
            } else {
                UpJunctions[(size_t)off] |= JF(BottomLeft);
                if (y < height && hasYNext) UpJunctions[(size_t)(off + width)] |= JF(TopRight);
            }
        }

        if (((int)wall.Segments & (int)WallSegments::TopRight) > 0 && !(wall.TopRightDoor && (int)result.TRCut > 0) && wall.TopRightThick()) {
            if ((int)result.TRCut > 0) {
                DownJunctions[(size_t)off] |= JF(BottomRight);
                if (x < width && hasXNext) DownJunctions[(size_t)(off + 1)] |= JF(TopLeft);
            } else {
                UpJunctions[(size_t)off] |= JF(BottomRight);
                if (x < width && hasXNext) UpJunctions[(size_t)(off + 1)] |= JF(TopLeft);
            }
        }

        if (wall.Segments == WallSegments::VerticalDiag && (wall.TopRightStyle == 1 || wall.TopRightStyle == 255)) {
            if ((int)result.TRCut > 0) {
                DownJunctions[(size_t)off] |= JF(DiagBottom);
                if (x < width && y < height && DownJunctions.size() > (size_t)(off + 1 + width)) DownJunctions[(size_t)(off + 1 + width)] |= JF(DiagTop);
            } else {
                UpJunctions[(size_t)off] |= JF(DiagBottom);
                if (x < width && y < height && DownJunctions.size() > (size_t)(off + 1 + width)) UpJunctions[(size_t)(off + 1 + width)] |= JF(DiagTop);
            }
        } else if (wall.Segments == WallSegments::HorizontalDiag && (wall.TopRightStyle == 1 || wall.TopRightStyle == 255)) {
            if ((int)result.TRCut > 0) {
                if (x < width && hasXNext) DownJunctions[(size_t)(off + 1)] |= JF(DiagLeft);
                if (y < height && hasYNext) DownJunctions[(size_t)(off + width)] |= JF(DiagRight);
            } else {
                if (x < width && hasXNext) UpJunctions[(size_t)(off + 1)] |= JF(DiagLeft);
                if (y < height && hasYNext) UpJunctions[(size_t)(off + width)] |= JF(DiagRight);
            }
        }
        Cuts[(size_t)off] = result;
    }
}

/// Walls Cutaway helper methods.
bool WallComponent::WallsDownAt(int x, int y) const {
    auto& cuts = blueprint->Cutaway;
    int idx = y * blueprint->Width + x;
    // C# indexes the flat array directly (neighbour rows wrap; outside the array throws). We wrap the same way
    // and treat outside-the-array as "not cut".
    if (idx < 0 || (size_t)idx >= cuts.size()) return false;
    return cuts[(size_t)idx];
}

uint8_t WallComponent::GetCutEdges(int x, int y) const { //todo, rotate result for rotations
    int result = 0;
    if (!WallsDownAt(x + 1, y)) result |= CE(PositiveX);
    if (!WallsDownAt(x - 1, y)) result |= CE(NegativeX);
    if (!WallsDownAt(x, y + 1)) result |= CE(PositiveY);
    if (!WallsDownAt(x, y - 1)) result |= CE(NegativeY);
    if (!WallsDownAt(x - 1, y - 1)) result |= CE(SpecialNegativeX) | CE(SpecialNegativeY);
    if (!WallsDownAt(x - 1, y + 1)) result |= CE(SpecialPositiveY);
    if (!WallsDownAt(x + 1, y - 1)) result |= CE(SpecialPositiveX);
    return (uint8_t)result;
}

Wall* WallComponent::GetPattern(uint16_t id) {
    auto it = WallCache.find(id);
    if (it == WallCache.end()) it = WallCache.emplace(id, FSO::Content::Content::Get()->WorldWalls->Get(id, false)).first;
    return it->second;
}

WallStyle* WallComponent::GetStyle(uint16_t id) {
    auto it = WallStyleCache.find(id);
    if (it == WallStyleCache.end()) it = WallStyleCache.emplace(id, FSO::Content::Content::Get()->WorldWalls->GetWallStyle(id)).first;
    return it->second;
}

_2DSprite* WallComponent::GetWallSprite(Wall* pattern, WallStyle* style, int rotation, bool down, WorldState& world) {
    auto _Sprite = world._2D->NewSprite(_2DBatchRenderMode::WALL);
    auto& Z = *WallZBuffers;
    SPR* sprite = nullptr;
    SPR* mask = nullptr;
    switch (world.Zoom()) {
        case WorldZoom::Far:
            sprite = pattern ? pattern->Far : nullptr;
            if (style != nullptr) mask = (down) ? style->WallsDownFar : style->WallsUpFar;
            _Sprite->DestRect = DESTINATION_FAR[rotation];
            _Sprite->Depth = &Z[(size_t)(rotation + 8)];
            break;
        case WorldZoom::Medium:
            sprite = pattern ? pattern->Medium : nullptr;
            if (style != nullptr) mask = (down) ? style->WallsDownMedium : style->WallsUpMedium;
            _Sprite->DestRect = DESTINATION_MED[rotation];
            _Sprite->Depth = &Z[(size_t)(rotation + 4)];
            break;
        case WorldZoom::Near:
            sprite = pattern ? pattern->Near : nullptr;
            if (style != nullptr) mask = (down) ? style->WallsDownNear : style->WallsUpNear;
            _Sprite->DestRect = DESTINATION_NEAR[rotation];
            _Sprite->Depth = &Z[(size_t)rotation];
            break;
    }
    if (sprite != nullptr && (size_t)rotation < sprite->Frames.size()) {
        if (mask == nullptr) mask = sprite;
        _Sprite->Pixel = world._2D->GetTexture(sprite->Frames[(size_t)rotation].get());
        _Sprite->Mask = ((size_t)rotation < mask->Frames.size()) ? world._2D->GetTexture(mask->Frames[(size_t)rotation].get()) : nullptr;
        _Sprite->SrcRect = st::Rectangle(0, 0, _Sprite->Pixel->Width, _Sprite->Pixel->Height);
    }

    _Sprite->Room = (uint16_t)TileRoom;
    return _Sprite;
}

//Gets a floor sprite. Used to draw floors cut in half by walls.
_2DSprite* WallComponent::GetFloorSprite(Floor* pattern, int rotation, WorldState& world, uint8_t cut) {
    auto _Sprite = world._2D->NewSprite(_2DBatchRenderMode::Z_BUFFER);
    if (pattern == nullptr) return _Sprite;
    auto& Z = *WallZBuffers;
    SPR2* sprite = nullptr;
    switch (world.Zoom()) {
        case WorldZoom::Far:
            sprite = pattern->Far;
            _Sprite->DestRect = FLRDEST_FAR;
            _Sprite->Depth = &Z[14];
            break;
        case WorldZoom::Medium:
            sprite = pattern->Medium;
            _Sprite->DestRect = FLRDEST_MED;
            _Sprite->Depth = &Z[13];
            break;
        case WorldZoom::Near:
            sprite = pattern->Near;
            _Sprite->DestRect = FLRDEST_NEAR;
            _Sprite->Depth = &Z[12];
            break;
    }
    if (sprite != nullptr && (size_t)rotation < sprite->Frames.size()) {
        _Sprite->Pixel = world._2D->GetTexture(sprite->Frames[(size_t)rotation].get());
        _Sprite->SrcRect = st::Rectangle(0, 0, _Sprite->Pixel->Width, _Sprite->Pixel->Height);
    }

    switch (cut) {
        case 0: //vertical cut, left side
            _Sprite->DestRect.Width /= 2;
            _Sprite->SrcRect.Width /= 2;
            break;
        case 1: //vertical cut, right side
            _Sprite->DestRect.X += _Sprite->DestRect.Width / 2;
            _Sprite->DestRect.Width /= 2;
            _Sprite->SrcRect.X += _Sprite->SrcRect.Width / 2;
            _Sprite->SrcRect.Width /= 2;
            break;
        case 2: //horizontal cut, top side
            _Sprite->DestRect.Height /= 2;
            _Sprite->SrcRect.Height /= 2;
            break;
        case 3:
            _Sprite->DestRect.Y += _Sprite->DestRect.Height / 2;
            _Sprite->DestRect.Height /= 2;
            _Sprite->SrcRect.Y += _Sprite->SrcRect.Height / 2;
            _Sprite->SrcRect.Height /= 2;
            break;
    }

    _Sprite->Room = (uint16_t)TileRoom;
    return _Sprite;
}

/// Gets rotated junctions and segements
JunctionFlags WallComponent::RotateJunction(WorldRotation rot, JunctionFlags input) {
    auto rotN = (int)rot;
    int rotLower = (((int)input & 15) << rotN);
    int rotHigher = (((int)input & 240) << rotN);
    return (JunctionFlags)(uint8_t)((rotLower & 15) | (rotLower >> 4) | ((rotHigher | rotHigher >> 4) & 240));
}

static inline WallCut FlipCut(WallCut c) {
    if (c == WallCut::DownLeftUpRight) return WallCut::DownRightUpLeft;
    if (c == WallCut::DownRightUpLeft) return WallCut::DownLeftUpRight;
    return c;
}

WallCuts WallComponent::RotateCuts(WorldRotation rot, WallCuts input, int16_t x, int16_t y) {
    int rotN = (int)rot;
    WallCuts output;
    switch (rotN) {
        case 0:
            return input;
        case 1:
            output.TRCut = input.TLCut;
            if (y + 1 < blueprint->Height) output.TLCut = Cuts[(size_t)((y + 1) * blueprint->Width + x)].TRCut;
            output.TLCut = FlipCut(output.TLCut); //flip cut
            return output;
        case 2:
            output.TRCut = input.TLCut;
            if (y + 1 < blueprint->Height) output.TRCut = Cuts[(size_t)((y + 1) * blueprint->Width + x)].TRCut;
            if (x + 1 < blueprint->Width) output.TLCut = Cuts[(size_t)(y * blueprint->Width + x + 1)].TLCut;
            output.TLCut = FlipCut(output.TLCut); //flip cuts
            output.TRCut = FlipCut(output.TRCut);
            return output;
        case 3:
            output.TLCut = input.TRCut;
            if (x + 1 < blueprint->Width) output.TRCut = Cuts[(size_t)(y * blueprint->Width + x + 1)].TLCut;
            output.TRCut = FlipCut(output.TRCut); //flip cut
            return output;
    }
    return output;
}

WallTile WallComponent::RotateWall(WorldRotation rot, const WallTile& input, int16_t x, int16_t y, int8_t level) {
    int rotN = (int)rot;
    WallTile output;
    if (rot != WorldRotation::TopLeft) {
        if (input.Segments == WallSegments::HorizontalDiag) {
            output.Segments = ((rotN % 2) == 0) ? WallSegments::HorizontalDiag : WallSegments::VerticalDiag;
            output.TopRightStyle = input.TopRightStyle;
            switch (rotN) {
                case 1:
                    output.BottomRightPattern = input.BottomLeftPattern;
                    output.BottomLeftPattern = input.BottomRightPattern;
                    output.TopLeftStyle = input.TopLeftPattern;
                    output.TopLeftPattern = input.TopLeftStyle;
                    break;
                case 2:
                    output.BottomRightPattern = input.BottomLeftPattern; //flip sides
                    output.BottomLeftPattern = input.BottomRightPattern;
                    output.TopLeftStyle = input.TopLeftPattern;
                    output.TopLeftPattern = input.TopLeftStyle;
                    break;
                case 3:
                    output.BottomRightPattern = input.BottomRightPattern;
                    output.BottomLeftPattern = input.BottomLeftPattern;
                    output.TopLeftStyle = input.TopLeftStyle;
                    output.TopLeftPattern = input.TopLeftPattern;
                    break;
            }
        } else if (input.Segments == WallSegments::VerticalDiag) {
            output.Segments = ((rotN % 2) == 0) ? WallSegments::VerticalDiag : WallSegments::HorizontalDiag;
            output.TopRightStyle = input.TopRightStyle;
            switch (rotN) {
                case 1:
                    output.BottomRightPattern = input.BottomRightPattern;
                    output.BottomLeftPattern = input.BottomLeftPattern;
                    output.TopLeftStyle = input.TopLeftStyle;
                    output.TopLeftPattern = input.TopLeftPattern;
                    break;
                case 2:
                    output.BottomRightPattern = input.BottomLeftPattern; //flip sides
                    output.BottomLeftPattern = input.BottomRightPattern;
                    output.TopLeftStyle = input.TopLeftPattern;
                    output.TopLeftPattern = input.TopLeftStyle;
                    break;
                case 3:
                    output.BottomRightPattern = input.BottomLeftPattern;
                    output.BottomLeftPattern = input.BottomRightPattern;
                    output.TopLeftStyle = input.TopLeftPattern;
                    output.TopLeftPattern = input.TopLeftStyle;
                    break;
            }
        } else {
            switch (rotN) {
                case 1:
                    if (((int)input.Segments & (int)WallSegments::TopLeft) > 0) output.Segments |= WallSegments::TopRight;
                    if (((int)input.Segments & (int)WallSegments::TopRight) > 0) output.Segments |= WallSegments::BottomRight;
                    if (((int)input.Segments & (int)WallSegments::BottomRight) > 0) output.Segments |= WallSegments::BottomLeft;
                    if (((int)input.Segments & (int)WallSegments::BottomLeft) > 0) output.Segments |= WallSegments::TopLeft;
                    output.TopLeftPattern = input.BottomLeftPattern;
                    output.TopRightPattern = input.TopLeftPattern;
                    output.BottomRightPattern = input.TopRightPattern;
                    output.BottomLeftPattern = input.BottomRightPattern;

                    if (y + 1 < blueprint->Height) {
                        auto newLeft = blueprint->GetWall(x, (int16_t)(y + 1), level);
                        output.TopLeftStyle = newLeft.TopRightStyle;
                        output.ObjSetTLStyle = newLeft.ObjSetTRStyle;
                        output.TopLeftDoor = newLeft.TopRightDoor;
                    }

                    output.TopRightStyle = input.TopLeftStyle;
                    output.ObjSetTRStyle = input.ObjSetTLStyle;
                    output.TopRightDoor = input.TopLeftDoor;
                    break;
                case 2:
                    if (((int)input.Segments & (int)WallSegments::TopLeft) > 0) output.Segments |= WallSegments::BottomRight;
                    if (((int)input.Segments & (int)WallSegments::TopRight) > 0) output.Segments |= WallSegments::BottomLeft;
                    if (((int)input.Segments & (int)WallSegments::BottomRight) > 0) output.Segments |= WallSegments::TopLeft;
                    if (((int)input.Segments & (int)WallSegments::BottomLeft) > 0) output.Segments |= WallSegments::TopRight;
                    output.TopLeftPattern = input.BottomRightPattern;
                    output.TopRightPattern = input.BottomLeftPattern;
                    output.BottomRightPattern = input.TopLeftPattern;
                    output.BottomLeftPattern = input.TopRightPattern;

                    if (y + 1 < blueprint->Height) {
                        auto newRight = blueprint->GetWall(x, (int16_t)(y + 1), level);
                        output.TopRightStyle = newRight.TopRightStyle;
                        output.ObjSetTRStyle = newRight.ObjSetTRStyle;
                        output.TopRightDoor = newRight.TopRightDoor;
                    }

                    if (x + 1 < blueprint->Width) {
                        auto newLeft = blueprint->GetWall((int16_t)(x + 1), y, level);
                        output.TopLeftStyle = newLeft.TopLeftStyle;
                        output.ObjSetTLStyle = newLeft.ObjSetTLStyle;
                        output.TopLeftDoor = newLeft.TopLeftDoor;
                    }
                    break;
                case 3:
                    if (((int)input.Segments & (int)WallSegments::TopLeft) > 0) output.Segments |= WallSegments::BottomLeft;
                    if (((int)input.Segments & (int)WallSegments::TopRight) > 0) output.Segments |= WallSegments::TopLeft;
                    if (((int)input.Segments & (int)WallSegments::BottomRight) > 0) output.Segments |= WallSegments::TopRight;
                    if (((int)input.Segments & (int)WallSegments::BottomLeft) > 0) output.Segments |= WallSegments::BottomRight;
                    output.TopLeftPattern = input.TopRightPattern;
                    output.TopRightPattern = input.BottomRightPattern;
                    output.BottomRightPattern = input.BottomLeftPattern;
                    output.BottomLeftPattern = input.TopLeftPattern;

                    output.TopLeftStyle = input.TopRightStyle;
                    output.TopLeftDoor = input.TopRightDoor;
                    output.ObjSetTLStyle = input.ObjSetTRStyle;

                    if (x + 1 < blueprint->Width) {
                        auto newRight = blueprint->GetWall((int16_t)(x + 1), y, level);
                        output.TopRightStyle = newRight.TopLeftStyle;
                        output.ObjSetTRStyle = newRight.ObjSetTLStyle;
                        output.TopRightDoor = newRight.TopLeftDoor;
                    }
                    break;
            }
        }
    } else {
        output = input;
    }
    return output;
}

_2DSprite* WallComponent::CopySprite(WorldState& world, _2DSprite* _Sprite) {
    auto spr = world._2D->NewSprite(_2DBatchRenderMode::WALL);
    spr->DestRect = _Sprite->DestRect;
    spr->SrcRect = _Sprite->SrcRect;
    spr->Pixel = _Sprite->Pixel;
    spr->Depth = _Sprite->Depth;
    spr->Room = _Sprite->Room;
    return spr;
}

}}}
