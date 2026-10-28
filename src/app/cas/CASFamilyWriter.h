/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims — saves a Create-A-Family family the way The Sims does (see README.md).
//
// TS1(sims) — FreeSims has no TS1 writer for families/characters. What is written was reverse engineered
// from the shipped data (UserData/Characters/User*.iff, UserData/Neighborhood.iff, TemplatePerson.iff):
//
//  UserData/Characters/User#####.iff  (one per Sim, lowest free number)  =  a clone of
//      GameData/Objects/Objects.far  People\TemplatePerson.iff  with
//      OBJD #128 'user##### - <first name>': new random GUID (bytes 28..31), OriginalGUID (field 68/69, bytes
//               140..143) = the template's GUID 0x7FD96B54 — exactly the two fields every shipped character changes
//      STR# #200 'bodystring'  the Sim's body strings (format -3, US English)
//      BMP_ #2002 'faces' 105x41 (3 views), #2003 'rel. images' 200x40 (5 relationship colours),
//           #2004 'thumbnail' 25x25, #2005 'speech_med' 16x16, #2006 'speech_large' 32x32  (24-bit BMPs)
//      CTSS #2000  first name, biography
//      rsmp        regenerated resource map (header offset 60 points at it)
//      (deleted 'XXXX' chunks of the template are dropped)
//  UserData/Neighborhood.iff:
//      NBRS  a neighbour per Sim in the lowest free slot (neighbour id = slot + 1): version 4, person mode 5,
//            person data (CASSimDesc::PersonData), GUID, name "user#####"
//      FAMI  new chunk (lowest free id): house 0 (the family bin), family number = highest + 1, budget §20,000,
//            member GUIDs (FreeSims FAMI.Write, version 9)
//      FAMs  same id: the family name (STR# format -3)
//      every other chunk is copied byte for byte; 'XXXX' (deleted) chunks are dropped; rsmp is rebuilt.
#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include "app/Canvas.h"
#include "app/cas/CASSimDesc.h"
#include "content/TS1CharacterFiles.h"

namespace simapp { namespace cas {

/// Portraits stored in a character file (rendered by CASSimPreview::RenderPortraits).
struct CASPortraits {
    Canvas Faces;       // 105x41: three 35x41 views on (0,0,80)
    Canvas RelImages;   // 200x40: five 40x40 faces on the relationship colours
    Canvas Thumbnail;   // 25x25 on (0,0,248)
    Canvas SpeechMed;   // 16x16 on white
    Canvas SpeechLarge; // 32x32 on white
};

/// One chunk of an IFF file, raw. TS1(sims): shared with the engine (src/content/TS1CharacterFiles.h).
using CASRawChunk = FSO::Content::TS1::RawIffChunk;

struct CASSaveResult {
    bool Ok = false;
    std::string Error;
    int16_t FamilyID = -1;            // FAMI chunk id = the family number the game uses (person data 61, global 9)
    int32_t FamilyNumber = 0;         // FAMI.FamilyNumber (unique id of user families)
    std::vector<uint32_t> GUIDs;      // member GUIDs (FAMI order)
    std::vector<int16_t> NeighbourIDs;
    std::vector<std::string> Files;   // character files written
};

class CASFamilyWriter {
public:
    /// userDataDir: the neighbourhood folder (…/UserData or …/UserData2); objectsFar: …/GameData/Objects/Objects.far
    CASFamilyWriter(const std::string& userDataDir, const std::string& objectsFar);

    /// GUIDs already in use elsewhere (e.g. the WorldObjects registry); optional.
    std::function<bool(uint32_t)> GUIDInUse;
    /// Random source (0..n-1); defaults to rand().
    std::function<uint32_t()> Random32;

    /// Writes the character files and adds the family to Neighborhood.iff. portraits[i] may be empty canvases.
    CASSaveResult Save(const CASFamilyDesc& family, const std::vector<CASPortraits>& portraits);

    /// Moves a family from the bin into a house (FAMI.HouseNumber), as the neighbourhood's move-in does.
    static bool MoveIn(const std::string& userDataDir, int16_t familyID, int16_t houseNumber, std::string* error = nullptr);

    // ---- raw IFF helpers ----
    static bool ReadIff(const std::vector<uint8_t>& file, std::vector<CASRawChunk>& out);
    /// Serialises chunks, dropping 'XXXX' and any old 'rsmp', and appends a rebuilt rsmp (version 0).
    static std::vector<uint8_t> WriteIff(const std::vector<CASRawChunk>& chunks);
    /// STR#/CTSS/FAMs body in TS1 format -3 (language code byte + C strings), US English.
    static std::vector<uint8_t> MakeSTRFormat3(const std::vector<std::string>& values);
    /// 24-bit Windows BMP (BMP_ chunk body).
    static std::vector<uint8_t> MakeBMP24(const Canvas& img);

    static bool ReadFile(const std::string& path, std::vector<uint8_t>& out);
    static bool WriteFileAtomic(const std::string& path, const std::vector<uint8_t>& data);

private:
    std::string _UserData, _ObjectsFar;
};

}}
