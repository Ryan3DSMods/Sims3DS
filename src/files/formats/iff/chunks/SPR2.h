/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/formats/iff/chunks/SPR2.cs
#pragma once
#include "files/formats/iff/AbstractIffChunk.h"
#include "files/formats/iff/chunks/PALT.h"
#include "files/utils/ITextureProvider.h"
#include <memory>

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

class SPR2;

/// <summary>
/// The frame (I.E sprite) of a SPR2 chunk.
/// </summary>
class SPR2Frame : public Utils::ITextureProvider, public Utils::IWorldTextureProvider
{
public:
    // FIX(sims) memory: frames decoded from a file are stored palettised: PalData (index) + AlphaData (alpha) +
    // ZBufferData, 3 bytes per pixel, the colour being PalColors[index] with that alpha (PixelData stays empty).
    // Was PixelData (RGBA, 4) + PalData + ZBufferData + an AlphaData that was allocated but never written: 7 bytes.
    // The renderer looks the palette up per pixel (_2DWorldBatch::RenderSprite): same output for every drawn pixel
    // (pixels with alpha 0 are never drawn; only their unused RGB differs). PixelData is used by frames built
    // with SetData (editor path); Color(i)/ExpandColors() give RGBA either way.
    std::vector<Color> PixelData;
    std::vector<uint8_t> AlphaData;
    std::vector<uint8_t> ZBufferData;
    std::vector<uint8_t> PalData;
    const Color* PalColors = nullptr;
    bool HasColor() const { return !PixelData.empty() || (PalColors != nullptr && !PalData.empty()); }
    uint8_t AlphaAt(size_t i) const { return !PixelData.empty() ? PixelData[i].A : AlphaData[i]; }
    Color ColorAt(size_t i) const {
        if (!PixelData.empty()) return PixelData[i];
        Color c = PalColors[PalData[i]];
        c.A = AlphaData[i];
        return c;
    }
    std::vector<Color> ExpandColors() const;

private:
    Utils::Texture2D* ZCache = nullptr;
    Utils::Texture2D* PixelCache = nullptr;
    Utils::WorldTexture WorldTextureCache;

public:
    int32_t Width = 0;
    int32_t Height = 0;
    uint32_t Flags = 0;
    uint16_t PaletteID = 0;
    uint16_t TransparentColorIndex = 0;
    Vector2 Position;

private:
    SPR2* Parent;
    uint32_t Version = 0;
    std::vector<uint8_t> ToDecode;
    bool HasToDecode = false;

public:
    explicit SPR2Frame(SPR2* parent)
    {
        this->Parent = parent;
    }

    // TS1(sims): sprite memory between zoom levels (an object's SPR2 holds the frames of all three zooms).
    // KeepCompressed frames keep their compressed bytes after decoding, so DropDecoded can free the decoded pixels
    // (~7 bytes per pixel) and DecodeIfRequired rebuilds them on next use. Returns the bytes freed.
    bool KeepCompressed = false;
    size_t DropDecoded();
    // FIX(sims): frames of a deferred (on-disk) object SPR2 do not keep their compressed bytes in memory: they are
    // read again from the file (FilePool::ReadAt, one seek + read) when decoded. The compressed frames of all three
    // zooms (near = 4x medium) were ~27 MB on a big lot (Magic Town 96), the largest item of the lot's heap.
    int64_t SrcOffset = -1;
    uint32_t SrcSize = 0;
    /// FIX(sims): IffFile::DetachSource: keep the compressed bytes in memory (the file is about to be rewritten).
    void DetachFromSource();

    /// <summary>
    /// Reads a SPR2 chunk from a stream.
    /// </summary>
    /// <param name="version">Version of the SPR2 that this frame belongs to.</param>
    /// <param name="io">A IOBuffer object used to read a SPR2 chunk.</param>
    void Read(uint32_t version, st::Reader& io, uint32_t guessedSize);

    void ReadDeferred(uint32_t version, st::Reader& io);

    void DecodeIfRequired();

    void Write(st::Writer& io);

private:
    /// <summary>
    /// Decodes this SPR2Frame.
    /// </summary>
    /// <param name="io">An IOBuffer instance used to read a SPR2Frame.</param>
    void Decode(st::Reader& io);

public:
    /// <summary>
    /// Gets a pixel from this SPR2Frame.
    /// </summary>
    Color GetPixel(int32_t x, int32_t y)
    {
        return ColorAt((size_t)(y * Width) + x);
    }

    /// <summary>
    /// Sets a pixel in this SPR2Frame.
    /// </summary>
    void SetPixel(int32_t x, int32_t y, Color color)
    {
        if (PixelData.empty() && HasColor()) PixelData = ExpandColors(); // FIX(sims): editing leaves palette mode
        PixelData[(y * Width) + x] = color;
    }

    // STUB(3ds): Texture2D upload. Decodes the frame (PixelData stays valid) and returns nullptr.
    Utils::Texture2D* GetTexture(Utils::GraphicsDevice* device) override;

    // STUB(3ds): Alpha8 z-texture upload. Decodes the frame (ZBufferData stays valid) and returns nullptr.
    Utils::Texture2D* GetZTexture(Utils::GraphicsDevice* device);

    Utils::WorldTexture* GetWorldTexture(Utils::GraphicsDevice* device) override;

    std::vector<Color> SetData(const std::vector<Color>& px, const std::vector<uint8_t>& zpx, Rectangle rect);

    void SetPalt(PALT* p);
};

/// <summary>
/// This chunk type holds a number of paletted sprites that may have z-buffer and/or alpha channels.
/// </summary>
class SPR2 : public IffChunk
{
public:
    IFF_CHUNK_TYPE("SPR2")

    std::vector<std::unique_ptr<SPR2Frame>> Frames;
    uint32_t DefaultPaletteID = 0;

    /// <summary>
    /// Reads a SPR2 chunk from a stream.
    /// </summary>
    /// <param name="iff">An Iff instance.</param>
    /// <param name="stream">A Stream object holding a SPR2 chunk.</param>
    void Read(IffFile* iff, st::Reader& stream) override;

    bool Write(IffFile* iff, st::Writer& stream) override;

    void Dispose() override;
};

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
