/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/HIT/Track.cs
#include "files/HIT/Track.h"
#include "files/HIT/HITText.h"
#include "core/io.h"

namespace FSO { namespace Files { namespace HIT {

Track::Track(const std::vector<uint8_t>& Filedata)
{
    st::Reader Reader(Filedata, st::Endian::Little);

    MagicNumber = Reader.cstr(4, false);

    if (MagicNumber == "2DKT")
        TWODKT = true;

    int CurrentVal = 8;
    std::string data;

    if (!TWODKT)
        data = std::string(Filedata.begin() + 4, Filedata.end()); // C#: ReadChars(Filedata.Length) stops at EOF
    else
        data = Reader.cstr((size_t)Reader.s32(), false);
    auto Values = Text::Split(data, ',');
    auto V = [&](size_t i) -> std::string { return i < Values.size() ? Values[i] : std::string(); };

    //MagicNumber = Values[0];
    Version = ParseHexString(V(1));
    TrackName = V(2);
    SoundID = ParseHexString(V(3));
    TrackID = ParseHexString(V(4));
    if (V(5) != "\r\n" && V(5) != "ETKD" && V(5) != "") //some tracks terminate here...
    {
        ArgType = (HITArgs)ParseHexString(V(5));
        ControlGroup = (HITControlGroups)ParseHexString(V(7));

        if (Version == 2)
            CurrentVal++;

        CurrentVal += 2;

        DuckingPriority = (HITDuckingPriorities)ParseHexString(V(CurrentVal));
        CurrentVal++;
        Looped = ParseHexString(V(CurrentVal));
        LoopDefined = true;
        CurrentVal++;
        Volume = ParseHexString(V(CurrentVal));
    }
}

uint32_t Track::ParseHexString(std::string input)
{
    bool IsHex = false;

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
        return 0;
    }
}

}}} // namespace FSO::Files::HIT
