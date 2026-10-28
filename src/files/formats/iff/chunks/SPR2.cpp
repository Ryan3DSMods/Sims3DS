/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/formats/iff/chunks/SPR2.cs
#include "files/FilePool.h"
#include "files/formats/iff/chunks/SPR2.h"
#include "files/formats/iff/chunks/SPR2FrameEncoder.h"
#include "files/formats/iff/IffFile.h"

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

void SPR2::Read(IffFile* iff, st::Reader& stream)
{
    (void)iff;
    st::Reader& io = stream;
    io.ByteOrder(st::Endian::Little);
    {
        auto version = io.ReadUInt32();
        uint32_t spriteCount = 0;

        if (version == 1000)
        {
            spriteCount = io.ReadUInt32();
            DefaultPaletteID = io.ReadUInt32();
            std::vector<uint32_t> offsetTable(spriteCount);
            for (uint32_t i = 0; i < spriteCount; i++)
            {
                offsetTable[i] = io.ReadUInt32();
            }

            Frames.clear();
            Frames.reserve(spriteCount);
            for (uint32_t i = 0; i < spriteCount; i++)
            {
                auto frame = new SPR2Frame(this);
                frame->KeepCompressed = SourceOffset >= 0; // TS1(sims): object sprites (DropDecoded)
                Frames.emplace_back(frame);
                io.Seek(st::SeekOrigin::Begin, offsetTable[i]);

                auto guessedSize = ((i + 1 < offsetTable.size()) ? offsetTable[i + 1] : (uint32_t)stream.Length()) - offsetTable[i];

                frame->Read(version, io, guessedSize);
            }
        }
        else if (version == 1001)
        {
            DefaultPaletteID = io.ReadUInt32();
            spriteCount = io.ReadUInt32();

            Frames.clear();
            Frames.reserve(spriteCount);
            for (uint32_t i = 0; i < spriteCount; i++)
            {
                auto frame = new SPR2Frame(this);
                frame->KeepCompressed = SourceOffset >= 0; // TS1(sims): object sprites (DropDecoded)
                Frames.emplace_back(frame);
                frame->Read(version, io, 0);
            }
        }
    }
}

bool SPR2::Write(IffFile* iff, st::Writer& stream)
{
    (void)iff;
    st::Writer& io = stream;
    io.ByteOrder(st::Endian::Little);
    {
        io.WriteUInt32(1001);
        io.WriteUInt32(DefaultPaletteID);
        {
            io.WriteUInt32((uint32_t)Frames.size());
            for (auto& frame : Frames)
            {
                frame->Write(io);
            }
        }
        return true;
    }
}

void SPR2::Dispose()
{
    for (auto& frame : Frames)
    {
        auto palette = ChunkParent->Get<PALT>(frame->PaletteID);
        if (palette != nullptr) palette->References--;
    }
}

void SPR2Frame::Read(uint32_t version, st::Reader& io, uint32_t guessedSize)
{
    Version = version;
    // FIX(sims): see SPR2Frame::SrcOffset
    bool onDisk = KeepCompressed && Parent != nullptr && Parent->SourceOffset >= 0 && Parent->ChunkParent != nullptr
                  && !Parent->ChunkParent->SourcePath.empty();
    auto lazy = [&](uint32_t size) {
        size_t left = io.Length() > io.pos() ? (size_t)(io.Length() - io.pos()) : 0;
        if (size > left) size = (uint32_t)left;
        SrcOffset = Parent->SourceOffset + 76 + (int64_t)io.pos();
        SrcSize = size;
        io.Skip(size);
        HasToDecode = true;
    };
    if (version == 1001)
    {
        auto spriteVersion = io.ReadUInt32();
        (void)spriteVersion;
        auto spriteSize = io.ReadUInt32();
        if (IffFile::RETAIN_CHUNK_DATA) ReadDeferred(1001, io);
        else if (onDisk) lazy(spriteSize);
        else { ToDecode = io.ReadBytes(spriteSize); HasToDecode = true; }
    } else
    {
        if (IffFile::RETAIN_CHUNK_DATA) ReadDeferred(1000, io);
        else if (onDisk) lazy(guessedSize);
        else { ToDecode = io.ReadBytes(guessedSize); HasToDecode = true; }
    }
}

