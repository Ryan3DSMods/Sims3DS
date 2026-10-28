/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/. 
 */
// Translated from FreeSims sims.files/formats/iff/chunks/OBJf.cs
#include "files/formats/iff/chunks/OBJf.h"
#include "files/formats/iff/IffFile.h"

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

void OBJf::Read(IffFile* iff, st::Reader& stream)
{
    (void)iff;
    st::Reader& io = stream;
    io.ByteOrder(st::Endian::Little);
    {
        io.ReadUInt32(); //pad
        Version = io.ReadUInt32();
        std::string magic = io.ReadCString(4);
        (void)magic;
        functions = std::vector<OBJfFunctionEntry>(io.ReadUInt32());
        for (size_t i=0; i<functions.size(); i++) {
            OBJfFunctionEntry result;
            result.ConditionFunction = io.ReadUInt16();
            result.ActionFunction = io.ReadUInt16();
            functions[i] = result;
        }
    }
}

bool OBJf::Write(IffFile* iff, st::Writer& stream)
{
    (void)iff;
    st::Writer& io = stream;
    io.ByteOrder(st::Endian::Little);
    {
        io.WriteUInt32(0);
        io.WriteUInt32(Version);
        io.WriteCString("fJBO", 4);
        io.WriteInt32((int32_t)functions.size());
        for (auto& func : functions)
        {
            io.WriteUInt16(func.ConditionFunction);
            io.WriteUInt16(func.ActionFunction);
        }
        return true;
    }
}

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
