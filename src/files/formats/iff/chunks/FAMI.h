/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/. 
 */
// Translated from FreeSims sims.files/formats/iff/chunks/FAMI.cs
#pragma once
#include "files/formats/iff/AbstractIffChunk.h"

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

/// <summary>
/// This class defines a single family in the neighbourhood, and various properties such as their 
/// budget and house assignment. These can be modified using GenericTS1Calls, but are mainly
/// defined within CAS.
/// </summary>
class FAMI : public IffChunk
{
public:
    IFF_CHUNK_TYPE("FAMI")

    uint32_t Version = 0;

    int32_t HouseNumber = 0;
    //this is not a typical family number - it is unique between user created families, but -1 for townies.
    //i believe it is an alternate family UID that basically runs on an auto increment to obtain its value.
    //(in comparison with the ChunkID as family that is used ingame, which appears to fill spaces as they are left)
    int32_t Offset = 0;
    int32_t FamilyNumber = 0;
    int32_t Budget = 0;
    int32_t NetWorth = 0;
    int32_t FamilyFriends = 0;
    int32_t Unknown = 0; //19, 17 or 1? could be flags, (1, 16, 2) ... 0 for townies
    std::vector<uint32_t> FamilyGUIDs;
    // TS1(sims): the bytes after the member list (version 8: 4, version 9: 16), written back as read
    std::vector<uint8_t> Tail;

    std::vector<uint32_t> RuntimeSubset; //the members of this family currently active. don't save!

    void SelectWholeFamily()
    {
        RuntimeSubset = FamilyGUIDs;
    }

    void SelectOneMember(uint32_t guid)
    {
        RuntimeSubset = std::vector<uint32_t>{ guid };
    }

    /// <summary>
    /// Reads a FAMI chunk from a stream.
    /// </summary>
    void Read(IffFile* iff, st::Reader& stream) override;

    bool Write(IffFile* iff, st::Writer& stream) override;
};

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
