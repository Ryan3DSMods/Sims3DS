/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/HIT/FSC.cs
#include "files/HIT/FSC.h"
#include "files/HIT/HITText.h"

namespace FSO { namespace Files { namespace HIT {

FSC::FSC(const std::vector<uint8_t>& Filedata)
{
    ReadFile(Filedata);
}

FSC::FSC(const std::string& Filepath)
{
    ReadFile(Text::ReadAllBytes(Filepath));
}

void FSC::ReadFile(const std::vector<uint8_t>& stream)
{
    auto lines = Text::ReadLines(stream);
    size_t li = 0;
    auto ReadLine = [&]() -> std::string { return li < lines.size() ? lines[li++] : std::string(); };
    auto U16 = [](const std::string& s) -> uint16_t { int32_t v = 0; Text::TryParseInt(s, v); return (uint16_t)v; };
    auto S16 = [](const std::string& s) -> int16_t { int32_t v = 0; Text::TryParseInt(s, v); return (int16_t)v; };

    Notes.clear();
    VersionCode = ReadLine();
    auto line = ReadLine();

    while (Text::StartsWith(line, "#") && li < lines.size())
        line = ReadLine();

    //Header
    auto Head = Text::Split(line, '\t');
    while (Head.size() < 15) Head.push_back("0");
    MasterVolume = U16(Head[1]);
    Priority = U16(Head[2]);
    Min = U16(Head[3]);
    Max = U16(Head[4]);
    Rows = U16(Head[5]);
    Columns = U16(Head[6]);
    Tempo = U16(Head[7]);
    BPB = U16(Head[8]);
    SelX = U16(Head[9]);
    SelY = U16(Head[10]);
    QuanX = U16(Head[11]);
    QuanY = U16(Head[12]);
    DiffX = U16(Head[13]);
    DiffY = U16(Head[14]);

    line = ReadLine();

    while ((Text::StartsWith(line, "#") || Text::StartsWith(line, "cells")) && li < lines.size())
        line = ReadLine();

    while (li < lines.size()) //read notes
    {
        auto line2 = ReadLine();
        auto Values = Text::Split(line2, '\t');
        if (!Text::StartsWith(line, "#") && Values.size() == 20)
        {
            FSCNote n;
            n.Volume = U16(Values[1]);
            n.Rand = Values[2] != "0";
            n.LRPan = U16(Values[3]);
            n.FBPan = U16(Values[4]);
            n.Rand2 = Values[5] != "0";

            n.Fin = U16(Values[6]);
            n.FOut = U16(Values[7]);
            n.dly = U16(Values[8]);
            n.Rand3 = Values[9] != "0";
            n.Loop = U16(Values[10]);

            n.Loop2 = Values[11] != "0";
            n.Quant = U16(Values[12]);
            n.Prob = U16(Values[13]);
            n.pitchL = S16(Values[14]);
            n.pitchR = S16(Values[15]);

            n.Fast = Values[16] != "0";
            n.GroupID = U16(Values[17]);
            n.Stereo = Values[18] != "0";
            n.Filename = Values[19];
            Notes.push_back(n);
        }
    }
}

}}} // namespace FSO::Files::HIT