void SPR2Frame::ReadDeferred(uint32_t version, st::Reader& io)
{
    this->Width = io.ReadUInt16();
    this->Height = io.ReadUInt16();
    this->Flags = io.ReadUInt32();
    this->PaletteID = io.ReadUInt16();

    if (version == 1000 || this->PaletteID == 0 || this->PaletteID == 0xA3A3)
    {
        this->PaletteID = (uint16_t)Parent->DefaultPaletteID;
    }

    TransparentColorIndex = io.ReadUInt16();

    auto y = io.ReadInt16();
    auto x = io.ReadInt16();
    this->Position = Vector2(x, y);

    this->Decode(io);
}

void SPR2Frame::DecodeIfRequired()
{
    if (HasToDecode)
    {
        if (ToDecode.empty() && SrcOffset >= 0) // FIX(sims): compressed bytes are read back from the file
        {
            std::string err;
            if (!FSO::Files::FilePool::ReadAt(Parent->ChunkParent->SourcePath, SrcOffset, SrcSize, ToDecode, &err))
            {
                ToDecode.clear();
                HasToDecode = false; // an unreadable frame stays empty (as a broken frame would)
                return;
            }
        }
        {
            st::Reader buf(ToDecode, st::Endian::Little);
            ReadDeferred(Version, buf);
        }

        if (!KeepCompressed || SrcOffset >= 0) // TS1(sims): kept for DropDecoded unless it can be read again
        {
            ToDecode.clear();
            ToDecode.shrink_to_fit();
        }
        HasToDecode = false;
    }
}

std::vector<Color> SPR2Frame::ExpandColors() const // FIX(sims): see SPR2.h
{
    if (!PixelData.empty()) return PixelData;
    std::vector<Color> out;
    if (!HasColor()) return out;
    out.resize(PalData.size());
    for (size_t i = 0; i < out.size(); i++) out[i] = ColorAt(i);
    return out;
}

void SPR2Frame::DetachFromSource() // FIX(sims): see SPR2.h
{
    if (SrcOffset < 0) return;
    if (ToDecode.empty()) {
        std::string err;
        if (!FSO::Files::FilePool::ReadAt(Parent->ChunkParent->SourcePath, SrcOffset, SrcSize, ToDecode, &err)) ToDecode.clear();
    }
    SrcOffset = -1;
    KeepCompressed = true;
    if (ToDecode.empty()) { KeepCompressed = false; HasToDecode = false; } // unreadable: stays as it is (or empty)
}

size_t SPR2Frame::DropDecoded() // TS1(sims): see SPR2.h
{
    if (HasToDecode) return 0;
    if (SrcOffset < 0 && (!KeepCompressed || ToDecode.empty())) return 0;
    size_t bytes = PixelData.capacity() * sizeof(Color) + AlphaData.capacity() + ZBufferData.capacity() + PalData.capacity();
    std::vector<Color>().swap(PixelData);
    std::vector<uint8_t>().swap(AlphaData);
    std::vector<uint8_t>().swap(ZBufferData);
    std::vector<uint8_t>().swap(PalData);
    HasToDecode = true;
    return bytes;
}

void SPR2Frame::Write(st::Writer& io)
{
    {
        st::Writer sprIO(st::Endian::Little);
        sprIO.WriteUInt16((uint16_t)Width);
        sprIO.WriteUInt16((uint16_t)Height);
        sprIO.WriteUInt32(Flags);
        sprIO.WriteUInt16(PaletteID);
        sprIO.WriteUInt16(TransparentColorIndex);
        sprIO.WriteUInt16((uint16_t)(int16_t)Position.Y);
        sprIO.WriteUInt16((uint16_t)(int16_t)Position.X);
        SPR2FrameEncoder::WriteFrame(this, sprIO);

        auto& data = sprIO.ToArray();
        io.WriteUInt32(1001);
        io.WriteUInt32((uint32_t)data.size());
        io.WriteBytes(data);
    }
}

