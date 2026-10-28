/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// TS1(sims): see TS1JobProvider.h
#include "content/TS1JobProvider.h"
#include "content/ContentManager.h"

namespace FSO { namespace Content { namespace TS1 {

using namespace FSO::Files::Formats::IFF;
using namespace FSO::Files::Formats::IFF::Chunks;

TS1JobProvider::TS1JobProvider(Content* contentManager) : ContentManager(contentManager) {}

void TS1JobProvider::Init()
{
    JobResources.clear();
    Jobs.clear();
    const std::string& base = ContentManager->SimsCompleteDir;
    std::vector<std::string> paths = { base + "/GameData/Work.iff", base + "/GameData/work.iff" };
    for (int i = 1; i <= 7; i++) {
        std::string ep = base + "/ExpansionPack" + ((i == 1) ? std::string("") : std::to_string(i));
        paths.push_back(ep + "/Work.iff");
    }
    auto addIff = [&](std::unique_ptr<IffFile> iff) {
        try {
            for (auto carr : iff->List<CARR>()) {
                if (Jobs.find((int16_t)carr->ChunkID) == Jobs.end()) Jobs[(int16_t)carr->ChunkID] = carr;
            }
        } catch (...) { return; }
        JobResources.push_back(std::move(iff));
    };
    // TS1(sims): FreeSO `JobResource = (IffFile)provider.Get("work.iff")` — the TS1Provider looks the name up in
    // every .far of the install. The Complete Collection ships it only in ExpansionShared/ExpansionShared.far (all
    // careers incl. the expansion ones: Livin' Large's paranormal/journalism/..., Superstar, Makin' Magic...). The
    // newest archive is read first so its CARR chunks win over an older loose GameData/Work.iff.
    if (ContentManager->WorldObjects) {
        auto& archives = ContentManager->WorldObjects->Archives;
        for (size_t i = archives.size(); i-- > 0;) {
            auto it = archives[i].EntriesByName.find("work.iff");
            if (it == archives[i].EntriesByName.end()) continue;
            std::unique_ptr<IffFile> iff(new IffFile());
            // TS1(sims): only the CARR chunks are used (work.iff is 4 MB, mostly pictures and strings)
            try { iff->ReadFromFile(archives[i].Path, it->second.DataOffset, (uint32_t)it->second.DataLength, true, "CARR"); } catch (...) { continue; }
            iff->Filename = "work.iff";
            if (_WorkPath.empty()) { _WorkPath = archives[i].Path; _WorkOffset = it->second.DataOffset; _WorkLength = (uint32_t)it->second.DataLength; _WorkInArchive = true; }
            addIff(std::move(iff));
        }
    }
    for (auto& p : paths) {
        if (!FileExists(p)) continue;
        std::unique_ptr<IffFile> iff;
        try { iff.reset(new IffFile(p)); } catch (...) { continue; }
        if (_WorkPath.empty()) _WorkPath = p;
        addIff(std::move(iff));
    }
}

CARR* TS1JobProvider::GetJob(int16_t jobType)
{
    auto it = Jobs.find(jobType);
    return (it == Jobs.end()) ? nullptr : it->second;
}

int32_t TS1JobProvider::GetJobData(int16_t jobType, int16_t level, int16_t data)
{
    auto job = GetJob(jobType);
    if (job == nullptr) return 0;
    if (data == 0) return (int32_t)job->JobLevels.size(); //number of levels (any level)
    if (level < 0 || level >= (int16_t)job->JobLevels.size() || data < 0 || data > 22) return 0;
    return job->GetJobData(level, data);
}

STR* TS1JobProvider::JobStrings(int16_t jobType)
{
    if (!_WorkStringsTried) {
        _WorkStringsTried = true;
        if (!_WorkPath.empty()) {
            std::unique_ptr<IffFile> iff(new IffFile());
            try {
                if (_WorkInArchive) iff->ReadFromFile(_WorkPath, _WorkOffset, _WorkLength, true, "STR#");
                else iff.reset(new IffFile(_WorkPath));
                _WorkStrings = std::move(iff);
            } catch (...) {}
        }
    }
    return _WorkStrings ? _WorkStrings->Get<STR>((uint16_t)jobType) : nullptr;
}

std::string TS1JobProvider::JobOffer(int16_t jobType, int32_t jobLevel)
{
    auto job = GetJob(jobType);
    std::string level = (job && jobLevel >= 0 && jobLevel < (int32_t)job->JobLevels.size()) ? job->JobLevels[(size_t)jobLevel].JobName : std::string();
    return (job ? job->Name : std::string("(unknown)")) + " career track for a " + level + ".";
}

int16_t TS1JobProvider::SetToNext(int16_t current)
{
    auto it = Jobs.upper_bound(current);
    return (it == Jobs.end()) ? (int16_t)-1 : it->first;
}

}}}
