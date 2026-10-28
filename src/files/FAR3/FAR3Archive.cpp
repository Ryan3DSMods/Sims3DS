/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/. 
 */
// Translated from FreeSims sims.files/FAR3/FAR3Archive.cs
#include "files/FAR3/FAR3Archive.h"
#include "files/FAR3/Decompresser.h"

namespace FSO { namespace Files { namespace FAR3 {

bool FAR3Archive::isReadingSomething = false;

uint8_t FAR3Archive::ReadU8() { uint8_t b; if (std::fread(&b, 1, 1, m_Reader) != 1) throw FAR3Exception("FAR3: read past end"); return b; }
uint16_t FAR3Archive::ReadU16() { uint8_t b[2]; if (std::fread(b, 1, 2, m_Reader) != 2) throw FAR3Exception("FAR3: read past end"); return (uint16_t)(b[0] | (b[1] << 8)); }
uint32_t FAR3Archive::ReadU32() { uint8_t b[4]; if (std::fread(b, 1, 4, m_Reader) != 4) throw FAR3Exception("FAR3: read past end"); return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24); }
std::vector<uint8_t> FAR3Archive::ReadBytes(size_t n)
{
    std::vector<uint8_t> v(n);
    size_t got = n ? std::fread(v.data(), 1, n, m_Reader) : 0;
    v.resize(got); //BinaryReader.ReadBytes returns a short array at EOF
    return v;
}

FAR3Archive::FAR3Archive(const std::string& Path)
{
    m_ArchivePath = Path;

    if (isReadingSomething == false)
    {
        isReadingSomething = true;

        m_Reader = std::fopen(Path.c_str(), "rb");
        if (!m_Reader)
        {
            isReadingSomething = false;
            throw FAR3Exception("Could not open the specified archive - " + Path + "! (FAR3Archive())");
        }

        auto hdr = ReadBytes(8);
        std::string Header(hdr.begin(), hdr.end());
        uint32_t Version = ReadU32();

        if ((Header != "FAR!byAZ") || (Version != 3))
        {
            isReadingSomething = false;
            throw FAR3Exception("Archive wasn't a valid FAR V.3 archive! (FAR3Archive())");
        }

        uint32_t ManifestOffset = ReadU32();
        m_ManifestOffset = ManifestOffset;

        std::fseek(m_Reader, (long)ManifestOffset, SEEK_SET);

        uint32_t NumFiles = ReadU32();

        for (uint32_t i = 0; i < NumFiles; i++)
        {
            Far3Entry Entry;
            Entry.DecompressedFileSize = ReadU32();
            auto Dummy = ReadBytes(3);
            Entry.CompressedFileSize = (uint32_t)((Dummy[0] << 0) | (Dummy[1] << 8) | (Dummy[2]) << 16);
            Entry.DataType = ReadU8();
            Entry.DataOffset = ReadU32();
            //Entry.HasFilename = m_Reader.ReadUInt16();
            Entry.IsCompressed = ReadU8();
            Entry.AccessNumber = ReadU8();
            Entry.FilenameLength = ReadU16();
            Entry.TypeID = ReadU32();
            Entry.FileID = ReadU32();
            auto name = ReadBytes(Entry.FilenameLength);
            Entry.Filename = std::string(name.begin(), name.end());

            if (m_Entries.find(Entry.Filename) == m_Entries.end())
                m_Entries[Entry.Filename] = Entry;
            m_EntriesList.push_back(Entry);

            //C#: m_EntryByID.Add (throws on duplicate) //isn't this a bad idea? i have a feeling this is a bad idea...
            if (m_EntryByID.find(Entry.FileID) != m_EntryByID.end())
            {
                isReadingSomething = false;
                throw FAR3Exception("An item with the same key has already been added.");
            }
            m_EntryByID[Entry.FileID] = Entry;
        }

        //Keep the stream open, it helps peformance.
        //m_Reader.Close();
        isReadingSomething = false;
    }
}

FAR3Archive::~FAR3Archive()
{
    Dispose();
}

std::vector<uint8_t> FAR3Archive::GetEntry(const Far3Entry& Entry)
{
    //lock (m_Reader) dropped: single threaded
    {
        if (!m_Reader) throw FAR3Exception("FAR3Archive is closed - FAR3Archive.GetEntry()");
        std::fseek(m_Reader, (long)Entry.DataOffset, SEEK_SET);

        isReadingSomething = true;

        if (Entry.IsCompressed == 0x01)
        {
            ReadBytes(9);
            uint32_t Filesize = ReadU32();
            uint16_t CompressionID = ReadU16();

            if (CompressionID == 0xFB10)
            {
                auto Dummy = ReadBytes(3);
                uint32_t DecompressedSize = (uint32_t)((Dummy[0] << 0x10) | (Dummy[1] << 0x08) | +Dummy[2]);

                Decompresser Dec;
                Dec.CompressedSize(Filesize);
                Dec.DecompressedSize(DecompressedSize);

                auto DecompressedData = Dec.Decompress(ReadBytes((size_t)Filesize));
                //m_Reader.Close();

                isReadingSomething = false;

                return DecompressedData;
            }
            else
            {
                std::fseek(m_Reader, (long)(std::ftell(m_Reader) - 15), SEEK_SET);

                auto Data = ReadBytes((size_t)Entry.DecompressedFileSize);
                //m_Reader.Close();

                isReadingSomething = false;

                return Data;
            }
        }
        else
        {
            auto Data = ReadBytes((size_t)Entry.DecompressedFileSize);
            //m_Reader.Close();

            isReadingSomething = false;

            return Data;
        }
    }

    //unreachable in C# too: throw new FAR3Exception("FAR3Entry didn't exist in archive - FAR3Archive.GetEntry()");
}

std::vector<std::pair<uint32_t, std::vector<uint8_t>>> FAR3Archive::GetAllEntries()
{
    std::vector<std::pair<uint32_t, std::vector<uint8_t>>> toReturn;
    for (Far3Entry& Entry : m_EntriesList)
    {
        toReturn.push_back(std::make_pair(Entry.FileID, GetEntry(Entry)));
    }
    return toReturn;
}

std::vector<Far3Entry> FAR3Archive::GetAllFAR3Entries()
{
    std::vector<Far3Entry> Entries;

    for (auto& KVP : m_Entries)
        Entries.push_back(KVP.second);

    return Entries;
}

std::vector<uint8_t> FAR3Archive::GetItemByID(uint32_t FileID)
{
    auto it = m_EntryByID.find(FileID);
    if (it == m_EntryByID.end())
    {
        throw FAR3Exception("Didn't find entry!");
    }
    return GetEntry(it->second);
}

std::vector<uint8_t> FAR3Archive::GetItemByID(uint64_t ID)
{
    //BitConverter.GetBytes(ID): FileID = high 4 bytes, TypeID = low 4 bytes (little endian)
    uint32_t FileID = (uint32_t)(ID >> 32);
    uint32_t TypeID = (uint32_t)(ID & 0xFFFFFFFFu);

    auto it = m_EntryByID.find(FileID);
    if (it == m_EntryByID.end() || it->second.TypeID != TypeID)
    {
        throw FAR3Exception("Didn't find entry!");
    }

    return GetEntry(it->second);
}

std::vector<uint8_t> FAR3Archive::operator[](const std::string& Filename)
{
    auto it = m_Entries.find(Filename);
    if (it == m_Entries.end()) throw FAR3Exception("The given key was not present in the dictionary.");
    return GetEntry(it->second);
}

void FAR3Archive::Dispose()
{
    if (m_Reader != nullptr)
    {
        std::fclose(m_Reader);
        m_Reader = nullptr;
    }
}

}}} // namespace FSO::Files::FAR3
