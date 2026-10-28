/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/. 
 */
// Translated from FreeSims sims.files/formats/iff/chunks/BCON.cs
#pragma once
#include "files/formats/iff/AbstractIffChunk.h"

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

/// <summary>
/// This chunk type holds a number of constants that behavior code can refer to. 
/// Labels may be provided for them in a TRCN chunk with the same ID.
/// </summary>
class BCON : public IffChunk
{
public:
    IFF_CHUNK_TYPE("BCON")

    uint8_t Flags = 0;
    std::vector<uint16_t> Constants;

    /// <summary>
    /// Reads a BCON chunk from a stream.
    /// </summary>
    void Read(IffFile* iff, st::Reader& stream) override;
};

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
