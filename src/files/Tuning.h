/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/. 
 */
// Translated from FreeSims sims.files/Tuning.cs
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include "core/io.h"

namespace FSO { namespace Files {

struct TuningEntry
{
    std::string EntryName;
    uint32_t KeyValueCount = 0;
    std::unordered_map<std::string, std::string> KeyValues;

    std::string ToString() const
    {
        return EntryName;
    }

    float GetNum(const std::string& key) const;
};

//NOTE: important tuning variables:
//28: object wear repair - important because this stuff doen't run in simantics anymore
//36: sim motive decay
//24: object deprecation
//21: "motivesTunning": regen motives when not online, dead makes motives drain faster...
//17: motive score weights for different lot types

/// <summary>
/// Loader/parser for tuning.dat.
/// </summary>
class Tuning
{
public:
    uint32_t EntryCount = 0;
    std::unordered_map<std::string, TuningEntry> EntriesByName;

    explicit Tuning(const std::vector<uint8_t>& Data);

    explicit Tuning(const std::string& Path);

private:
    void Create(st::Reader& Reader);

    /// <summary>
    /// Reads a variable-length pascal string offset by 13 characters.
    /// </summary>
    std::string DecodeString(st::Reader& reader);
};

}} // namespace FSO::Files
