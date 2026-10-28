/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/HIT/HSM.cs
#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace FSO { namespace Files { namespace HIT {

class HSM
{
public:
    /// <summary>
    /// HSM is a plaintext format that names various HIT constants including subroutine locations.
    /// </summary>
    std::unordered_map<std::string, int32_t> Constants;

    /// <summary>
    /// Creates a new hsm file.
    /// </summary>
    /// <param name="Filedata">The data to create the hsm from.</param>
    explicit HSM(const std::vector<uint8_t>& Filedata);

    /// <summary>
    /// Creates a new hsm file.
    /// </summary>
    /// <param name="Filedata">The path to the data to create the hsm from.</param>
    explicit HSM(const std::string& Filepath);

private:
    void ReadFile(const std::vector<uint8_t>& stream);
};

}}} // namespace FSO::Files::HIT
