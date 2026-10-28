/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/. 
 */
// Translated from FreeSims sims.files/formats/iff/chunks/SLOT.cs
#pragma once
#include "files/formats/iff/AbstractIffChunk.h"
#include <unordered_map>

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

enum class SLOTFlags : int32_t
{
    NORTH = 1,
    NORTH_EAST = 2,
    EAST = 4,
    SOUTH_EAST = 8,
    SOUTH = 16,
    SOUTH_WEST = 32,
    WEST = 64,
    NORTH_WEST = 128,
    AllowAnyRotation = 256,
    Absolute = 512,
    FacingAwayFromObject = 1024,
    IgnoreRooms = 2048,
    SnapToDirection = 4096,
    RandomScoring=8192,
    AllowFailureTrees = 16385,
    AllowDifferentAlts=32768,
    UseAverageObjectLocation = 65536
};
FSO_FLAGS_ENUM(SLOTFlags)

enum class SLOTFacing : int32_t
{
    FaceAnywhere = -3,
    FaceTowardsObject = -2,
    FaceAwayFromObject = -1,
};

class SLOTItem
{
public:
    uint16_t Type = 0;
    Vector3 Offset;
    int32_t Standing = 1;
    int32_t Sitting = 0;
    int32_t Ground = 0;
    SLOTFlags Rsflags = (SLOTFlags)0;
    int32_t SnapTargetSlot = -1;
    int32_t MinProximity = 0;
    int32_t MaxProximity = 0;
    int32_t OptimalProximity = 0;
    float Gradient = 0;
    SLOTFacing Facing = SLOTFacing::FaceTowardsObject;
    int32_t Resolution = 16;
    int32_t Height = 0;
};

/// <summary>
/// This format isn't documented on the wiki! Thanks, Darren!
/// </summary>
class SLOT : public IffChunk
{
public:
    IFF_CHUNK_TYPE("SLOT")

    static const float HeightOffsets[9];

    std::unordered_map<uint16_t, std::vector<SLOTItem>> Slots;

    void Read(IffFile* iff, st::Reader& stream) override;
};

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
