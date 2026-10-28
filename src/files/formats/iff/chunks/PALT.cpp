/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/. 
 */
// Translated from FreeSims sims.files/formats/iff/chunks/PALT.cs
#include "files/formats/iff/chunks/PALT.h"
#include "files/formats/iff/IffFile.h"

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

void PALT::Read(IffFile* iff, st::Reader& stream)
{
    (void)iff;
    st::Reader& io = stream;
    io.ByteOrder(st::Endian::Little);
    {
        auto version = io.ReadUInt32();
        auto numEntries = io.ReadUInt32();
        auto reserved = io.ReadBytes(8);
        (void)version; (void)reserved;

        Colors = std::vector<Color>(numEntries);
        for (uint32_t i = 0; i < numEntries; i++)
        {
            auto r = io.ReadByte();
            auto g = io.ReadByte();
            auto b = io.ReadByte();
            Colors[i] = Color(r, g, b);
        }
    }
}

bool PALT::Write(IffFile* iff, st::Writer& stream)
{
    (void)iff;
    st::Writer& io = stream;
    io.ByteOrder(st::Endian::Little);
    {
        io.WriteUInt32(0);
        io.WriteUInt32((uint32_t)Colors.size());
        io.WriteBytes(std::vector<uint8_t>{ 0, 0, 0, 0, 0, 0, 0, 0 });
        for (auto& col : Colors)
        {
            io.WriteByte(col.R);
            io.WriteByte(col.G);
            io.WriteByte(col.B);
        }
        return true;
    }
}

bool PALT::PalMatch(const std::vector<Color>& data)
{
    for (size_t i=0; i<Colors.size(); i++)
    {
        if (i >= data.size()) return true;
        if (data[i].A != 0 && data[i] != Colors[i]) return false;
    }
    return true;
}

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