void SPR2Frame::Decode(st::Reader& io)
{
    int32_t y = 0;
    bool endmarker = false;

    bool hasPixels = (this->Flags & 0x01) == 0x01;
    bool hasZBuffer = (this->Flags & 0x02) == 0x02;
    bool hasAlpha = (this->Flags & 0x04) == 0x04;

    size_t numPixels = (size_t)this->Width * this->Height;
    (void)hasAlpha; // FIX(sims): alpha lives in AlphaData whenever there are pixels (see SPR2.h)
    std::vector<Color>().swap(this->PixelData);
    if (hasPixels){
        this->PalData = std::vector<uint8_t>(numPixels);
        this->AlphaData = std::vector<uint8_t>(numPixels); // 0 = untouched / transparent
    }
    if (hasZBuffer){
        this->ZBufferData = std::vector<uint8_t>(numPixels);
    }

    static PALT fallbackPalette; // FIX(sims): frames keep a pointer to their palette's colours
    if (fallbackPalette.Colors.size() < 256) fallbackPalette.Colors = std::vector<Color>(256);
    auto palette = Parent->ChunkParent->Get<PALT>(this->PaletteID);
    if (palette == nullptr) palette = &fallbackPalette;
    if (palette->Colors.size() < 256) palette->Colors.resize(256); // padded once, before any frame points at it
    palette->References++;
    PalColors = palette->Colors.data();

    while (!endmarker)
    {
        auto marker = io.ReadUInt16();
        int32_t command = marker >> 13;
        int32_t count = marker & 0x1FFF;

        switch (command)
        {
            /** Fill with pixel data **/
            case 0x00:
            {
                int32_t bytes = count;
                bytes -= 2;

                int32_t x = 0;

                while (bytes > 0)
                {
                    auto pxMarker = io.ReadUInt16();
                    int32_t pxCommand = pxMarker >> 13;
                    int32_t pxCount = pxMarker & 0x1FFF;
                    bytes -= 2;

                    switch (pxCommand)
                    {
                        case 0x01:
                        case 0x02:
                        {
                            bool pxWithAlpha = pxCommand == 0x02;
                            for (int32_t col = 0; col < pxCount; col++)
                            {
                                auto zValue = io.ReadByte();
                                auto pxValue = io.ReadByte();
                                bytes -= 2;

                                uint8_t pxA = palette->Colors[pxValue].A;
                                if (pxWithAlpha)
                                {
                                    auto alpha = io.ReadByte();
                                    pxA = (uint8_t)(alpha * 8.2258064516129032258064516129032);
                                    bytes--;
                                }
                                //this mode draws the transparent colour as solid for some reason.
                                //fixes backdrop theater
                                size_t offset = (size_t)(y * Width) + x;
                                //C# indexes PixelData/ZBufferData unconditionally here (NRE if absent); guarded.
                                if (hasPixels) {
                                    this->AlphaData[offset] = pxA;
                                    this->PalData[offset] = pxValue;
                                }
                                if (hasZBuffer) this->ZBufferData[offset] = zValue;
                                x++;
                            }
                            if (pxWithAlpha)
                            {
                                /** Padding? **/
                                if ((pxCount * 3) % 2 != 0){
                                    bytes--;
                                    io.ReadByte();
                                }
                            }
                            break;
                        }
                        case 0x03:
                            for (int32_t col = 0; col < pxCount; col++)
                            {
                                size_t offset = (size_t)(y * Width) + x;
                                if (hasPixels) {
                                    this->AlphaData[offset] = 0;
                                    this->PalData[offset] = (uint8_t)TransparentColorIndex;
                                }
                                if (hasZBuffer){
                                    this->ZBufferData[offset] = 255;
                                }
                                x++;
                            }
                            break;
                        case 0x06:
                            for (int32_t col = 0; col < pxCount; col++)
                            {
                                auto pxIndex = io.ReadByte();
                                bytes--;
                                size_t offset = (size_t)(y * Width) + x;
                                uint8_t pxA = palette->Colors[pxIndex].A;
                                uint8_t z = 0;

                                //not sure if this should happen
                                /*if (pxIndex == TransparentColorIndex)
                                {
                                    pxColor.A = 0;
                                    z = 255;
                                }*/
                                if (hasPixels) {
                                    this->AlphaData[offset] = pxA;
                                    this->PalData[offset] = pxIndex;
                                }
                                if (hasZBuffer)
                                {
                                    this->ZBufferData[offset] = z;
                                }
                                x++;
                            }
                            if (pxCount % 2 != 0)
                            {
                                bytes--;
                                io.ReadByte();
                            }
                            break;
                    }
                }

                /** If row isnt filled in, the rest is transparent **/
                while (x < Width)
                {
                    size_t offset = (size_t)(y * Width) + x;
                    if (hasZBuffer)
                    {
                        this->ZBufferData[offset] = 255;
                    }
                    x++;
                }
                break;
            }
            /**  Leave the next count rows in the color channel filled with the transparent color,
             * in the z-buffer channel filled with 255, and in the alpha channel filled with 0. **/
            case 0x04:
                for (int32_t row = 0; row < count; row++)
                {
                    for (int32_t col = 0; col < Width; col++)
                    {
                        size_t offset = (size_t)((y+row) * Width) + col;
                        if (hasPixels)
                        {
                            this->AlphaData[offset] = 0;
                            this->PalData[offset] = (uint8_t)TransparentColorIndex;
                        }
                        if (hasZBuffer)
                        {
                            ZBufferData[offset] = 255;
                        }
                    }
                }
                y += count - 1;
                break;
            case 0x05:
                endmarker = true;
                break;
        }
        y++;
    }
}

