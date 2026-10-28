/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/formats/iff/chunks/WALm.cs
#pragma once
#include "files/formats/iff/AbstractIffChunk.h"

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

class WALmEntry
{
public:
    std::string Name;
    int32_t Unknown = 0; //usually 1
    uint8_t ID = 0;
    std::vector<uint8_t> Unknown2;
    WALmEntry(st::Reader& io, int32_t id)
    {
        Name = io.ReadNullTerminatedString();
        if (Name.size() % 2 == 0) io.ReadByte(); //pad to short width
        Unknown = io.ReadInt32(); //index in iff?
        ID = io.ReadByte();
        Unknown2 = io.ReadBytes(5 + id * 2);

        //id 0 seems to be an older format
        //unknown2 is 01 00 00 00 00 00
        //id 1 adds more fields
        //unknown2 is 01 01 00 00 00 00 00 00

        //related to number of walls or floors in the file?
    }
};

/// <summary>
/// WALm and FLRm chunks, used for mapping walls and floors in ARRY chunks to walls and floors in resource files (outwith floors.iff)
/// </summary>
class WALm : public IffChunk
{
public:
    IFF_CHUNK_TYPE("WALm")

    std::vector<WALmEntry> Entries;

    void Read(IffFile* iff, st::Reader& stream) override;
};

class FLRm : public WALm
{
public:
    IFF_CHUNK_TYPE("FLRm")
    //literally no difference
};

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
