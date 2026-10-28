/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/formats/iff/chunks/STR.cs
#include "files/formats/iff/chunks/STR.h"
#include "files/formats/iff/IffFile.h"

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

const char* const STR::LanguageSetNames[20] =
{
    "English (US)",
    "English (UK)",
    "French",
    "German",
    "Italian",
    "Spanish",
    "Dutch",
    "Danish",
    "Swedish",
    "Norwegian",
    "Finish",
    "Hebrew",
    "Russian",
    "Portuguese",
    "Japanese",
    "Polish",
    "Simplified Chinese",
    "Traditional Chinese",
    "Thai",
    "Korean"
};

STRLangCode STR::DefaultLangCode = STRLangCode::EnglishUS;

STR::STR()
{
    LanguageSets.resize(20);
    for (int i = 0; i < 20; i++) LanguageSets[i] = STRLanguageSet();
}

STRLanguageSet* STR::GetLanguageSet(STRLangCode set)
{
    if (set == STRLangCode::Default) set = DefaultLangCode;
    int code = (int)set;
    if (LanguageSets[code-1].Strings.size() == 0) return &LanguageSets[0]; //if undefined, fallback to English US
    else return &LanguageSets[code-1];
}

bool STR::IsSetInit(STRLangCode set)
{
    if (set == STRLangCode::Default) set = DefaultLangCode;
    if (set == STRLangCode::EnglishUS) return true;
    int code = (int)set;
    return (LanguageSets[code - 1].Strings.size() > 0);
}

void STR::InitLanguageSet(STRLangCode set)
{
    if (set == STRLangCode::Default) set = DefaultLangCode;
    int code = (int)set;
    auto length = LanguageSets[0].Strings.size();
    LanguageSets[code - 1].Strings = std::vector<STRItem>(length);
    for (size_t i=0; i< length; i++)
    {
        auto& src = LanguageSets[0].Strings[i];
        STRItem item;
        item.LanguageCode = (uint8_t)code;
        item.Value = src.Value;
        item.Comment = src.Comment;
        LanguageSets[code - 1].Strings[i] = item;
    }
}

std::string STR::GetString(int32_t index)
{
    return GetString(index, STRLangCode::Default);
}

std::string STR::GetString(int32_t index, STRLangCode language)
{
    auto item = GetStringEntry(index, language);
    if (item != nullptr)
    {
        return item->Value;
    }
    return std::string(); //null
}

bool STR::TryGetString(int32_t index, STRLangCode language, std::string& out)
{
    auto item = GetStringEntry(index, language);
    if (item != nullptr)
    {
        out = item->Value;
        return true;
    }
    return false;
}

void STR::SetString(int32_t index, const std::string& value)
{
    SetString(index, value, STRLangCode::Default);
}

void STR::SetString(int32_t index, const std::string& value, STRLangCode language)
{
    auto languageSet = GetLanguageSet(language);
    if (index < (int32_t)languageSet->Strings.size())
    {
        languageSet->Strings[index].Value = value;
    }
}

void STR::SwapString(int32_t srcindex, int32_t dstindex)
{
    for (auto& languageSet : LanguageSets)
    {
        if (languageSet.Strings.size() == 0) continue; //language not initialized
        auto temp = languageSet.Strings[srcindex];
        languageSet.Strings[srcindex] = languageSet.Strings[dstindex];
        languageSet.Strings[dstindex] = temp;
    }
}

void STR::InsertString(int32_t index, const STRItem& item)
{
    uint8_t i = 1;
    for (auto& languageSet : LanguageSets) {
        if (languageSet.Strings.size() == 0 && i > 1)
        {
            i++;
            continue; //language not initialized
        }
        STRItem newItem;
        newItem.LanguageCode = i;
        newItem.Value = item.Value;
        newItem.Comment = item.Comment;
        languageSet.Strings.insert(languageSet.Strings.begin() + index, newItem);
        i++;
    }
}

void STR::RemoveString(int32_t index)
{
    for (auto& languageSet : LanguageSets)
    {
        if (languageSet.Strings.size() == 0) continue; //language not initialized
        languageSet.Strings.erase(languageSet.Strings.begin() + index);
    }
}

STRItem* STR::GetStringEntry(int32_t index)
{
    return GetStringEntry(index, STRLangCode::Default);
}

STRItem* STR::GetStringEntry(int32_t index, STRLangCode language)
{
    auto languageSet = GetLanguageSet(language);
    if (index >= 0 && index < (int32_t)languageSet->Strings.size())
    {
        return &languageSet->Strings[index];
    }
    return nullptr;
}

