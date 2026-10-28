/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/formats/iff/chunks/CARR.cs
#pragma once
#include "files/formats/iff/AbstractIffChunk.h"
#include "files/formats/iff/IffFieldEncode.h"

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

class JobLevel
{
public:
    int32_t MinRequired[10] = {0}; //friends, then skills.
    int32_t MotiveDelta[7] = {0};
    int32_t Salary = 0;
    int32_t StartTime = 0;
    int32_t EndTime = 0;
    int32_t CarType = 0;

    std::string JobName;
    std::string MaleUniformMesh;
    std::string FemaleUniformMesh;
    std::string UniformSkin;
    std::string unknown;

    explicit JobLevel(IffFieldEncode& iop);
};

class CARR : public IffChunk
{
public:
    IFF_CHUNK_TYPE("CARR")

    std::string Name;
    std::vector<JobLevel> JobLevels;

    void Read(IffFile* iff, st::Reader& stream) override;

    int32_t GetJobData(int32_t level, int32_t data);
};

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
