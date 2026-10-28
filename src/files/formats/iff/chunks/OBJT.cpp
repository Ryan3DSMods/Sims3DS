/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/formats/iff/chunks/OBJT.cs
#include "files/formats/iff/chunks/OBJT.h"
#include "files/formats/iff/IffFile.h"

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

void OBJT::Read(IffFile* iff, st::Reader& stream)
{
    (void)iff;
    st::Reader& io = stream;
    io.ByteOrder(st::Endian::Little);
    {
        auto zero = io.ReadInt32();
        (void)zero;
        auto version = io.ReadInt32(); //should be 2/3
        auto objt = io.ReadInt32(); //tjbo
        (void)objt;

        Entries.clear();
        //single tile objects are named. multitile objects arent.

        while (io.HasMore())
        {
            Entries.push_back(OBJTEntry(io, version));
        }
    }
}

OBJTEntry::OBJTEntry(st::Reader& io, int32_t version)
{
    //16 bytes of data
    GUID = io.ReadUInt32();
    if (GUID == 0) return;
    Unknown1a = io.ReadUInt16(); //likely number of attributes
    InitTreeVersion = io.ReadUInt16();
    Unknown2a = io.ReadUInt16(); //objd version?
    MainTreeVersion = io.ReadUInt16();
    //increases by one each time, one based, essentially an ID for this loaded type. Mostly matches index in array, but I guess it can possibly be different.
    TypeID = io.ReadUInt16();
    OBJDType = (Chunks::OBJDType)io.ReadUInt16();
    //then the name, null terminated
    Name = io.ReadNullTerminatedString();
    if (Name.size() % 2 == 0) io.ReadByte(); //pad to short width
    if (version > 2) io.ReadInt32(); //not sure what this is
}

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
