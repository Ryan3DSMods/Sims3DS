/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/. 
 */
// Translated from FreeSims sims.files/Tuning.cs
#include "files/Tuning.h"
#include "files/FAR3/Decompresser.h"
#include <stdexcept>
#include <cstdlib>
#include <cctype>

namespace FSO { namespace Files {

float TuningEntry::GetNum(const std::string& key) const
{
    auto it = KeyValues.find(key);
    if (it == KeyValues.end()) throw std::runtime_error("The given key was not present in the dictionary.");
    return std::strtof(it->second.c_str(), nullptr); //en-US culture: '.' decimal separator
}

Tuning::Tuning(const std::vector<uint8_t>& Data)
{
    st::Reader m_Reader(Data, st::Endian::Little);
    Create(m_Reader);
}

Tuning::Tuning(const std::string& Path)
{
    std::vector<uint8_t> data;
    if (!st::ReadAllBytes(Path, data))
    {
        throw std::runtime_error("Tuning.cs: Invalid path: "+Path);
    }
    st::Reader m_Reader(data, st::Endian::Little);
    Create(m_Reader);
}

void Tuning::Create(st::Reader& m_Reader)
{
    uint8_t BodyType = m_Reader.ReadByte();

    if (BodyType != 0x01 && BodyType != 0x03)
        throw std::runtime_error("Tuning.cs: Unknown Persist BodyType!");

    uint32_t DecompressedSize = m_Reader.ReadUInt32();
    uint32_t CompressedSize = m_Reader.ReadUInt32();
    uint32_t StreamBodySize = m_Reader.ReadUInt32(); //same as compressed size for all current examples
    (void)StreamBodySize;
    //Note: wiki.niotso.org says this is actually one byte Compressor and four bytes CompressionParameters.
    uint16_t CompressionID = m_Reader.ReadUInt16();

    if (CompressionID != 0xFB10)
        throw std::runtime_error("Tuning.cs: Unknown CompressionID!");

    auto Dummy = m_Reader.ReadBytes(3);
    uint32_t DecompressedSize2 = (uint32_t)((Dummy[0] << 0x10) | (Dummy[1] << 0x08) | +Dummy[2]);
    (void)DecompressedSize2;

    FAR3::Decompresser Dec;
    Dec.CompressedSize(CompressedSize);
    Dec.DecompressedSize(DecompressedSize);

    auto decompressedData = Dec.Decompress(m_Reader.ReadBytes((size_t)CompressedSize));

    st::Reader Reader(decompressedData, st::Endian::Little);

    EntryCount = Reader.ReadUInt32();

    for (uint32_t i = 0; i < EntryCount; i++)
    {
        TuningEntry Entry;
        Entry.EntryName = DecodeString(Reader);
        Entry.KeyValueCount = Reader.ReadUInt32();

        for (uint32_t j = 0; j < Entry.KeyValueCount; j++)
        {
            std::string Key = DecodeString(Reader);
            std::string Val = DecodeString(Reader);

            if (Entry.KeyValues.find(Key) != Entry.KeyValues.end()) throw std::runtime_error("An item with the same key has already been added.");
            Entry.KeyValues[Key] = Val;
        }
        std::string lower = Entry.EntryName;
        for (auto& c : lower) c = (char)std::tolower((unsigned char)c);
        if (EntriesByName.find(lower) != EntriesByName.end()) throw std::runtime_error("An item with the same key has already been added.");
        EntriesByName[lower] = Entry;
    }
}

std::string Tuning::DecodeString(st::Reader& reader)
{
    int32_t length = 0;
    uint8_t last = reader.ReadByte();
    uint8_t j = 0;
    while ((last & 0x80) > 0)
    {
        length |= (last&0x7F) << ((j++) * 7);
        last = reader.ReadByte();
    }
    length |= last << ((j++) * 7);

    auto Chars = reader.ReadBytes(length < 0 ? 0 : (size_t)length);

    for(size_t i = 0; i < Chars.size(); i++)
        Chars[i] = (uint8_t)(Chars[i] - 13);

    return std::string(Chars.begin(), Chars.end());
}

}} // namespace FSO::Files
