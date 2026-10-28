/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.Content.TS1.TS1JobProvider
// TS1(sims): FreeSims declares `TS1JobProvider Jobs;` in Content.cs but the class is not in the tree and
// its construction is commented out. This is a minimal provider for the SimAntics "job data [temp 0,1]" scope
// (33) and "Set to Next: Career": it reads the CARR (career) chunks of Work.iff (GameData/ and expansion
// pack folders). If no Work.iff exists (the test install has none) it is empty: careers iterate to nothing
// and job data reads 0.
#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "files/formats/iff/IffFile.h"
#include "files/formats/iff/chunks/CARR.h"
#include "files/formats/iff/chunks/STR.h"

namespace FSO { namespace Content {
class Content;
namespace TS1 {

class TS1JobProvider
{
public:
    explicit TS1JobProvider(Content* contentManager);
    void Init();

    std::vector<std::unique_ptr<FSO::Files::Formats::IFF::IffFile>> JobResources;
    std::map<int16_t, FSO::Files::Formats::IFF::Chunks::CARR*> Jobs; // by chunk id (job type)

    FSO::Files::Formats::IFF::Chunks::CARR* GetJob(int16_t jobType);
    /// job data(temp0 = job type, temp1 = level, data = field), see Behavior.iff STR# 243 'job data'.
    int32_t GetJobData(int16_t jobType, int16_t level, int16_t data);
    /// next job type after current, -1 at the end.
    int16_t SetToNext(int16_t current);
    /// FIX(sims): FreeSO TS1JobProvider.JobStrings / JobOffer for the dialog tokens $Job: $JobDesc: $JobOffer:
    /// (work.iff STR# with the career's id, read on first use).
    FSO::Files::Formats::IFF::Chunks::STR* JobStrings(int16_t jobType);
    std::string JobOffer(int16_t jobType, int32_t jobLevel);

private:
    std::string _WorkPath; uint32_t _WorkOffset = 0, _WorkLength = 0; bool _WorkInArchive = false;
    std::unique_ptr<FSO::Files::Formats::IFF::IffFile> _WorkStrings;
    bool _WorkStringsTried = false;
    Content* ContentManager;
};

}}}
