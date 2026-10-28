/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/formats/iff/chunks/DGRP.cs
#include "files/formats/iff/chunks/DGRP.h"
#include "files/formats/iff/chunks/SPR.h"
#include "files/formats/iff/chunks/SPR2.h"
#include "files/formats/iff/IffFile.h"

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

DGRPImage* DGRP::GetImage(uint32_t direction, uint32_t zoom, uint32_t worldRotation){

    uint32_t rotatedDirection = 0;

    /**LeftFront = 0x10,
    LeftBack = 0x40,
    RightFront = 0x04,
    RightBack = 0x01**/
    int32_t rotateBits = (int32_t)direction << ((int32_t)worldRotation * 2);
    rotatedDirection = (uint32_t)((rotateBits & 255) | (rotateBits >> 8));

    for (DGRPImage& image : Images)
    {
        if (image.Direction == rotatedDirection && image.Zoom == zoom)
        {
            return &image;
        }
    }
    return nullptr;
}

void DGRP::Read(IffFile* iff, st::Reader& stream)
{
    (void)iff;
    st::Reader& io = stream;
    io.ByteOrder(st::Endian::Little);
    {
        auto version = io.ReadUInt16();
        uint32_t imageCount = version < 20003 ? io.ReadUInt16() : io.ReadUInt32();
        Images.clear();
        Images.reserve(imageCount);

        for (uint32_t i = 0; i < imageCount; i++)
        {
            DGRPImage image(this);
            image.Read(version, io);
            Images.push_back(image);
        }
    }
}

bool DGRP::Write(IffFile* iff, st::Writer& stream)
{
    (void)iff;
    st::Writer& io = stream;
    io.ByteOrder(st::Endian::Little);
    {
        io.WriteUInt16(20004);
        io.WriteUInt32((uint32_t)Images.size());

        for (auto& img : Images)
        {
            img.Write(io);
        }
    }
    return true;
}

void DGRPImage::Read(uint32_t version, st::Reader& io)
{
    uint32_t spriteCount = 0;
    if (version < 20003){
        spriteCount = io.ReadUInt16();
        Direction = io.ReadByte();
        Zoom = io.ReadByte();
    }else{
        Direction = io.ReadUInt32();
        Zoom = io.ReadUInt32();
        spriteCount = io.ReadUInt32();
    }

    this->Sprites.clear();
    this->Sprites.reserve(spriteCount);
    for (uint32_t i = 0; i < spriteCount; i++){
        DGRPSprite sprite(Parent);
        sprite.Read(version, io);
        this->Sprites.push_back(sprite);
    }
}

void DGRPImage::Write(st::Writer& io)
{
    io.WriteUInt32(Direction);
    io.WriteUInt32(Zoom);
    io.WriteUInt32((uint32_t)Sprites.size());
    for (auto& spr : Sprites)
    {
        spr.Write(io);
    }
}

void DGRPSprite::Read(uint32_t version, st::Reader& io)
{
    if (version < 20003)
    {
        //Unknown ignored "Type" field
        auto type = io.ReadUInt16();
        (void)type;
        SpriteID = io.ReadUInt16();
        SpriteFrameIndex = io.ReadUInt16();

        auto flagsRaw = io.ReadUInt16();
        Flags = (DGRPSpriteFlags)flagsRaw;

        SpriteOffset.X = io.ReadInt16();
        SpriteOffset.Y = io.ReadInt16();

        if (version == 20001)
        {
            ObjectOffset.Z = io.ReadFloat();
        }
    }
    else
    {
        SpriteID = io.ReadUInt32();
        SpriteFrameIndex = io.ReadUInt32();
        SpriteOffset.X = (float)io.ReadInt32();
        SpriteOffset.Y = (float)io.ReadInt32();
        ObjectOffset.Z = io.ReadFloat();
        Flags = (DGRPSpriteFlags)io.ReadUInt32();
        if (version == 20004)
        {
            ObjectOffset.X = io.ReadFloat();
            ObjectOffset.Y = io.ReadFloat();
        }
    }
}

void DGRPSprite::Write(st::Writer& io)
{
    io.WriteUInt32(SpriteID);
    io.WriteUInt32(SpriteFrameIndex);
    io.WriteInt32((int32_t)SpriteOffset.X);
    io.WriteInt32((int32_t)SpriteOffset.Y);
    io.WriteFloat(ObjectOffset.Z);
    io.WriteUInt32((uint32_t)Flags);
    io.WriteFloat(ObjectOffset.X);
    io.WriteFloat(ObjectOffset.Y);
}

Vector2 DGRPSprite::GetPosition()
{
    auto iff = Parent->ChunkParent;
    auto spr2 = iff->Get<SPR2>((uint16_t)this->SpriteID);
    if (spr2 != nullptr)
    {
        return spr2->Frames[this->SpriteFrameIndex]->Position;
    }
    return Vector2(0, 0);
}

Utils::Texture2D* DGRPSprite::GetTexture(Utils::GraphicsDevice* device){
    auto iff = Parent->ChunkParent;
    auto spr2 = iff->Get<SPR2>((uint16_t)this->SpriteID);
    if (spr2 != nullptr){
        return spr2->Frames[this->SpriteFrameIndex]->GetTexture(device);
    }
    auto spr1 = iff->Get<SPR>((uint16_t)this->SpriteID);
    if (spr1 != nullptr){
        return spr1->Frames[(int32_t)this->SpriteFrameIndex]->GetTexture(device);
    }
    return nullptr;
}

Utils::WorldTexture* DGRPSprite::GetWorldTexture(Utils::GraphicsDevice* device)
{
    auto iff = Parent->ChunkParent;
    auto spr2 = iff->Get<SPR2>((uint16_t)this->SpriteID);
    if (spr2 != nullptr)
    {
        return spr2->Frames[this->SpriteFrameIndex]->GetWorldTexture(device);
    }
    auto spr1 = iff->Get<SPR>((uint16_t)this->SpriteID);
    if (spr1 != nullptr)
    {
        WorldTextureCache = Utils::WorldTexture();
        WorldTextureCache.Pixel = spr1->Frames[(int32_t)this->SpriteFrameIndex]->GetTexture(device);
        return &WorldTextureCache;
    }
    return nullptr;
}

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
