/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/HIT/Hitlist.cs
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace FSO { namespace Files { namespace HIT {

/// <summary>
/// HLS refers to two binary formats that both define a list of IDs, known as a hitlist.
/// One format is a Pascal string with a 4-byte, little-endian length, representing a
/// comma-seperated list of decimal values, or decimal ranges (e.g. "1025-1035"), succeeded
/// by a single LF newline.
/// </summary>
class Hitlist
{
private:
    uint32_t m_IDCount = 0;
    void Read(const std::vector<uint8_t>& data);
public:
    std::vector<uint32_t> IDs; //variable length so it's easier to fill with ranges

    /// <summary>
    /// Creates a new hitlist.
    /// </summary>
    /// <param name="Filedata">The data to create the hitlist from.</param>
    explicit Hitlist(const std::vector<uint8_t>& Filedata);
    Hitlist() {}

    void Populate(const std::string& str);

    static std::shared_ptr<Hitlist> HitlistFromString(const std::string& str);

    /// <summary>
    /// Creates a new hitlist.
    /// </summary>
    /// <param name="Filepath">The path to the hitlist to read.</param>
    explicit Hitlist(const std::string& Filepath);
};

}}} // namespace FSO::Files::HIT
