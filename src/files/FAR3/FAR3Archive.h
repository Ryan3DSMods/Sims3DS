/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/. 
 */
// Translated from FreeSims sims.files/FAR3/FAR3Archive.cs
#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include <utility>
#include <unordered_map>
#include "files/FAR3/Far3Entry.h"
#include "files/FAR3/FAR3Exception.h"

namespace FSO { namespace Files { namespace FAR3 {

/// <summary>
/// Represents a single FAR3 archive.
/// </summary>
class FAR3Archive
{
private:
    FILE* m_Reader = nullptr;
public:
    static bool isReadingSomething;

private:
    std::string m_ArchivePath;
    std::unordered_map<std::string, Far3Entry> m_Entries;
    std::vector<Far3Entry> m_EntriesList;
    std::unordered_map<uint32_t, Far3Entry> m_EntryByID;
    uint32_t m_ManifestOffset = 0;

    uint32_t ReadU32();
    uint16_t ReadU16();
    uint8_t ReadU8();
    std::vector<uint8_t> ReadBytes(size_t n);

public:
    /// <summary>
    /// Creates a new FAR3Archive instance from a path.
    /// </summary>
    /// <param name="Path">The path to the archive.</param>
    explicit FAR3Archive(const std::string& Path);
    ~FAR3Archive();
    FAR3Archive(const FAR3Archive&) = delete;
    FAR3Archive& operator=(const FAR3Archive&) = delete;

    /// <summary>
    /// Gets an entry's data from a Far3Entry instance.
    /// </summary>
    std::vector<uint8_t> GetEntry(const Far3Entry& Entry);

    /// <summary>
    /// Returns the entries of this FAR3Archive as byte arrays together with their corresponding FileIDs.
    /// </summary>
    std::vector<std::pair<uint32_t, std::vector<uint8_t>>> GetAllEntries();

    /// <summary>
    /// Returns the entries of this FAR3Archive as FAR3Entry instances in a List.
    /// </summary>
    std::vector<Far3Entry> GetAllFAR3Entries();

    /// <summary>
    /// Gets an entry from a FileID.
    /// </summary>
    std::vector<uint8_t> GetItemByID(uint32_t FileID);

    /// <summary>
    /// Gets an entry from its ID (TypeID + FileID).
    /// </summary>
    std::vector<uint8_t> GetItemByID(uint64_t ID);

    /// <summary>
    /// Gets an entry's data from a filename.
    /// </summary>
    std::vector<uint8_t> operator[](const std::string& Filename);

    /// <summary>
    /// Disposes this FAR3Archive instance.
    /// </summary>
    void Dispose();
};

}}} // namespace FSO::Files::FAR3
