/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// TS1(sims): The Sims 1 family files (*.FAM) - FreeSims/FreeSO have no reader.
//
// A .FAM (TemplateFamilyUnleashed/*.FAM, and what the neighbourhood's "export family" writes) is an IFF holding:
//   FAMI #n / FAMs #n / FAMh #n   the family (n = its family number in the neighbourhood it came from), name, history
//   NBRS #1                       that whole neighbourhood; the members are the records whose GUID is in FAMI
//   uChr #id                      a member's body strings (STR# format -3); id = the member's neighbour id there
//   CTSS #id, CTSS #2000+id       name + biography ("Filename: .\userdata\characters\user00091"), all languages
//   BMP_ #id                      the member's 25x25 thumbnail
//   CTSS #1000                    the family name in every language;  EXPi  the expansion packs it needs
//   SIMI/HOUS/ARRY/OBJM/objt/...  the family's house (FAMI.HouseNumber, 0 = no house: Strays_4000, the family bin)
//
// Import (what the Unleashed installer does for the five Old Town families and the strays, and the neighbourhood's
// "import family"): for each member a character file Characters/User#####.iff (cloned from TemplatePerson /
// TemplateCat / TemplateDog: content/TS1CharacterFiles) with the member's body strings, CTSS and thumbnail, keeping
// the member's GUID (the Old Town houses' OBJM refers to them) and user number when they are free; a neighbour record
// (person data, relationships among the imported Sims and with neighbours of the same GUID); a FAMI/FAMs (the old
// family number when free) living in FAMI.HouseNumber when that house is free (else the family bin, house 0); and,
// unless the neighbourhood folder already has its own HouseNN.iff, the .FAM's house as HouseNN.iff (the
// TemplateCommunity copies of houses 56/63/69/76/77 were saved with other GUIDs for these Sims).
//
// Front-end API:
//   TS1FamilyImport::Read(path, fam)                 - preview (name, members' names/thumbnails, house, needs EP)
//   TS1FamilyImport::Import(userDir, fam, opts, &r)  - writes Characters/, Neighborhood.iff (+ the house)
//   TS1FamilyImport::InstallTemplates(userDir, root) - every TemplateFamilyUnleashed/*.FAM not yet imported
//                                                      (by GUID), as the installer does; idempotent.
// Content::Neighborhood must have this userDir loaded (it supplies the template iffs and is updated: call
// Neighborhood->InitSpecific + LoadCharacters afterwards, as after Create-A-Family).
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "content/TS1CharacterFiles.h"

namespace FSO { namespace Content { namespace TS1 {

struct TS1FamFile {
    std::string Path;
    std::string Name;                 // FAMs 0
    int16_t FamilyID = 0;             // FAMI chunk id
    int32_t HouseNumber = 0;
    int32_t Budget = 0;
    std::vector<uint32_t> GUIDs;
    std::vector<uint8_t> FAMIData;    // raw FAMI body (patched on import)
    struct Member {
        uint32_t GUID = 0;
        int16_t OldNeighbourID = 0;
        std::string UserName;         // "user00091" (the NBRS record name)
        std::string FirstName, Bio;
        std::vector<std::string> BodyStrings;
        RawIffChunk CTSS;             // multi-language name/bio (CTSS #2000+id or #id)
        RawIffChunk Thumbnail;        // BMP_ #id (empty Type if none)
        int32_t PersonMode = 5;
        std::vector<int16_t> PersonData;
        std::vector<std::pair<int32_t, std::vector<int16_t>>> Relationships; // keyed by the old neighbour ids
        bool Pet() const { return !BodyStrings.empty() && (BodyStrings[0] == "dog" || BodyStrings[0] == "kat" || BodyStrings[0] == "cat"); }
    };
    std::vector<Member> Members;
    std::vector<std::pair<int16_t, uint32_t>> OldNeighbourGUIDs; // every NBRS record of the source neighbourhood
    std::vector<RawIffChunk> HouseChunks; // the house (empty when HouseNumber == 0)
    std::vector<RawIffChunk> FamilyChunks; // FAMs, FAMh, CTSS #1000
};

class TS1FamilyImport {
public:
    struct Options {
        bool MoveIntoHouse = true;     // live in FAMI.HouseNumber if that lot is free
        bool WriteHouse = true;        // write the .FAM's house when the neighbourhood has no such house file
    };
    struct Result {
        bool Ok = false;
        std::string Error;
        int16_t FamilyID = -1;
        int32_t HouseNumber = 0;
        std::vector<uint32_t> GUIDs;
        std::vector<int16_t> NeighbourIDs;
        std::vector<std::string> Files;
        bool Skipped = false;           // InstallTemplates: already imported
    };
    static bool Read(const std::string& path, TS1FamFile& out, std::string* err = nullptr);
    static Result Import(const std::string& userDir, const TS1FamFile& fam, const Options& opts);
    static std::vector<Result> InstallTemplates(const std::string& userDir, const std::string& installRoot);
};

}}}
