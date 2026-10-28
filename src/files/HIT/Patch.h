/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/HIT/Patch.cs
#pragma once
#include <cstdint>
#include <string>

namespace FSO { namespace Files { namespace HIT {

class Patch
{
public:
    std::string Name;
    std::string Filename;
    bool Looped = false;
    bool Piano = false;

    uint32_t FileID = 0; //patches are stubbed out in TSO.
    bool TSO = false;

    Patch() {}
    explicit Patch(uint32_t id);
    explicit Patch(const std::string& patchString);
};

}}} // namespace FSO::Files::HIT
