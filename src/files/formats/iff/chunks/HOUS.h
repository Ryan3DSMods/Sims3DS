/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/formats/iff/chunks/HOUS.cs
#pragma once
#include "files/formats/iff/AbstractIffChunk.h"

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

class HOUS : public IffChunk
{
public:
    IFF_CHUNK_TYPE("HOUS")

    int32_t Version = 0;
    int32_t UnknownFlag = 0;
    int32_t UnknownOne = 0;
    int32_t UnknownNumber = 0;
    int32_t UnknownNegative = 0;
    int16_t CameraDir = 0;
    int16_t UnknownOne2 = 0;
    int16_t UnknownFlag2 = 0;
    uint32_t GUID = 0;
    std::string RoofName;

    void Read(IffFile* iff, st::Reader& stream) override;
};

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
