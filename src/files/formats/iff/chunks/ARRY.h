/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/. 
 */
// Translated from FreeSims sims.files/formats/iff/chunks/ARRY.cs
#pragma once
#include "files/formats/iff/AbstractIffChunk.h"

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

enum class ARRYType : int32_t
{
    RLEAlt = 4, //altitude
    RLEFloor = 1, //floors, "ground", grass, flags, pool "ARRY(9)", water
    RLEWalls = 8, //walls
    Objects = 2, //objects "ARRY(3)"
};

class ARRY : public IffChunk
{
public:
    IFF_CHUNK_TYPE("Arry")

    int32_t Width = 0;
    int32_t Height = 0;
    ARRYType Type = (ARRYType)0;
    std::vector<uint8_t> Data;
    std::vector<uint8_t> TransposeData() const;

    void Read(IffFile* iff, st::Reader& stream) override;

    int32_t ByteSize() const
    {
        return (int32_t)Type;
    }

    std::string DebugPrint() const;
};

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
