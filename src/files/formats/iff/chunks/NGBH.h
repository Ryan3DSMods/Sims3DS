/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/. 
 */
// Translated from FreeSims sims.files/formats/iff/chunks/NGBH.cs
#pragma once
#include "files/formats/iff/AbstractIffChunk.h"
#include <unordered_map>

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

class InventoryItem
{
public:
    int32_t Type = 0;
    uint32_t GUID = 0;
    uint16_t Count = 0;

    InventoryItem() { }

    explicit InventoryItem(st::Reader& io)
    {
        Type = io.ReadInt32();
        GUID = io.ReadUInt32();
        Count = io.ReadUInt16();
    }

    void SerializeInto(st::Writer& io)
    {
        io.WriteInt32(Type);
        io.WriteUInt32(GUID);
        io.WriteUInt16(Count);
    }

    InventoryItem Clone() const
    {
        InventoryItem r; r.Type = Type; r.GUID = GUID; r.Count = Count; return r;
    }
};

/// <summary>
/// This chunk type contains general neighbourhood data within a neighbourhood.iff file.
/// The only thing this was used for initially was tracking the tutorial.
/// 
/// As of hot date, it also includes inventory data, which was added as something of an afterthought.
/// </summary>
class NGBH : public IffChunk
{
public:
    IFF_CHUNK_TYPE("NGBH")

    int16_t NeighborhoodData[16] = {0};
    std::unordered_map<int16_t, std::vector<InventoryItem>> InventoryByID;

    uint32_t Version = 0;
    int32_t Offset = 0;

    /// <summary>
    /// Reads a NGBH chunk from a stream.
    /// </summary>
    void Read(IffFile* iff, st::Reader& stream) override;

    bool Write(IffFile* iff, st::Writer& stream) override;
};

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
