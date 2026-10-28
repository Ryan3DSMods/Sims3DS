/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims — Create-A-Family / Create-A-Sim: the data the player designs (see src/app/cas/README.md).
// TS1(sims): moved from src/app/cas/CASSimDesc.h so the engine (Make New Character, .FAM import) can share it;
// app/cas/CASSimDesc.h forwards these names into simapp::cas.
//
// TS1(sims): FreeSims has no TS1 CAS (its PersonSelectionEdit.cs is the TSO avatar creator: gender, skin
// tone, head/body collections, name, description). What a TS1 Sim *is* on disk was taken from the game's own data:
//   * the body strings (STR# 200 'bodystring' of every shipped UserData/Characters/User*.iff),
//   * the person data layout (GameData/Behavior.iff STR# 200 'Person Data Labels') and the values the shipped
//     neighbourhood stores for CAS-made Sims (UserData/Neighborhood.iff NBRS),
//   * the character template (GameData/Objects/Objects.far People\TemplatePerson.iff) and its 'init traits'
//     tree (BHAV 4100), translated below as CASSimDesc::InitTraits.
#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace FSO { namespace Content { namespace TS1 { namespace CAS {

enum class CASAge : int { Adult = 0, Child = 1 };
enum class CASGender : int { Male = 0, Female = 1 };
enum class CASSkin : int { Light = 0, Medium = 1, Dark = 2 }; // FreeSims AppearanceType Light/Medium/Dark

/// "lgt" / "med" / "drk" (the skin tone code in TS1 texture names and body string 14).
const char* SkinCode(CASSkin s);
/// Person data 60 'skin color': lgt 1, drk 2, med 3 (from the shipped NBRS records).
int16_t SkinPersonData(CASSkin s);

/// The five TS1 personality traits in CAS order (UIText.iff STR# 130 'DesignCharStrs' 17..21).
enum CASTrait : int { TraitNeat = 0, TraitOutgoing, TraitActive, TraitPlayful, TraitNice, TraitCount };
static const int PersonalityPoints = 25; // points to spend in CAS
static const int TraitMaxPoints = 10;    // LEDs per trait
/// Person data index of a trait (Behavior.iff STR#200): Neat 7, Outgoing 6, Active 3, Playful 5, Nice 2.
int TraitPersonDataIndex(int trait);

/// Zodiac signs, UIText.iff STR# 164 'signs' 0..11 (Aries..Pisces).
static const int ZodiacCount = 12;
/// Personality profile of each sign (Neat, Outgoing, Active, Playful, Nice; 25 points each).
extern const int ZodiacProfiles[ZodiacCount][TraitCount];

/// A head or body skin: the mesh (suit name in a BCF/CMX) and its texture (BMP, no extension), lower case.
struct CASOutfit {
    std::string Mesh;    // "c010ma_baldbeard" / "b002mafat_01"
    std::string Texture; // "c010malgt_baldbeard01" / "b002mafatlgt_slob"
    bool Empty() const { return Mesh.empty(); }
    bool operator==(const CASOutfit& o) const { return Mesh == o.Mesh && Texture == o.Texture; }
    bool operator!=(const CASOutfit& o) const { return !(*this == o); }
};

/// One Sim being designed.
struct CASSimDesc {
    std::string FirstName;
    std::string Bio;
    CASAge Age = CASAge::Adult;
    CASGender Gender = CASGender::Female;
    CASSkin Skin = CASSkin::Light;
    CASOutfit Head, Body;
    int Traits[TraitCount] = { 0, 0, 0, 0, 0 }; // 0..10 points each
    int16_t Interests[10] = { 0 };               // person data 46..55 (set by InitTraits)

    int PointsUsed() const;
    int PointsLeft() const { return PersonalityPoints - PointsUsed(); }
    /// Sign whose profile is closest to the traits (least squared difference), -1 while no point is spent.
    /// TS1(sims): the game shows the sign the personality matches; the profiles are the shared Sims table.
    int Zodiac() const;
    /// "fat" / "fit" / "skn" / "chd" — the body type of the chosen body mesh (b###MAfit_01 -> "fit").
    std::string BodyType() const;
    /// STR# 200 'bodystring' exactly as the shipped characters have it (28 strings, 30 for females).
    std::vector<std::string> BodyStrings() const;
    /// The 80 person data entries the neighbour record (NBRS, version 4) stores for this Sim.
    std::vector<int16_t> PersonData(int16_t familyChunkID) const;

    /// Translation of TemplatePerson.iff BHAV 4100 'init traits' (the tree the game runs on a new Sim): ten
    /// interests (person data 46..55), random 0..10, with exactly one 0, three in 1..3, three in 4..6 and three in
    /// 7..10. rand(n) must return 0..n-1 (the VM's Random Number primitive).
    void InitTraits(const std::function<int(int)>& rand);
};

/// The family being designed (Create A Family).
struct CASFamilyDesc {
    static const int MaxMembers = 8;          // TS1: up to 8 Sims per family
    static const int32_t StartingFunds = 20000; // TS1: every new family starts with §20,000
    std::string LastName;
    std::vector<CASSimDesc> Members;
    int32_t Budget = StartingFunds;

    int Adults() const;
    /// Done is allowed when the family has a name and at least one adult (children can't live alone).
    bool CanFinish() const { return !LastName.empty() && Adults() > 0; }
};

}}}}
