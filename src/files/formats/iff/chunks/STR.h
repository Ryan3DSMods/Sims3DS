/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/formats/iff/chunks/STR.cs
#pragma once
#include "files/formats/iff/AbstractIffChunk.h"
#include <array>

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

enum class STRLangCode : uint8_t
{
    Default = 0,
    EnglishUS = 1,
    EnglishUK = 2,
    French = 3,
    German = 4,
    Italian = 5,
    Spanish = 6,
    Dutch = 7,
    Danish = 8,
    Swedish = 9,
    Norwegian = 10,
    Finish = 11,
    Hebrew = 12,
    Russian = 13,
    Portuguese = 14,
    Japanese = 15,
    Polish = 16,
    SimplifiedChinese = 17,
    TraditionalChinese = 18,
    Thai = 19,
    Korean = 20
};

/// <summary>
/// Item in a STR chunk.
/// </summary>
class STRItem
{
public:
    uint8_t LanguageCode = 0;
    std::string Value;
    std::string Comment;
};

/// <summary>
/// Set of STRItems for a language.
/// </summary>
class STRLanguageSet
{
public:
    std::vector<STRItem> Strings;
};

/// <summary>
/// This chunk type holds text strings.
/// The first two bytes correspond to the format code, of which there are four types.
/// Some chunks in the game do not specify any data after the version number, so be sure to implement bounds checking.
/// </summary>
class STR : public IffChunk
{
public:
    IFF_CHUNK_TYPE("STR#")

    static const char* const LanguageSetNames[20];

    std::vector<STRLanguageSet> LanguageSets; //20 entries (fewer only for format -4)
    static STRLangCode DefaultLangCode;

    STR();

    /// <summary>
    /// How many strings are in this chunk?
    /// </summary>
    int32_t Length() const
    {
        return (int32_t)LanguageSets[0].Strings.size();
    }

    STRLanguageSet* GetLanguageSet(STRLangCode set);

    bool IsSetInit(STRLangCode set);

    void InitLanguageSet(STRLangCode set);

    /// <summary>
    /// Gets a string from this chunk.
    /// </summary>
    /// <param name="index">Index of string.</param>
    /// <returns>A string at specific index, empty if not found (C#: null).</returns>
    std::string GetString(int32_t index);
    std::string GetString(int32_t index, STRLangCode language);
    /// Like GetString but reports whether the entry existed (C# null vs value).
    bool TryGetString(int32_t index, STRLangCode language, std::string& out);

    void SetString(int32_t index, const std::string& value);
    void SetString(int32_t index, const std::string& value, STRLangCode language);

    void SwapString(int32_t srcindex, int32_t dstindex);

    void InsertString(int32_t index, const STRItem& item);

    void RemoveString(int32_t index);

    /// <summary>
    /// Gets a STRItem instance from this STR chunk.
    /// </summary>
    /// <param name="index">Index of STRItem.</param>
    /// <returns>STRItem at index, null if not found.</returns>
    STRItem* GetStringEntry(int32_t index);
    STRItem* GetStringEntry(int32_t index, STRLangCode language);

    /// <summary>
    /// Reads a STR chunk from a stream.
    /// </summary>
    /// <param name="iff">An Iff instance.</param>
    /// <param name="stream">A Stream object holding a STR chunk.</param>
    void Read(IffFile* iff, st::Reader& stream) override;
    bool Write(IffFile* iff, st::Writer& stream) override;
};

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
