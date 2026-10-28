/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/formats/iff/chunks/TTAT.cs
#pragma once
#include "files/formats/iff/AbstractIffChunk.h"
#include <unordered_map>

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

class TATT : public IffChunk
{
public:
    IFF_CHUNK_TYPE("TATT")

    std::unordered_map<uint32_t, std::vector<int16_t>> TypeAttributesByGUID;

    uint32_t Version = 0;
    int32_t Offset = 0;

    void Read(IffFile* iff, st::Reader& stream) override;

    bool Write(IffFile* iff, st::Writer& stream) override;
};

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
