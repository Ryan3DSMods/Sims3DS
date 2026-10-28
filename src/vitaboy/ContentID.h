/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.Common.Content.ContentID (sims.common/content/ContentID.cs), header-only.
// Lives with Vitaboy because only the avatar code uses it so far.
#pragma once
#include <cstdint>
#include <string>

namespace FSO { namespace Common { namespace Content {

/// <summary>
/// Represents the ID of a content resource (TSO type/file id pair, or a TS1 file name).
/// </summary>
class ContentID {
public:
    uint32_t TypeID = 0;
    uint32_t FileID = 0;
    std::string FileName; // TS1: empty == C# null
    bool HasFileName = false;

    ContentID() {}
    ContentID(uint32_t typeID, uint32_t fileID) : TypeID(typeID), FileID(fileID) {}
    explicit ContentID(const std::string& name) : FileName(name), HasFileName(true) {}
    /// Shared ID: (FileID << 32) | TypeID as in the C# Shift/ToUInt64.
    explicit ContentID(uint64_t id) : TypeID((uint32_t)id), FileID((uint32_t)(id >> 32)) {}
    uint64_t Shift() const { return ((uint64_t)FileID << 32) | TypeID; }
};

}}}
