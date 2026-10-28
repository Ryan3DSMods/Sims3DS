/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/formats/iff/chunks/PIFF.cs
#include "files/formats/iff/chunks/PIFF.h"
#include "files/formats/iff/IffFile.h"
#include <cstring>
#include <stdexcept>

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

void PIFF::Read(IffFile* iff, st::Reader& stream)
{
    (void)iff;
    st::Reader& io = stream;
    io.ByteOrder(st::Endian::Little);
    {
        Version = io.ReadUInt16();
        SourceIff = io.ReadVariableLengthPascalString();
        Entries = std::vector<PIFFEntry>(io.ReadUInt16());
        for (size_t i=0; i<Entries.size(); i++)
        {
            PIFFEntry e;
            e.Type = io.ReadCString(4);
            e.ChunkID = io.ReadUInt16();
            e.Delete = io.ReadByte()>0;
            
            if (!e.Delete)
            {
                e.ChunkLabel = io.ReadVariableLengthPascalString();
                e.ChunkFlags = io.ReadUInt16();
                if (Version > 0) e.NewChunkID = io.ReadUInt16();
                else e.NewChunkID = e.ChunkID;
                e.NewDataSize = io.ReadUInt32();

                auto size = io.ReadUInt32();
                e.Patches = std::vector<PIFFPatch>(size);
                uint32_t lastOff = 0;
                
                for (size_t j=0; j<e.Patches.size(); j++)
                {
                    PIFFPatch p;

                    p.Offset = lastOff + io.ReadVarLen();
                    lastOff = p.Offset;
                    p.Size = io.ReadVarLen();
                    p.Mode = (PIFFPatchMode)io.ReadByte();

                    if (p.Mode == PIFFPatchMode::Add) p.Data = io.ReadBytes(p.Size);
                    e.Patches[j] = p;
                }
            }
            Entries[i] = e;
        }
    }
}

bool PIFF::Write(IffFile* iff, st::Writer& stream)
{
    (void)iff;
    st::Writer& io = stream;
    io.ByteOrder(st::Endian::Little);
    {
        io.WriteUInt16(1);
        io.WriteVariableLengthPascalString(SourceIff);
        io.WriteUInt16((uint16_t)Entries.size());
        for (auto& ent : Entries)
        {
            io.WriteCString(ent.Type, 4);
            io.WriteUInt16(ent.ChunkID);
            io.WriteByte((uint8_t)(ent.Delete ? 1 : 0));

            if (!ent.Delete)
            {
                io.WriteVariableLengthPascalString(ent.ChunkLabel); //0 length means no replacement
                io.WriteUInt16(ent.ChunkFlags);
                io.WriteUInt16(ent.NewChunkID);
                io.WriteUInt32(ent.NewDataSize);
                io.WriteUInt32((uint32_t)ent.Patches.size());

                uint32_t lastOff = 0;
                for (auto& p : ent.Patches)
                {
                    io.WriteVarLen(p.Offset-lastOff);
                    lastOff = p.Offset;
                    io.WriteVarLen(p.Size);
                    io.WriteByte((uint8_t)p.Mode);
                    if (p.Mode == PIFFPatchMode::Add) io.WriteBytes(p.Data);
                }
            }
        }
    }
    return true;
}

std::vector<uint8_t> PIFFEntry::Apply(const std::vector<uint8_t>& src) const
{
    std::vector<uint8_t> result(NewDataSize);
    uint32_t srcPtr = 0;
    uint32_t destPtr = 0;
    int i = 0;
    auto copy = [&](const std::vector<uint8_t>& from, uint32_t fromPos, uint32_t toPos, uint32_t n) {
        //Array.Copy bounds checks
        if ((size_t)fromPos + n > from.size() || (size_t)toPos + n > result.size()) throw std::out_of_range("PIFFEntry.Apply: copy out of range");
        if (n) std::memcpy(result.data() + toPos, from.data() + fromPos, n);
    };
    for (auto& p : Patches)
    {
        auto copyCount = p.Offset - destPtr;
        copy(src, srcPtr, destPtr, copyCount);
        srcPtr += copyCount; destPtr += copyCount;
        if (p.Mode == PIFFPatchMode::Add)
        {
            copy(p.Data, 0, destPtr, p.Size);
            destPtr += p.Size;
        } else
        {
            srcPtr += p.Size;
        }
        i++;
    }
    auto remainder = NewDataSize - destPtr;
    if (remainder != 0) copy(src, srcPtr, destPtr, remainder);

    return result;
}

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
