/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/. 
 */
// Translated from FreeSims sims.files/FAR1/FAR1Archive.cs
#include "files/FAR1/FAR1Archive.h"
#include "files/FilePool.h"
#include <cstring>
#include <stdexcept>

namespace FSO { namespace Files { namespace FAR1 {

namespace {
    uint32_t RdU32(const uint8_t* b) { return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24); }
}

// TS1(sims): reads go through the shared FilePool (no FILE* kept per archive: the 3DS runs out of handles).
std::vector<uint8_t> FAR1Archive::ReadBytesAt(int64_t offset, int32_t length)
{
    if (!m_Open) throw std::runtime_error("FAR1: archive closed");
    std::vector<uint8_t> data;
    std::string err;
    if (!FSO::Files::FilePool::ReadAt(m_Path, offset, length < 0 ? 0 : (size_t)length, data, &err))
        throw std::runtime_error("Could not read archive: " + err);
    return data; //BinaryReader.ReadBytes returns a short array at EOF
}

FAR1Archive::FAR1Archive(const std::string& Path, bool v1b)
{
    m_Path = Path;
    std::string err;
    //Magic number - An 8-byte string (not null-terminated), consisting of the ASCII characters "FAR!byAZ"
    //Version - A 4-byte unsigned integer specifying the version; 1a and 1b each specify 1.
    //File table offset - A 4-byte unsigned integer specifying the offset to the file table
    uint8_t hdr[16];
    int64_t got = FSO::Files::FilePool::ReadAt(Path, 0, hdr, 16, &err);
    if (got < 0) throw std::runtime_error("Could not open archive: " + err);
    if (got != 16 || std::memcmp(hdr, "FAR!byAZ", 8) != 0 || RdU32(hdr + 8) != 1)
        throw std::runtime_error("Archive wasn't a valid FAR V.1 archive! (" + Path + ")");
    m_Open = true;
    m_ManifestOffset = RdU32(hdr + 12);

    // the manifest runs to the end of the file: one read instead of 4-5 small reads per entry
    int64_t size = FSO::Files::FilePool::Size(Path, &err);
    if (size < (int64_t)m_ManifestOffset + 4) throw std::runtime_error("FAR1: truncated archive " + Path);
    std::vector<uint8_t> man = ReadBytesAt(m_ManifestOffset, (int32_t)(size - m_ManifestOffset));
    size_t p = 0;
    auto need = [&](size_t n) { if (p + n > man.size()) throw std::runtime_error("FAR1: read past end (" + Path + ")"); };
    need(4);
    m_NumFiles = RdU32(&man[p]); p += 4;
    m_Entries.reserve(m_NumFiles < 100000 ? m_NumFiles : 0);
    for (uint32_t i = 0; i < m_NumFiles; i++)
    {
        FarEntry Entry;
        need(v1b ? 14 : 16);
        Entry.DataLength = (int32_t)RdU32(&man[p]);
        Entry.DataLength2 = (int32_t)RdU32(&man[p + 4]);
        Entry.DataOffset = (int32_t)RdU32(&man[p + 8]);
        if (v1b) { Entry.FilenameLength = (int16_t)(man[p + 12] | (man[p + 13] << 8)); p += 14; }
        else { Entry.FilenameLength = (int16_t)RdU32(&man[p + 12]); p += 16; }
        size_t n = Entry.FilenameLength < 0 ? 0 : (size_t)Entry.FilenameLength;
        if (p + n > man.size()) n = man.size() - p; // BinaryReader.ReadBytes: short at EOF
        Entry.Filename.assign((const char*)man.data() + p, n);
        p += n;
        m_Entries.push_back(std::move(Entry));
    }
}

FAR1Archive::~FAR1Archive()
{
    Close();
}

std::vector<uint8_t> FAR1Archive::GetEntry(const std::pair<std::string, std::vector<uint8_t>>& Entry, bool* found)
{
    return GetEntry(Entry.first, found);
}

std::vector<uint8_t> FAR1Archive::GetEntry(const std::string& Filename, bool* found)
{
    for (FarEntry& Ent : m_Entries)
    {
        if (Ent.Filename == Filename)
        {
            if (found) *found = true;
            return ReadBytesAt(Ent.DataOffset, Ent.DataLength);
        }
    }

    if (found) *found = false;
    return std::vector<uint8_t>(); //null
}

std::vector<uint8_t> FAR1Archive::GetEntry(const FarEntry& Entry, bool* found)
{
    // TS1(sims): callers pass entries of this archive's manifest (or copies of them in their own index): read
    // them directly instead of searching the manifest by name (O(n) string compares per read; the manifest may have
    // been dropped, see DropManifest).
    if (Entry.DataLength >= 0 && Entry.DataOffset >= 0)
    {
        if (found) *found = true;
        return ReadBytesAt(Entry.DataOffset, Entry.DataLength);
    }
    for (FarEntry& Ent : m_Entries)
    {
        if (Ent.Filename == Entry.Filename)
        {
            if (found) *found = true;
            return ReadBytesAt(Ent.DataOffset, Ent.DataLength);
        }
    }

    if (found) *found = false;
    return std::vector<uint8_t>(); //null
}

std::vector<FarEntry>& FAR1Archive::GetAllFarEntries()
{
    return m_Entries;
}

std::vector<std::pair<std::string, std::vector<uint8_t>>> FAR1Archive::GetAllEntries()
{
    std::vector<std::pair<std::string, std::vector<uint8_t>>> Entries;

    for (FarEntry& Entry : m_Entries)
    {
        auto Data = ReadBytesAt(Entry.DataOffset, Entry.DataLength);
        Entries.push_back(std::make_pair(Entry.Filename, Data));
    }

    return Entries;
}

void FAR1Archive::Close()
{
    if (m_Open) { FSO::Files::FilePool::Close(m_Path); m_Open = false; }
}

}}} // namespace FSO::Files::FAR1
