/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/. 
 */
// Translated from FreeSims sims.files/formats/iff/chunks/NBRS.cs
#include "files/formats/iff/chunks/NBRS.h"
#include "files/formats/iff/IffFile.h"
#include <algorithm>
#include <stdexcept>

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

void NBRS::Read(IffFile* iff, st::Reader& stream)
{
    (void)iff;
    st::Reader& io = stream;
    io.ByteOrder(st::Endian::Little);
    {
        io.ReadUInt32(); //pad
        Version = io.ReadUInt32(); //0x49 for latest game
        std::string magic = io.ReadCString(4); //SRBN
        (void)magic;
        auto count = io.ReadUInt32();

        for (uint32_t i=0; i<count; i++)
        {
            if (!io.HasMore()) return; //C#: return (the OrderBy below is skipped)
            {
                auto neigh = new Neighbour(io);
                Entries.emplace_back(neigh);
                if (neigh->Unknown1 > 0)
                {
                    //C#: NeighbourByID.Add (throws on duplicate)
                    if (NeighbourByID.find(neigh->NeighbourID) != NeighbourByID.end())
                        throw std::runtime_error("An item with the same key has already been added.");
                    NeighbourByID[neigh->NeighbourID] = neigh;
                    DefaultNeighbourByGUID[neigh->GUID] = neigh->NeighbourID;
                }
            }
        }
    }
    std::stable_sort(Entries.begin(), Entries.end(),
        [](const std::unique_ptr<Neighbour>& a, const std::unique_ptr<Neighbour>& b) { return a->NeighbourID < b->NeighbourID; });
}

bool NBRS::Write(IffFile* iff, st::Writer& stream)
{
    (void)iff;
    st::Writer& io = stream;
    io.ByteOrder(st::Endian::Little);
    {
        io.WriteUInt32(0);
        // FIX(sims): keep the version that was read (the shipped base-game files are 0x3E); C# always wrote 0x49.
        io.WriteUInt32(Version != 0 ? Version : 0x49);
        io.WriteCString("SRBN", 4);
        // FIX(sims): C# wrote Entries.Count but then only the valid neighbours, in dictionary order, which
        // corrupts every file with free slots. TS1 stores neighbour id N in slot N-1 and a free slot as a lone
        // int32 0 (Unknown1 == 0); write the slots in that order so a read/write round trip is lossless.
        int32_t slots = (int32_t)Entries.size();
        for (auto& kv : NeighbourByID) if (kv.first > slots) slots = kv.first;
        io.WriteInt32(slots);
        for (int32_t slot = 0; slot < slots; slot++)
        {
            auto it = NeighbourByID.find((int16_t)(slot + 1));
            if (it != NeighbourByID.end() && it->second->Unknown1 == 1) it->second->Save(io);
            else io.WriteInt32(0);
        }
    }
    return true;
}

Neighbour::Neighbour(st::Reader& io)
{
    Unknown1 = io.ReadInt32();
    if (Unknown1 != 1) { return; }
    Version = io.ReadInt32();
    if (Version == 0xA)
    {
        //TODO: what version does this truly start?
        Unknown3 = io.ReadInt32();
        if (Unknown3 != 9) { }
    }
    Name = io.ReadNullTerminatedString();
    if (Name.size() % 2 == 0) io.ReadByte();
    MysteryZero = io.ReadInt32();
    if (MysteryZero != 0) { }
    PersonMode = io.ReadInt32();
    if (PersonMode > 0)
    {
        int size = (Version == 0x4) ? 0xa0 : 0x200;
        PersonData = std::vector<int16_t>(88);
        int pdi = 0;
        for (int i=0; i<size; i+=2)
        {
            if (pdi >= 88)
            {
                io.ReadBytes(size - i);
                break;
            }
            PersonData[pdi++] = io.ReadInt16();
        }
    }

    NeighbourID = io.ReadInt16();
    GUID = io.ReadUInt32();
    UnknownNegOne = io.ReadInt32();
    if (UnknownNegOne != -1) { }

    auto entries = io.ReadInt32();
    Relationships.clear();
    for (int i=0; i<entries; i++)
    {
        auto keyCount = io.ReadInt32();
        if (keyCount != 1) { }
        auto key = io.ReadInt32();
        std::vector<int16_t> values;
        auto valueCount = io.ReadInt32();
        for (int j=0; j<valueCount; j++)
        {
            values.push_back((int16_t)io.ReadInt32());
        }
        //C#: Relationships.Add (throws on duplicate)
        if (Relationships.find(key) != Relationships.end())
            throw std::runtime_error("An item with the same key has already been added.");
        Relationships[key] = values;
    }
}

void Neighbour::Save(st::Writer& io)
{
    io.WriteInt32(Unknown1);
    io.WriteInt32(Version);
    if (Version == 0xA) io.WriteInt32(Unknown3);
    io.WriteNullTerminatedString(Name);
    if (Name.size() % 2 == 0) io.WriteByte(0);
    io.WriteInt32(MysteryZero);
    io.WriteInt32(PersonMode);
    if (PersonMode > 0)
    {
        int size = (Version == 0x4) ? 0xa0 : 0x200;
        int pdi = 0;
        for (int i = 0; i < size; i += 2)
        {
            if (pdi >= 88)
            {
                io.WriteInt16(0);
            }
            else
            {
                io.WriteInt16(PersonData[pdi++]);
            }
        }
    }

    io.WriteInt16(NeighbourID);
    io.WriteUInt32(GUID);
    io.WriteInt32(UnknownNegOne);

    io.WriteInt32((int32_t)Relationships.size());
    for (auto& rel : Relationships)
    {
        io.WriteInt32(1); //keycount (1)
        io.WriteInt32(rel.first);
        io.WriteInt32((int32_t)rel.second.size());
        for (auto val : rel.second)
        {
            io.WriteInt32(val);
        }
    }
}

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
