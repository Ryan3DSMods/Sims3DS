/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/formats/iff/chunks/TTAT.cs
#include "files/formats/iff/chunks/TTAT.h"
#include "files/formats/iff/chunks/TTAB.h"
#include "files/formats/iff/IffFieldEncode.h"
#include "files/formats/iff/IffFile.h"
#include <memory>

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

void TATT::Read(IffFile* iff, st::Reader& stream)
{
    (void)iff;
    st::Reader& io = stream;
    io.ByteOrder(st::Endian::Little);
    {
        io.ReadUInt32(); //pad
        auto version = io.ReadUInt32(); //zero
        (void)version;

        auto TTAT = io.ReadCString(4);
        (void)TTAT;

        std::unique_ptr<IOProxy> iop;
        auto compressionCode = io.ReadByte();
        //HACK: for freeso we don't run the field encoding coompression
        //since fso neighbourhoods are not compatible with ts1, it does not matter too much
        if (compressionCode != 1) iop.reset(new TTABNormal(&io));
        else iop.reset(new IffFieldEncode(&io));

        auto total = iop->ReadInt32();
        for (int i=0; i<total; i++)
        {
            auto guid = (uint32_t)iop->ReadInt32();
            auto count = iop->ReadInt32();
            std::vector<int16_t> tatts(count < 0 ? 0 : count);
            for (int j=0; j<count; j++)
            {
                tatts[j] = iop->ReadInt16();
            }
            TypeAttributesByGUID[guid] = tatts;
        }
    }
}

bool TATT::Write(IffFile* iff, st::Writer& stream)
{
    (void)iff;
    st::Writer& io = stream;
    io.ByteOrder(st::Endian::Little);
    {
        io.WriteInt32(0);
        io.WriteInt32(0); //version

        io.WriteCString("TTAT");

        io.WriteByte(0); //compression code
        io.WriteInt32((int32_t)TypeAttributesByGUID.size());
        for (auto& tatt : TypeAttributesByGUID)
        {
            io.WriteUInt32(tatt.first);
            io.WriteInt32((int32_t)tatt.second.size());
            for (auto value : tatt.second)
                io.WriteInt16(value);
        }

    }
    return true;
}

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
