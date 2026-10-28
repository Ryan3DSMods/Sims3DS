/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/HIT/EVT.cs
#include "files/HIT/EVT.h"
#include "files/HIT/HITText.h"

namespace FSO { namespace Files { namespace HIT {

EVT::EVT(const std::vector<uint8_t>& Filedata)
{
    ReadFile(Filedata);
}

EVT::EVT(const std::string& Filepath)
{
    ReadFile(Text::ReadAllBytes(Filepath));
}

void EVT::ReadFile(const std::vector<uint8_t>& data)
{
    Entries.clear();
    // C#: split on "\r\n" only; ReadLines also accepts bare \n (identical for the Maxis files)
    auto Lines = Text::ReadLines(data);

    for (size_t i = 0; i < Lines.size(); i++)
    {
        if (Lines[i] == "") continue;
        auto Values = Text::Split(Lines[i], ',');
        while (Values.size() < 7) Values.push_back(""); // C# threw IndexOutOfRange on short lines

        EVTEntry Entry;
        Entry.Name = Text::ToLower(Values[0]);
        Entry.EventType = ParseHexString(Values[1]);
        Entry.TrackID = ParseHexString(Values[2]);
        Entry.Unknown = ParseHexString(Values[3]);
        Entry.Unknown2 = ParseHexString(Values[4]);
        Entry.Unknown3 = ParseHexString(Values[5]);
        Entry.Unknown4 = ParseHexString(Values[6]);
        Entries.push_back(Entry);
    }
}

uint32_t EVT::ParseHexString(std::string input)
{
    bool IsHex = false;
    input = Text::ToLower(Text::Trim(input));

    if (input == "") return 0;
    if (Text::StartsWith(input, "0x"))
    {
        input = input.substr(2);
        IsHex = true;
    }
    //Sigh, Maxis...
    else if (Text::Contains(input, "a") || Text::Contains(input, "b") || Text::Contains(input, "b") ||
        Text::Contains(input, "c") || Text::Contains(input, "d") || Text::Contains(input, "e") || Text::Contains(input, "f"))
    {
        IsHex = true;
    }

    try
    {
        if (IsHex) return Text::ToUInt32Hex(input);
        else return Text::ToUInt32(input);
    }
    catch (std::exception&)
    {
        return 0; // C#: FormatException (unreachable for the game's files)
    }
}

}}} // namespace FSO::Files::HIT
