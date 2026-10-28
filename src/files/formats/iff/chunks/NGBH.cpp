/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/. 
 */
// Translated from FreeSims sims.files/formats/iff/chunks/NGBH.cs
#include "files/formats/iff/chunks/NGBH.h"
#include "files/formats/iff/IffFile.h"

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

void NGBH::Read(IffFile* iff, st::Reader& stream)
{
    (void)iff;
    st::Reader& io = stream;
    io.ByteOrder(st::Endian::Little);
    {
        io.ReadUInt32(); //pad
        Version = io.ReadUInt32(); //0x49 for latest game
        std::string magic = io.ReadCString(4); //HBGN
        (void)magic;

        for (int i=0; i<16; i++)
        {
            NeighborhoodData[i] = io.ReadInt16();
        }

        // TS1(sims): pre-Hot Date neighbourhoods (version 0x3E, e.g. the base game UserData/Neighborhood.iff)
        // end after the 16 neighbourhood shorts: the inventory section was added with Hot Date. The C# read
        // past the end here (EndOfStreamException), which made the whole NGBH chunk unreadable.
        if (!io.HasMore()) return;
        auto count = io.ReadInt32();
        for (int i = 0; i < count; i++)
        {
            if (io.ReadInt32() != 1) { }
            auto neighID = io.ReadInt16();
            auto inventoryCount = io.ReadInt32();
            std::vector<InventoryItem> inventory;

            for (int j=0; j<inventoryCount; j++)
            {
                inventory.push_back(InventoryItem(io));
            }
            InventoryByID[neighID] = inventory;
        }
    }
}

bool NGBH::Write(IffFile* iff, st::Writer& stream)
{
    (void)iff;
    st::Writer& io = stream;
    io.ByteOrder(st::Endian::Little);
    {
        io.WriteInt32(0);
        io.WriteInt32(0x49);
        io.WriteCString("HBGN", 4);
        
        for (int i=0; i<16; i++)
        {
            io.WriteInt16(NeighborhoodData[i]);
        }

        io.WriteInt32(16); //NeighborhoodData.Length (sic)
        for (auto& item : InventoryByID)
        {
            io.WriteInt32(1);
            io.WriteInt16(item.first);
            io.WriteInt32((int32_t)item.second.size());
            for (auto& invent : item.second)
            {
                invent.SerializeInto(io);
            }
        }
    }
    return true;
}

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
