/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/. 
 */
// Translated from FreeSims sims.files/formats/iff/chunks/BMP.cs
#pragma once
#include "files/formats/iff/AbstractIffChunk.h"
#include "files/utils/ITextureProvider.h"

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

/// <summary>
/// This chunk type holds an image in BMP format.
/// </summary>
class BMP : public IffChunk
{
public:
    IFF_CHUNK_TYPE("BMP_")

private:
    std::vector<uint8_t> data;

public:
    /// <summary>
    /// Reads a BMP chunk from a stream.
    /// </summary>
    void Read(IffFile* iff, st::Reader& stream) override;

    /// Raw BMP file bytes (added accessor: C# decodes via System.Drawing/Texture2D).
    const std::vector<uint8_t>& GetData() const { return data; }

    // STUB(3ds): System.Drawing.Bitmap.FromStream - returns nothing on 3DS (use GetData()).
    void* GetBitmap() { return nullptr; }

    // STUB(3ds): Texture2D.FromStream - returns nullptr on 3DS (use GetData()).
    Utils::Texture2D* GetTexture(Utils::GraphicsDevice* device) { (void)device; return nullptr; }
};

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
