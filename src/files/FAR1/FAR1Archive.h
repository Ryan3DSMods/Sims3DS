/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/. 
 */
// Translated from FreeSims sims.files/FAR1/FAR1Archive.cs
#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include <utility>
#include "files/FAR1/FarEntry.h"

namespace FSO { namespace Files { namespace FAR1 {

/// <summary>
/// A FAR1 (File Archive v1) archive.
/// </summary>
class FAR1Archive
{
private:
    std::string m_Path;
    bool m_Open = false; // TS1(sims): no FILE* held: reads go through FSO::Files::FilePool (src/files/FilePool.h)

    uint32_t m_ManifestOffset = 0;
    uint32_t m_NumFiles = 0;
    std::vector<FarEntry> m_Entries;

    std::vector<uint8_t> ReadBytesAt(int64_t offset, int32_t length);

public:
    /// <summary>
    /// The offset into the archive of the manifest.
    /// </summary>
    uint32_t ManifestOffset() const { return m_ManifestOffset; }

    /// <summary>
    /// The number of files/entries in the archive.
    /// </summary>
    uint32_t NumFiles() const { return m_NumFiles; }

    /// <summary>
    /// Creates a new FAR1Archive instance from a path.
    /// </summary>
    /// <param name="Path">The path to the archive.</param>
    /// <param name="v1b">FAR 1b (16-bit filename length) rather than 1a.</param>
    FAR1Archive(const std::string& Path, bool v1b);
    ~FAR1Archive();

    FAR1Archive(const FAR1Archive&) = delete;
    FAR1Archive& operator=(const FAR1Archive&) = delete;

    /// <summary>
    /// Gets an entry based on a KeyValuePair.
    /// </summary>
    /// <param name="Entry">A KeyValuePair (string, byte[]) representing the entry. The byte array can be null.</param>
    /// <returns>The entry's data, or an empty vector if the entry wasn't found (sets found=false).</returns>
    std::vector<uint8_t> GetEntry(const std::pair<std::string, std::vector<uint8_t>>& Entry, bool* found = nullptr);
    /// Convenience: lookup by filename.
    std::vector<uint8_t> GetEntry(const std::string& Filename, bool* found = nullptr);

    /// <summary>
    /// Gets an entry's data from a FarEntry instance.
    /// </summary>
    std::vector<uint8_t> GetEntry(const FarEntry& Entry, bool* found = nullptr);

    /// <summary>
    /// Returns a list of all FarEntry instances in this archive.
    /// </summary>
    std::vector<FarEntry>& GetAllFarEntries();
    const std::string& Path() const { return m_Path; }
    /// TS1(sims): frees the manifest once the caller has its own index (GetEntry(FarEntry) keeps working,
    /// GetEntry(name) / GetAllFarEntries no longer find anything).
    void DropManifest() { std::vector<FarEntry>().swap(m_Entries); }

    /// <summary>
    /// Gets all entries in the archive.
    /// </summary>
    /// <returns>A List of KeyValuePair instances.</returns>
    std::vector<std::pair<std::string, std::vector<uint8_t>>> GetAllEntries();

    void Close();
};

}}} // namespace FSO::Files::FAR1
