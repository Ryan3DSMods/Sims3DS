/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// TS1(sims): The Sims 1 avatar appearance, parsed from the person's body strings
// (STR# OBJD.BodyStringID, normally #200, in UserData/Characters/User*.iff or People\*.iff).
// FreeSims only understands TSO body strings (hex outfit ids) and falls back to random TSO outfits, so the TS1
// mesh/texture names would be lost. They are kept here (VMAvatar::TS1Appearance) for the Vitaboy/body code.
//
// Body string layout (observed in the shipped characters/NPCs):
//   0 age type (adult/child/cat/dog)   1 body  "mesh,BODY=texture"   2 head "mesh,HEAD-HEAD=texture"
//   3 head bone ("HEAD")  4 "Top"  5 hand bone ("R_HAND")  6 "Palm"   7-11 random/name lists (unused by users)
//   12 gender  13 age  14 skin tone (lgt/med/drk)  15 naked body  16 underwear/swim/sleep body
//   17-22 hands: left open, right open, left pointing, right pointing, left closed, right closed ("mesh,HAND=tex")
//   27 formal body   28/29 alternate (wedding glove) hands
#pragma once
#include <array>
#include <string>
#include <vector>
#include "simantics/_files_contract.h"

namespace FSO { namespace SimAntics { namespace Model {

class VMTS1BodyPart {
public:
    std::string Mesh;     // e.g. "b002mafat_01" (bmf/skn mesh name)
    std::string Binding;  // e.g. "BODY", "HEAD-HEAD", "HAND"
    std::string Texture;  // e.g. "B002MAFatlgt_slob" (bmp texture name)
    bool Empty() const { return Mesh.empty() && Texture.empty(); }
    /// "mesh,BINDING=texture"
    static VMTS1BodyPart Parse(const std::string& str);
};

class VMTS1Appearance {
public:
    bool Valid = false;
    std::string AgeType;   // adult / child / cat / dog
    std::string Gender;    // male / female / ...
    int32_t Age = 0;
    std::string SkinTone;  // lgt / med / drk
    VMTS1BodyPart Body;
    VMTS1BodyPart Head;
    std::string HeadBone, HeadAttach, HandBone, HandAttach; // "HEAD", "Top", "R_HAND", "Palm"
    VMTS1BodyPart NakedBody;
    VMTS1BodyPart Underwear;
    VMTS1BodyPart FormalBody;
    std::array<VMTS1BodyPart, 6> Hands; // LO, RO, LP, RP, LC, RC
    std::vector<std::string> Raw;        // every body string, by index

    static VMTS1Appearance FromBodyStrings(FSO::Files::Formats::IFF::Chunks::STR* bodyStrings);
};

}}}
