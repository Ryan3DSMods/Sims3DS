/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/. 
 */
// Translated from FreeSims sims.files/formats/otf/OTFFile.cs (OTF.cs is a duplicate under TSO.Files.formats.otf)
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace FSO { namespace Files { namespace Formats { namespace OTF {

//BELOW CLASSES NEEDS DOCUMENTATION - THANKS DARREN!!

class OTFTableKey
{
public:
    int32_t ID = 0;
    std::string Label;
    int32_t Value = 0;
};

class OTFTable
{
public:
    int32_t ID = 0;
    std::string Name;
    std::vector<OTFTableKey> Keys;

    OTFTableKey* GetKey(int32_t id)
    {
        for (auto& k : Keys) if (k.ID == id) return &k;
        return nullptr;
    }
};

/// <summary>
/// Object Tuning File (OTF) is an SGML format which defines tuning constants.
/// </summary>
class OTFFile
{
public:
    /// <summary>
    /// Constructs an OTF instance from a filepath.
    /// </summary>
    explicit OTFFile(const std::string& filepath);

    OTFFile()
    {
        //you can also create empty OTFs!
    }

    std::vector<OTFTable> Tables;

    /// <summary>
    /// Gets an OTFTable instance from an ID.
    /// </summary>
    OTFTable* GetTable(int32_t ID)
    {
        for (auto& t : Tables) if (t.ID == ID) return &t;
        return nullptr;
    }

    /// <summary>
    /// Reads an OTF from a byte buffer. (C# uses System.Xml; a minimal tag/attribute
    /// scanner for the <T i n><K i l v/></T> structure is used here.)
    /// </summary>
    void Read(const std::vector<uint8_t>& data);
};

}}}} // namespace FSO::Files::Formats::OTF
