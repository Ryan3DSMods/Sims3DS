/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/. 
 */
// Translated from FreeSims sims.files/formats/iff/chunks/FWAV.cs
#pragma once
#include "files/formats/iff/AbstractIffChunk.h"

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

/// <summary>
/// This chunk type holds the name of a sound event that this object uses.
/// </summary>
class FWAV : public IffChunk
{
public:
    IFF_CHUNK_TYPE("FWAV")

    std::string Name;

    /// <summary>
    /// Reads a FWAV chunk from a stream.
    /// </summary>
    void Read(IffFile* iff, st::Reader& stream) override;

    bool Write(IffFile* iff, st::Writer& stream) override;
};

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
