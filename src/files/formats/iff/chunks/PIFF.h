/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/formats/iff/chunks/PIFF.cs
#pragma once
#include "files/formats/iff/AbstractIffChunk.h"

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

enum class PIFFPatchMode : uint8_t
{
    Remove = 0,
    Add = 1
};

class PIFFPatch
{
public:
    uint32_t Offset = 0;
    uint32_t Size = 0;
    PIFFPatchMode Mode = PIFFPatchMode::Remove;
    std::vector<uint8_t> Data;
};

class PIFFEntry
{
public:
    std::string Type;
    uint16_t ChunkID = 0;
    uint16_t NewChunkID = 0;
    bool Delete = false;

    std::string ChunkLabel;
    uint16_t ChunkFlags = 0;
    uint32_t NewDataSize = 0;

    std::vector<PIFFPatch> Patches;

    std::vector<uint8_t> Apply(const std::vector<uint8_t>& src) const;
};

class PIFF : public IffChunk
{
public:
    IFF_CHUNK_TYPE("PIFF")

    uint16_t Version = 1;
    std::string SourceIff;
    std::vector<PIFFEntry> Entries;

    PIFF()
    {
        ChunkType = "PIFF";
    }

    void Read(IffFile* iff, st::Reader& stream) override;

    bool Write(IffFile* iff, st::Writer& stream) override;
};

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
