/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/HIT/EVT.cs
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace FSO { namespace Files { namespace HIT {

class EVTEntry
{
public:
    std::string Name;
    uint32_t EventType = 0;
    uint32_t TrackID = 0;
    uint32_t Unknown = 0;
    uint32_t Unknown2 = 0;
    uint32_t Unknown3 = 0;
    uint32_t Unknown4 = 0;
};

/// <summary>
/// EVT is a CSV format that defines a list of events for HIT to listen for.
/// </summary>
class EVT
{
public:
    std::vector<EVTEntry> Entries;

    /// <summary>
    /// Creates a new evt file.
    /// </summary>
    /// <param name="Filedata">The data to create the evt from.</param>
    explicit EVT(const std::vector<uint8_t>& Filedata);

    /// <summary>
    /// Creates a new evt file.
    /// </summary>
    /// <param name="Filedata">The path to the data to create the evt from.</param>
    explicit EVT(const std::string& Filepath);

private:
    void ReadFile(const std::vector<uint8_t>& data);
    uint32_t ParseHexString(std::string input);
};

}}} // namespace FSO::Files::HIT
