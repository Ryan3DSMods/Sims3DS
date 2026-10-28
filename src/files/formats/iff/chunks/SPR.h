/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/formats/iff/chunks/SPR.cs
#pragma once
#include "files/formats/iff/AbstractIffChunk.h"
#include "files/formats/iff/chunks/PALT.h"
#include "files/utils/ITextureProvider.h"
#include <memory>

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

class SPR;

/// <summary>
/// The frame (I.E sprite) of a SPR chunk.
/// </summary>
class SPRFrame : public Utils::ITextureProvider
{
public:
    static PALT DEFAULT_PALT;

    uint32_t Version = 0;
private:
    SPR* Parent;
    Utils::Texture2D* PixelCache = nullptr;
    std::vector<uint8_t> ToDecode;
    bool HasToDecode = false;

public:
    /// <summary>
    /// Constructs a new SPRFrame instance.
    /// </summary>
    /// <param name="parent">A SPR parent.</param>
    explicit SPRFrame(SPR* parent)
    {
        this->Parent = parent;
    }

    /// <summary>
    /// Reads a SPRFrame from a stream.
    /// </summary>
    void Read(uint32_t version, st::Reader& io, uint32_t guessedSize);

    void ReadDeferred(uint32_t version, st::Reader& io);

    void DecodeIfRequired();

    // FIX(sims) memory: decoded frames are palettised, 2 bytes per pixel (Index + Alpha, colour = PalColors[Index]
    // with that alpha; was RGBA, 4 bytes). The renderer looks the palette up per drawn pixel (same output).
    // Frames of read-only files (walls/floors: IffFile::ReadFromFile) do not keep their compressed bytes: they are
    // read again from the file when decoded, and DropDecoded frees the pixels on a zoom change (all three zooms'
    // frames were kept decoded, ~4 MB, plus ~3 MB of compressed bytes read at boot).
    std::vector<uint8_t> Index;
    std::vector<uint8_t> Alpha;
    const Color* PalColors = nullptr;
    int64_t SrcOffset = -1;
    uint32_t SrcSize = 0;
    bool HasColor() const { return PalColors != nullptr && !Index.empty(); }
    Color ColorAt(size_t i) const { Color c = PalColors[Index[i]]; c.A = Alpha[i]; return c; }
    /// RGBA copy of the frame (decodes it first; empty when the frame has no pixels)
    std::vector<Color> ExpandColors();
    size_t DropDecoded();
    void DetachFromSource();

private:
    /// <summary>
    /// Decodes this SPRFrame.
    /// </summary>
    /// <param name="io">IOBuffer used to read a SPRFrame.</param>
    void Decode(st::Reader& io);

public:
    int32_t Width = 0;
    int32_t Height = 0;

protected:
    void Init(int32_t width, int32_t height)
    {
        this->Width = width;
        this->Height = height;
        Index = std::vector<uint8_t>((size_t)Width * Height);
        Alpha = std::vector<uint8_t>((size_t)Width * Height);
    }

public:
    Color GetPixel(int32_t x, int32_t y)
    {
        return ColorAt((size_t)(y * Width) + x);
    }

    // STUB(3ds): Texture2D upload. Decodes the frame and keeps Data; returns nullptr.
    Utils::Texture2D* GetTexture(Utils::GraphicsDevice* device) override;
};

/// <summary>
/// This chunk type holds a number of paletted sprites that share a common color palette and lack z-buffers and
/// alpha buffers. SPR# chunks can be either big-endian or little-endian, which must be determined by comparing
/// the first two bytes to zero (since no version number uses more than two bytes).
/// </summary>
class SPR : public IffChunk
{
public:
    IFF_CHUNK_TYPE("SPR#")

    std::vector<std::unique_ptr<SPRFrame>> Frames;
    uint16_t PaletteID = 0;
private:
    std::vector<uint32_t> Offsets;
public:
    st::Endian ByteOrd = st::Endian::Little;

    /// <summary>
    /// Reads a SPR chunk from a stream.
    /// </summary>
    /// <param name="iff">An Iff instance.</param>
    /// <param name="stream">A Stream object holding a SPR chunk.</param>
    void Read(IffFile* iff, st::Reader& stream) override;
};

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
