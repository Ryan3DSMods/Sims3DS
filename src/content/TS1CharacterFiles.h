/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// TS1(sims): The Sims 1 character files (UserData/Characters/User#####.iff), shared by Create-A-Family
// (src/app/cas/CASFamilyWriter), the engine (Make New Character: babies, clones, pets, the toad prince; Hot Date /
// Unleashed clothing changes) and the .FAM family import (TS1FamilyImport.h).
//
// FreeSims has no TS1 writer; FreeSO's SimitoneNeighbourGenerator clones TemplatePerson in memory and saves it with
// its IffFile writer. Here the files are written at byte level so every chunk the template carries is kept exactly:
//
//  User#####.iff = a clone of the template's iff (People\TemplatePerson.iff, GUID 0x7FD96B54; the Unleashed pets
//                  ExpansionPack5 TemplateDog.iff 0x4A70DF92 / TemplateCat.iff 0x7BEA0977) with
//      OBJD (every one)  GUID (bytes 28..31) = the new GUID, OriginalGUID (bytes 140..143) = the template GUID,
//                        label "user##### - <first name>"
//      STR# #200 'bodystring'  the body strings (format -3, US English)
//      CTSS #2000        first name, biography
//      BMP_ #2002..2006  portraits, when given (faces, rel. images, thumbnail, speech_med, speech_large)
//      rsmp              rebuilt;  'XXXX' (deleted) chunks dropped
#pragma once
#include <cstdint>
#include <functional>
#include <set>
#include <string>
#include <vector>

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks { class Neighbour; } } } } }
namespace FSO { namespace Content { namespace TS1 {
class TS1NeighborhoodProvider;

/// One chunk of an IFF file, raw.
struct RawIffChunk {
    std::string Type;   // 4 characters
    uint16_t ID = 0;
    uint16_t Flags = 0x10;
    std::string Label;
    std::vector<uint8_t> Data;
};

/// Byte-level IFF I/O (the shipped files' layout: 64-byte header, 76-byte chunk headers, rsmp version 0).
namespace RawIff {
    bool ReadFile(const std::string& path, std::vector<uint8_t>& out);
    /// Writes to path.tmp and renames over path.
    bool WriteFileAtomic(const std::string& path, const std::vector<uint8_t>& data);
    bool Parse(const std::vector<uint8_t>& file, std::vector<RawIffChunk>& out);
    /// Serialises chunks, dropping 'XXXX' and any old 'rsmp', and appends a rebuilt rsmp.
    std::vector<uint8_t> Build(const std::vector<RawIffChunk>& chunks);
    /// STR#/CTSS/FAMs/uChr body in TS1 format -3 (language code byte + value + comment), US English.
    std::vector<uint8_t> MakeSTRFormat3(const std::vector<std::string>& values);
    /// Values of a STR#-style chunk (formats 0, -1, -2, -3, -4), the US English (language 1 / first) entries.
    std::vector<std::string> ReadSTRValues(const std::vector<uint8_t>& data);
    RawIffChunk* Find(std::vector<RawIffChunk>& chunks, const std::string& type, uint16_t id);
}

class TS1CharacterFiles {
public:
    static const uint32_t TEMPLATE_PERSON_GUID = 0x7FD96B54; // People\TemplatePerson.iff
    static const uint32_t TEMPLATE_DOG_GUID = 0x4A70DF92;    // ExpansionPack5 TemplateDog.iff
    static const uint32_t TEMPLATE_CAT_GUID = 0x7BEA0977;    // ExpansionPack5 TemplateCat.iff

    struct NewCharacter {
        uint32_t GUID = 0;
        uint32_t TemplateGUID = TEMPLATE_PERSON_GUID;
        std::string FirstName, Bio;
        std::vector<std::string> BodyStrings;
        /// Extra chunks (e.g. BMP_ portraits), replacing template chunks of the same type and id.
        std::vector<RawIffChunk> Extra;
        /// FIX(sims) QA3: the template is one of the TemplateNPCs 'Generic' person templates, whose OBJD object type
        /// is a generator marker (HDT_Generic 512, SSCelebGeneric 2048, MMClownieGeneric 4096) instead of 2 (Person).
        bool ForcePerson = false;
    };

    /// Raw bytes of the iff holding object `guid` (an Objects.far / expansion far entry, or a loose/character
    /// file) through Content::Get()->WorldObjects. False when unknown.
    static bool LoadObjectIff(uint32_t guid, std::vector<uint8_t>& out);
    /// The character file chunks for `c`, cloned from the template's chunks (see the header comment).
    static std::vector<RawIffChunk> Clone(const std::vector<RawIffChunk>& tpl, const NewCharacter& c, const std::string& userName);
    /// Lowest free "user#####" (no neighbour of that name in `usedNames`, no Characters/User#####.iff), and its
    /// number in *number. `preferred` >= 0 is tried first (the .FAM import keeps the original user numbers).
    static std::string NextUserName(const std::string& charactersDir, const std::set<std::string>& usedNames,
                                    int preferred = -1, int* number = nullptr);
    /// "Characters/User00087.iff" for "user00087".
    static std::string FileNameFor(const std::string& userName);
    /// A random GUID not used by any object/character (WorldObjects) nor `avoid`.
    static uint32_t NewGUID(const std::function<uint32_t()>& rnd, const std::set<uint32_t>& avoid = {});

    /// Writes the character file (Characters/<FileNameFor(userName)>) and registers its OBJD with WorldObjects
    /// so the GUID creates avatars right away. Returns the path, "" on failure (error in *err).
    static std::string WriteCharacter(const std::string& userDir, const NewCharacter& c, const std::string& userName,
                                      std::string* err = nullptr);
    /// Replaces STR#200 'bodystring' of an existing character file (clothing changes, growing up).
    static bool ReplaceBodyStrings(const std::string& path, const std::vector<std::string>& bodyStrings);
    /// Adds a neighbour record (lowest free id, version 4 / person mode 5 as the shipped records) for `guid` to the
    /// neighbourhood in memory. Saved with the neighbourhood (see STUBS_expansions.md).
    static FSO::Files::Formats::IFF::Chunks::Neighbour* AddNeighbour(TS1NeighborhoodProvider* prov, uint32_t guid,
                                                                    const std::string& userName, const std::vector<int16_t>& personData);
    /// Neighbour names in use ("user00012", "maid", ...), lower case.
    static std::set<std::string> NeighbourNames(TS1NeighborhoodProvider* prov);
    /// Character file of a GUID registered with WorldObjects from a user folder ("" for Objects.far NPCs etc.).
    static std::string CharacterFileOf(uint32_t guid);
};

}}}
