/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/HIT/HITFile.cs
#include "files/HIT/HITFile.h"
#include "files/HIT/HITText.h"
#include "core/io.h"

namespace FSO { namespace Files { namespace HIT {

HITFile::HITFile(const std::vector<uint8_t>& Filedata)
{
    ReadFile(Filedata);
}

HITFile::HITFile(const std::string& Filepath)
{
    ReadFile(Text::ReadAllBytes(Filepath));
}

void HITFile::ReadFile(const std::vector<uint8_t>& data)
{
    st::Reader Reader(data, st::Endian::Little);

    MagicNumber = Reader.cstr(4, false);
    MajorVersion = Reader.u32();
    MinorVersion = Reader.u32();
    auto signature = Reader.cstr(4, false);
    (void)signature;

    auto tableLoc = FindBytePattern(data, std::vector<uint8_t>{ (uint8_t)'E', (uint8_t)'N', (uint8_t)'T', (uint8_t)'P' });
    if (tableLoc != -1)
    {
        Reader.seek((size_t)tableLoc);
        EntryPointByTrackID.clear();

        // C# loops until "EENT" (an EndOfStreamException ends a table without terminator)
        while (Reader.pos() + 4 <= Reader.size())
        {
            auto EndTest = Text::ToLower(Reader.cstr(4, false)); //can be invalid chars
            if (EndTest == "eent")
            {
                break;
            }
            else
            {
                Reader.seek(Reader.pos() - 4); //go back to read it as a table entry
                if (Reader.pos() + 8 > Reader.size()) break;
                auto track = Reader.u32();
                auto address = Reader.u32();
                EntryPointByTrackID.emplace(track, address); // C# Dictionary.Add
            }
        }
    }

    this->Data = data;
}

int32_t HITFile::FindBytePattern(const std::vector<uint8_t>& stream, const std::vector<uint8_t>& pattern)
{ //a simple pattern matcher
    for (size_t i = 0; i + pattern.size() <= stream.size(); i++)
    {
        bool match = true;
        for (size_t j = 0; j < pattern.size(); j++)
        {
            if (stream[i + j] != pattern[j]) { match = false; break; }
        }
        if (match) return (int32_t)(i + pattern.size()); // C#: stream position after the pattern
    }
    return -1; //no match
}

}}} // namespace FSO::Files::HIT
