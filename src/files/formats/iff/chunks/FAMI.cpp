/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/. 
 */
// Translated from FreeSims sims.files/formats/iff/chunks/FAMI.cs
#include "files/formats/iff/chunks/FAMI.h"
#include "files/formats/iff/IffFile.h"

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

void FAMI::Read(IffFile* iff, st::Reader& stream)
{
    (void)iff;
    st::Reader& io = stream;
    io.ByteOrder(st::Endian::Little);
    {
        io.ReadUInt32(); //pad
        Version = io.ReadUInt32(); //0x9 for latest game
        std::string magic = io.ReadCString(4); //IMAF
        (void)magic;

        HouseNumber = io.ReadInt32();
        FamilyNumber = io.ReadInt32();
        Budget = io.ReadInt32();
        NetWorth = io.ReadInt32();
        FamilyFriends = io.ReadInt32();
        Unknown = io.ReadInt32();
        auto n = io.ReadInt32();
        FamilyGUIDs = std::vector<uint32_t>(n < 0 ? 0 : n);
        for (size_t i=0; i<FamilyGUIDs.size(); i++)
        {
            FamilyGUIDs[i] = io.ReadUInt32();
        }
        Tail.clear();
        while (io.Position() < io.Length()) Tail.push_back(io.ReadByte()); // TS1(sims)
    }
}

bool FAMI::Write(IffFile* iff, st::Writer& stream)
{
    (void)iff;
    st::Writer& io = stream;
    io.ByteOrder(st::Endian::Little);
    {
        io.WriteInt32(0);
        // FIX(sims): FreeSims always wrote version 9 + 16 bytes; the shipped Neighborhood.iff families are
        // version 7 (no tail) and the .FAM templates 8 (4 bytes): write the version that was read (7 for new ones)
        uint32_t version = Version ? Version : 7;
        io.WriteUInt32(version);
        io.WriteCString("IMAF", 4);
        io.WriteInt32(HouseNumber);
        io.WriteInt32(FamilyNumber);
        io.WriteInt32(Budget);
        io.WriteInt32(NetWorth);
        io.WriteInt32(FamilyFriends);
        io.WriteInt32(Unknown);
        io.WriteInt32((int32_t)FamilyGUIDs.size());
        for (auto guid : FamilyGUIDs)
            io.WriteUInt32(guid);

        size_t tailSize = (version >= 9) ? 16 : (version == 8 ? 4 : 0);
        for (size_t i = 0; i < tailSize; i++) io.WriteByte(i < Tail.size() ? Tail[i] : (uint8_t)0);
    }
    return true;
}

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
