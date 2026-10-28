/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/. 
 */
// Translated from FreeSims sims.files/formats/iff/chunks/PALT.cs
#pragma once
#include "files/formats/iff/AbstractIffChunk.h"

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

/// <summary>
/// This chunk type holds a color palette.
/// </summary>
class PALT : public IffChunk
{
public:
    IFF_CHUNK_TYPE("PALT")

    PALT()
    {
    }

    explicit PALT(Color color)
    {
        Colors = std::vector<Color>(256);
        for (int i = 0; i < 256; i++)
        {
            Colors[i] = color;
        }
    }

    std::vector<Color> Colors;
    int32_t References = 0;

    /// <summary>
    /// Reads a PALT chunk from a stream.
    /// </summary>
    /// <param name="iff">An Iff instance.</param>
    /// <param name="stream">A Stream object holding a PALT chunk.</param>
    void Read(IffFile* iff, st::Reader& stream) override;

    bool Write(IffFile* iff, st::Writer& stream) override;

    bool PalMatch(const std::vector<Color>& data);
};

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