void STR::Read(IffFile* iff, st::Reader& stream)
{
    st::Reader& io = stream;
    io.ByteOrder(st::Endian::Little);
    {
        auto formatCode = io.ReadInt16();
        LanguageSets = std::vector<STRLanguageSet>(20);
        if (!io.HasMore()){
            for (int i = 0; i < 20; i++) LanguageSets[i] = STRLanguageSet();
            return;
        }

        if (formatCode == 0)
        {
            auto numStrings = io.ReadUInt16();
            for (int i = 0; i < 20; i++) LanguageSets[i] = STRLanguageSet();
            LanguageSets[0].Strings = std::vector<STRItem>(numStrings);
            for (int i = 0; i < numStrings; i++)
            {
                STRItem item;
                item.Value = io.ReadPascalString();
                LanguageSets[0].Strings[i] = item;
            }
        }
        //This format changed 00 00 to use C strings rather than Pascal strings.
        else if (formatCode == -1)
        {
            auto numStrings = io.ReadUInt16();
            for (int i = 0; i < 20; i++) LanguageSets[i] = STRLanguageSet();
            LanguageSets[0].Strings = std::vector<STRItem>(numStrings);
            for (int i = 0; i < numStrings; i++)
            {
                STRItem item;
                item.Value = io.ReadNullTerminatedUTF8();
                LanguageSets[0].Strings[i] = item;
            }
        }
        //This format changed FF FF to use string pairs rather than single strings.
        else if (formatCode == -2)
        {
            auto numStrings = io.ReadUInt16();
            for (int i = 0; i < 20; i++) LanguageSets[i] = STRLanguageSet();
            LanguageSets[0].Strings = std::vector<STRItem>(numStrings);
            for (int i = 0; i < numStrings; i++)
            {
                STRItem item;
                item.Value = io.ReadNullTerminatedString();
                item.Comment = io.ReadNullTerminatedString();
                LanguageSets[0].Strings[i] = item;
            }
        }
        //This format changed FD FF to use a language code.
        else if (formatCode == -3)
        {
            auto numStrings = io.ReadUInt16();
            for (int i = 0; i < 20; i++) LanguageSets[i] = STRLanguageSet();
            std::vector<STRItem> LangSort[20];
            bool LangSortUsed[20] = {false};
            // FIX(sims) memory: install files opened read-only (IffFile::ReadFromFile: objects, character files,
            // globals) keep only US English and the default language; the game reads nothing else (GetLanguageSet
            // falls back to US English) and never writes these chunks back. The other 19 language sets were ~2/3
            // of the decoded string memory (object and character STR#s carry all translations).
            bool onlyDefault = iff != nullptr && !iff->SourcePath.empty() && !IffFile::RETAIN_CHUNK_DATA;
            int keepLang = (int)DefaultLangCode;
            for (int i = 0; i < numStrings; i++)
            {
                STRItem item;
                item.LanguageCode = io.ReadByte();
                if (onlyDefault && item.LanguageCode > 1 && item.LanguageCode != keepLang) {
                    io.ReadNullTerminatedString(); io.ReadNullTerminatedString();
                    continue;
                }
                item.Value = io.ReadNullTerminatedString();
                item.Comment = io.ReadNullTerminatedString();

                int lang = item.LanguageCode;
                if (lang == 0) lang = 1;
                else if (lang < 0 || lang > 20) continue; //???
                LangSortUsed[lang - 1] = true;

                LangSort[lang - 1].push_back(item);
            }
            for (size_t i=0; i<LanguageSets.size(); i++)
            {
                if (LangSortUsed[i]) LanguageSets[i].Strings = LangSort[i];
            }
        }

        //This format is only used in The Sims Online. The format is essentially a performance improvement:
        //it counteracts both the short string limit of 255 characters found in 00 00 and the inherent slowness
        //of null-terminated strings in the other formats (which requires two passes over each string), and it
        //also provides a string pair count for each language set which eliminates the need for two passes over
        //each language set.
        else if (formatCode == -4)
        {
            auto numLanguageSets = io.ReadByte();
            this->LanguageSets = std::vector<STRLanguageSet>(numLanguageSets);

            for (int i=0; i < numLanguageSets; i++)
            {
                STRLanguageSet item;
                auto numStringPairs = io.ReadUInt16();
                item.Strings = std::vector<STRItem>(numStringPairs);
                for (int x = 0; x < numStringPairs; x++)
                {
                    STRItem str;
                    str.LanguageCode = (uint8_t)(io.ReadByte() + 1);
                    str.Value = io.ReadVariableLengthPascalString();
                    str.Comment = io.ReadVariableLengthPascalString();
                    item.Strings[x] = str;
                }
                this->LanguageSets[i] = item;
            }
        }
    }
}

bool STR::Write(IffFile* iff, st::Writer& stream)
{
    (void)iff;
    st::Writer& io = stream;
    io.ByteOrder(st::Endian::Little);
    {
        io.WriteInt16(-4);
        io.WriteByte(20);

        for (auto& set : LanguageSets)
        {
            io.WriteUInt16((uint16_t)set.Strings.size());
            for (auto& str : set.Strings)
            {
                io.WriteByte((uint8_t)(str.LanguageCode - 1));
                io.WriteVariableLengthPascalString(str.Value);
                io.WriteVariableLengthPascalString(str.Comment);
            }
        }

        return true;
    }
}

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
