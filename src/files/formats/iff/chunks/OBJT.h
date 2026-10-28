/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/formats/iff/chunks/OBJT.cs
#pragma once
#include "files/formats/iff/AbstractIffChunk.h"
#include "files/formats/iff/chunks/OBJD.h"

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

class OBJTEntry
{
public:
    uint32_t GUID = 0;
    uint16_t Unknown1a = 0;
    uint16_t InitTreeVersion = 0;
    uint16_t Unknown2a = 0;
    uint16_t MainTreeVersion = 0;
    uint16_t TypeID = 0;
    Chunks::OBJDType OBJDType = Chunks::OBJDType::Unknown;
    std::string Name;
    OBJTEntry(st::Reader& io, int32_t version);
};

class OBJT : public IffChunk
{
public:
    IFF_CHUNK_TYPE("objt")

    //another sims 1 masterpiece. A list of object info.
    std::vector<OBJTEntry> Entries;

    void Read(IffFile* iff, st::Reader& stream) override;
};

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