Utils::Texture2D* SPR2Frame::GetTexture(Utils::GraphicsDevice* device)
{
    (void)device;
    DecodeIfRequired();
    // STUB(3ds): C# builds a Texture2D(Width, Height) from PixelData (null when 0-sized) and
    // drops PixelData unless RETAIN_CHUNK_DATA. PixelData is retained here; no texture.
    return PixelCache;
}

Utils::Texture2D* SPR2Frame::GetZTexture(Utils::GraphicsDevice* device)
{
    (void)device;
    DecodeIfRequired();
    // STUB(3ds): C# builds an Alpha8 Texture2D from ZBufferData. ZBufferData is retained here.
    return ZCache;
}

Utils::WorldTexture* SPR2Frame::GetWorldTexture(Utils::GraphicsDevice* device)
{
    WorldTextureCache.Pixel = this->GetTexture(device);
    WorldTextureCache.ZBuffer = this->GetZTexture(device);
    return &WorldTextureCache;
}

std::vector<Color> SPR2Frame::SetData(const std::vector<Color>& px, const std::vector<uint8_t>& zpx, Rectangle rect)
{
    PixelCache = nullptr; //can't exactly dispose this.. it's likely still in use!
    ZCache = nullptr;
    PixelData = px;
    ZBufferData = zpx;
    Position = Vector2((float)rect.X, (float)rect.Y);

    Width = rect.Width;
    Height = rect.Height;
    Flags = 7;
    TransparentColorIndex = 255;

    auto colors = SPR2FrameEncoder::QuantizeFrame(this, PalData);

    std::vector<Color> palt(256);
    int i = 0;
    for (auto& c : colors)
        palt[i++] = Color(c.R, c.G, c.B, (uint8_t)255);

    return palt;
}

void SPR2Frame::SetPalt(PALT* p)
{
    if (this->PaletteID != 0)
    {
        auto old = Parent->ChunkParent->Get<PALT>(this->PaletteID);
        if (old != nullptr) old->References--;
    }
    PaletteID = p->ChunkID;
    p->References++;
}

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
