/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/HIT/Hot.cs
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "files/HIT/EVT.h"
#include "files/HIT/HSM.h"
#include "files/HIT/Hitlist.h"
#include "files/HIT/Patch.h"
#include "files/HIT/Track.h"

namespace FSO { namespace Files { namespace HIT {

/// <summary>
/// Enumeration constants.
/// </summary>
struct EventMappingEquate
{
    std::string Label;
    int32_t Value = 0;
};

/// <summary>
/// This section assigns a sound ID to a number of subroutines
/// (exported or not) in the corresponding HIT file.
/// </summary>
struct TrackData
{
    //The syntax for TrackData is A = B, where A is the sound's File ID
    //and B is the offset to the subroutine in the accompanying HIT file.
    int64_t FileID = 0;
    int32_t SubRoutineOffset = 0;
};

enum class HotReadMode
{
    None,
    EventMappingEquate,
    Options,
    TrackData,
    EventMapping,
    Track,
    Patch,
    GlobalHitList
};

/// <summary>
/// HOT (short for HIT Options Table) is an ini format that defines
/// enumeration constants and track data for HIT binary files.
/// </summary>
class Hot
{
private:
    int32_t m_Version = 0;
    int32_t m_LoadPriority = 0;
    std::vector<EventMappingEquate> m_EventMappingEquations;
    std::vector<FSO::Files::HIT::TrackData> m_TrackDataList;
    std::unordered_map<std::string, int32_t> EventMappingEquate_;  // C#: private Dictionary EventMappingEquate

    int32_t HSMConst(const std::string& input);
    int32_t ParseEME(const std::string& eme);

public:
    std::unordered_map<uint32_t, FSO::Files::HIT::Track> Tracks;
    std::unordered_map<uint32_t, std::shared_ptr<Hitlist>> Hitlists;
    std::unordered_map<uint32_t, FSO::Files::HIT::Patch> Patches;
    std::unordered_map<uint32_t, uint32_t> TrackData;
    std::unordered_map<std::string, EVTEntry> Events;

    std::shared_ptr<HSM> AsmNames;

    /// <summary>
    /// Gets this Hot instance's list of EventMappingEquate instances.
    /// </summary>
    const std::vector<EventMappingEquate>& EventMappingEquations() const { return m_EventMappingEquations; }

    /// <summary>
    /// Gets this Hot instance's list of TrackData instances.
    /// </summary>
    const std::vector<FSO::Files::HIT::TrackData>& TrackDataList() const { return m_TrackDataList; }

    /// TS1(sims): the [Options] values (the C# kept them private); LoadPriority orders the EP banks.
    int32_t Version() const { return m_Version; }
    int32_t LoadPriority() const { return m_LoadPriority; }

    /// C# Hot(byte[] FileData): the old options-only reader (never stores anything but the options).
    explicit Hot(const std::vector<uint8_t>& FileData);

    void Load(const std::vector<uint8_t>& FileData);

    Hot(const std::string& Filepath, std::shared_ptr<HSM> myAsm);
};

}}} // namespace FSO::Files::HIT
