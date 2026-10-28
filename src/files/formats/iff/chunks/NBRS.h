/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/. 
 */
// Translated from FreeSims sims.files/formats/iff/chunks/NBRS.cs
#pragma once
#include "files/formats/iff/AbstractIffChunk.h"
#include <memory>
#include <unordered_map>

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

class Neighbour
{
public:
    int32_t Unknown1 = 1; //1
    int32_t Version = 0xA; //0x4, 0xA
    //if 0xA, unknown3 follows
    //0x4 indicates person data size of 0xa0.. (160 bytes, or 80 entries)
    int32_t Unknown3 = 9; //9
    std::string Name;
    int32_t MysteryZero = 0;
    int32_t PersonMode = 0; //0/5/9
    std::vector<int16_t> PersonData; //can be null (empty)

    int16_t NeighbourID = 0;
    uint32_t GUID = 0;
    int32_t UnknownNegOne = -1; //negative 1 usually

    std::unordered_map<int32_t, std::vector<int16_t>> Relationships;

    Neighbour() { }

    explicit Neighbour(st::Reader& io);

    void Save(st::Writer& io);
};

/// <summary>
/// This chunk defines all neighbours in a neighbourhood. 
/// A neighbour is a specific version of a sim object with associated relationships and person data. (skills, person type)
/// 
/// These can be read within SimAntics without the avatar actually present. This is used to find and spawn suitable sims on 
/// ped portals as visitors, and also drive phone calls to other sims in the neighbourhood.
/// When neighbours are spawned, they assume the attributes saved here. A TS1 global call allows the game to save these attributes.
/// </summary>
class NBRS : public IffChunk
{
public:
    IFF_CHUNK_TYPE("NBRS")

    std::vector<std::unique_ptr<Neighbour>> Entries;
    std::unordered_map<int16_t, Neighbour*> NeighbourByID;
    std::unordered_map<uint32_t, int16_t> DefaultNeighbourByGUID;

    uint32_t Version = 0;
    int32_t Offset = 0;

    /// <summary>
    /// Reads a NBRS chunk from a stream.
    /// </summary>
    void Read(IffFile* iff, st::Reader& stream) override;

    /// <summary>
    /// Writes a NBRS chunk to a stream.
    /// </summary>
    bool Write(IffFile* iff, st::Writer& stream) override;
};

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
