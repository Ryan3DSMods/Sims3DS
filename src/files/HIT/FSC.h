/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/HIT/FSC.cs
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace FSO { namespace Files { namespace HIT {

struct FSCNote
{
    uint16_t Volume = 0; //0-1024
    bool Rand = false;
    uint16_t LRPan = 0; //0-1024
    uint16_t FBPan = 0; //0-1024, front back
    bool Rand2 = false;

    uint16_t Fin = 0;
    uint16_t FOut = 0;
    uint16_t dly = 0;
    bool Rand3 = false; //what
    uint16_t Loop = 0;

    bool Loop2 = false; //might be count then decider here
    uint16_t Quant = 0; //but then what is this?
    uint16_t Prob = 0; //probably random probability, not sure of range (0-16?)
    int16_t pitchL = 0; //pitch offsets
    int16_t pitchR = 0;

    bool Fast = false;
    uint16_t GroupID = 0;
    bool Stereo = false;
    std::string Filename;
};

class FSC
{
public:
    /// <summary>
    /// FSC is a tabulated plaintext format that describes a sequence of notes to be played. In this game it is used to sequence the ambient sounds.
    /// </summary>
    std::vector<FSCNote> Notes;

    std::string VersionCode;

    uint16_t MasterVolume = 0;
    uint16_t Priority = 0;
    uint16_t Min = 0;
    uint16_t Max = 0;
    uint16_t Rows = 0; //these seem to be outright lies, but let's leave them in
    uint16_t Columns = 0;
    uint16_t Tempo = 0;
    uint16_t BPB = 0; //beats per bar
    uint16_t SelX = 0;
    uint16_t SelY = 0;
    uint16_t QuanX = 0;
    uint16_t QuanY = 0;
    uint16_t DiffX = 0;
    uint16_t DiffY = 0;

    /// <summary>
    /// Creates a new hsm file.
    /// </summary>
    /// <param name="Filedata">The data to create the hsm from.</param>
    explicit FSC(const std::vector<uint8_t>& Filedata);

    /// <summary>
    /// Creates a new hsm file.
    /// </summary>
    /// <param name="Filedata">The path to the data to create the hsm from.</param>
    explicit FSC(const std::string& Filepath);

private:
    void ReadFile(const std::vector<uint8_t>& stream);
};

}}} // namespace FSO::Files::HIT
