/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/. 
 */
// Translated from FreeSims sims.files/formats/iff/chunks/OBJf.cs
#pragma once
#include "files/formats/iff/AbstractIffChunk.h"

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

struct OBJfFunctionEntry {
    uint16_t ConditionFunction = 0;
    uint16_t ActionFunction = 0;
};

/// <summary>
/// This chunk type assigns BHAV subroutines to a number of events that occur in 
/// (or outside of?) the object, which are described in behavior.iff chunk 00F5.
/// </summary>
class OBJf : public IffChunk
{
public:
    IFF_CHUNK_TYPE("OBJf")

    std::vector<OBJfFunctionEntry> functions;
    uint32_t Version = 0;

    /// <summary>
    /// Reads a OBJf chunk from a stream.
    /// </summary>
    void Read(IffFile* iff, st::Reader& stream) override;

    bool Write(IffFile* iff, st::Writer& stream) override;
};

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
