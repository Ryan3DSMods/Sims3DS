/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/HIT/HITFile.cs
#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace FSO { namespace Files { namespace HIT {

/// <summary>
/// HIT files contain the bytecode to be executed when plaing track events.
/// </summary>
class HITFile
{
public:
    std::string MagicNumber;
    uint32_t MajorVersion = 0;
    uint32_t MinorVersion = 0;
    std::vector<uint8_t> Data;
    std::unordered_map<uint32_t, uint32_t> EntryPointByTrackID;

    /// <summary>
    /// Creates a new track.
    /// </summary>
    /// <param name="Filedata">The data to create the track from.</param>
    explicit HITFile(const std::vector<uint8_t>& Filedata);

    /// <summary>
    /// Creates a new track.
    /// </summary>
    /// <param name="Filedata">The path to the data to create the track from.</param>
    explicit HITFile(const std::string& Filepath);

    int32_t FindBytePattern(const std::vector<uint8_t>& stream, const std::vector<uint8_t>& pattern);

private:
    void ReadFile(const std::vector<uint8_t>& data);
};

}}} // namespace FSO::Files::HIT
