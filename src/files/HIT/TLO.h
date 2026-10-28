/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/HIT/TLO.cs
//ok, why do we have support for this? it's only used by hitlab and only the hitlab test files have them...
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace FSO { namespace Files { namespace HIT {

/// <summary>
/// Represents a section in a tracklogic file.
/// </summary>
class TLOSection
{
public:
    std::string Name;
    uint32_t GroupID1 = 0;
    uint32_t FileID1 = 0;
    uint32_t GroupID2 = 0;
    uint32_t FileID2 = 0;
    uint32_t TypeID = 0;
    uint32_t GroupID3 = 0;
    uint32_t FileID3 = 0;
};

/// <summary>
/// Represents a TLO file.
/// TLO (short for Track Logic according to hitlab.ini) is a format
/// used solely for Hitlab. Integers are little-endian.
/// </summary>
class TLO
{
private:
    uint32_t m_Count = 0;
    void Read(const std::vector<uint8_t>& Filedata);
public:
    std::vector<TLOSection> Sections;

    /// <summary>
    /// Creates a new tracklogic instance.
    /// </summary>
    /// <param name="Filedata">The data to create the tracklogic instance from.</param>
    explicit TLO(const std::vector<uint8_t>& Filedata);

    /// <summary>
    /// Creates a new tracklogic instance.
    /// </summary>
    /// <param name="Filepath">The path to the tracklogic file to read.</param>
    explicit TLO(const std::string& Filepath);
};

}}} // namespace FSO::Files::HIT
