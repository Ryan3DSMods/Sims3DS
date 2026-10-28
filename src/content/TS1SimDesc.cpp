/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims — Create-A-Sim data model (see CASSimDesc.h). TS1(sims): no FreeSims code behind this file.
#include "content/TS1SimDesc.h"

namespace FSO { namespace Content { namespace TS1 { namespace CAS {

const char* SkinCode(CASSkin s) {
    switch (s) {
        case CASSkin::Medium: return "med";
        case CASSkin::Dark: return "drk";
        default: return "lgt";
    }
}

int16_t SkinPersonData(CASSkin s) {
    switch (s) {
        case CASSkin::Medium: return 3;
        case CASSkin::Dark: return 2;
        default: return 1;
    }
}

int TraitPersonDataIndex(int trait) {
    static const int idx[TraitCount] = { 7, 6, 3, 5, 2 }; // Neat, Outgoing, Active, Playful, Nice
    return (trait >= 0 && trait < TraitCount) ? idx[trait] : 7;
}

// Neat, Outgoing, Active, Playful, Nice
const int ZodiacProfiles[ZodiacCount][TraitCount] = {
    { 5, 8, 6, 3, 3 },  // Aries
    { 5, 5, 3, 8, 4 },  // Taurus
    { 4, 7, 8, 3, 3 },  // Gemini
    { 6, 3, 6, 4, 6 },  // Cancer
    { 4, 10, 4, 4, 3 }, // Leo
    { 9, 2, 6, 3, 5 },  // Virgo
    { 2, 8, 2, 6, 7 },  // Libra
    { 6, 5, 8, 3, 3 },  // Scorpio
    { 2, 3, 9, 7, 4 },  // Sagittarius
    { 7, 4, 1, 8, 5 },  // Capricorn
    { 4, 4, 4, 7, 6 },  // Aquarius
    { 5, 3, 7, 3, 7 },  // Pisces
};

int CASSimDesc::PointsUsed() const {
    int n = 0;
    for (int i = 0; i < TraitCount; i++) n += Traits[i];
    return n;
}

int CASSimDesc::Zodiac() const {
    if (PointsUsed() == 0) return -1;
    int best = 0, bestD = 1 << 30;
    for (int s = 0; s < ZodiacCount; s++) {
        int d = 0;
        for (int t = 0; t < TraitCount; t++) { int x = Traits[t] - ZodiacProfiles[s][t]; d += x * x; }
        if (d < bestD) { bestD = d; best = s; }
    }
    return best;
}

std::string CASSimDesc::BodyType() const {
    if (Age == CASAge::Child) return "chd";
    // b###<g><a><type>_...  -> characters 6..8
    if (Body.Mesh.size() >= 9) {
        std::string t = Body.Mesh.substr(6, 3);
        if (t == "fat" || t == "fit" || t == "skn") return t;
    }
    return "fit";
}

std::vector<std::string> CASSimDesc::BodyStrings() const {
    // Layout of every shipped character (e.g. User00000.iff 'Bob'):
    //  0 adult|child  1 body  2 head  3 HEAD  4 Top  5 R_HAND  6 Palm  7..11 ""  12 male|female  13 27|9
    //  14 lgt|med|drk  15 naked body  16 underwear  17..22 hands  23..26 ""  27 formal  28/29 bride gloves (females)
    const bool child = Age == CASAge::Child, female = Gender == CASGender::Female;
    const std::string skin = SkinCode(Skin), type = BodyType();
    const std::string g = female ? "f" : "m";
    std::vector<std::string> s(female ? 30 : 28);
    s[0] = child ? "child" : "adult";
    s[1] = Body.Mesh + ",BODY=" + Body.Texture;
    s[2] = Head.Mesh + ",HEAD-HEAD=" + Head.Texture;
    s[3] = "HEAD"; s[4] = "Top"; s[5] = "R_HAND"; s[6] = "Palm";
    s[12] = female ? "female" : "male";
    s[13] = child ? "9" : "27";
    s[14] = skin;
    // naked body: n<g><type>_01 (skinny adults and children share the unisex nuskn_01 / nuchd_01 mesh)
    std::string nakedMesh = (child || type == "skn") ? "nu" + type + "_01" : "n" + g + type + "_01";
    std::string nakedTex = child ? "nuchd" + skin + "_01" : "n" + g + type + skin + "_01";
    s[15] = nakedMesh + ",BODY=" + nakedTex;
    // underwear: same mesh, u<g><type><skin>_briefs01 (men) / _undies01 (women, children)
    s[16] = nakedMesh + ",BODY=u" + g + type + skin + ((!child && !female) ? "_briefs01" : "_undies01");
    // hands: H<m|f|u><L|R><O|P|C>, texture hua<o|p|c><skin>
    const std::string hc = child ? "u" : g;
    static const char* hand[6] = { "LO", "RO", "LP", "RP", "LC", "RC" };
    static const char* htex[6] = { "o", "o", "p", "p", "c", "c" };
    for (int i = 0; i < 6; i++) s[17 + i] = "H" + hc + hand[i] + ",HAND=hua" + htex[i] + skin;
    // formal wear: f<g><type>_01 (children have none)
    s[27] = child ? ",BODY=" : "f" + g + type + "_01,BODY=f" + g + type + skin + "_01";
    if (female) {
        s[28] = "H" + hc + "LO,HAND=gfao_bride";
        s[29] = "H" + hc + "RO,HAND=gfao_bride";
    }
    return s;
}

std::vector<int16_t> CASSimDesc::PersonData(int16_t familyChunkID) const {
    // Values as the shipped neighbourhood stores them for CAS-made family members (Pleasant, Goth, Newbie, ...).
    std::vector<int16_t> pd(80, 0);
    for (int t = 0; t < TraitCount; t++) pd[(size_t)TraitPersonDataIndex(t)] = (int16_t)(Traits[t] * 100);
    for (int i = 0; i < 10; i++) pd[(size_t)(46 + i)] = Interests[i];
    if (Age == CASAge::Child) { pd[56] = -1; pd[57] = 4; } // job type / promotion level of the shipped children
    pd[58] = (int16_t)(Age == CASAge::Child ? 9 : 27);    // 'Person Age'
    pd[60] = SkinPersonData(Skin);                         // 'skin color'
    pd[61] = familyChunkID;                                // 'family number' (= FAMI chunk id)
    pd[65] = (int16_t)(Gender == CASGender::Female ? 1 : 0); // 'gender'
    int z = Zodiac();
    pd[70] = (int16_t)(z >= 0 ? z + 1 : 0);                // 'zodiac' TS1(sims): 1 Aries .. 12 Pisces
    return pd;
}

void CASSimDesc::InitTraits(const std::function<int(int)>& rand) {
    // BHAV 4100 'init traits' (People\TemplatePerson.iff), instruction by instruction:
    //  0-3  Local[0..2] = 3, Local[3] = 1        4  Temp[0] = 46
    //  5    Param[0] = random 0..10 (Random Number, range literal 11)
    //  6    Param[0] == 0 ?  7: Local[3] == 1 ? (10: Local[3] = 0) : retry
    //  8    Param[0] < 4 ?   11: Local[2] > 0 ? (14: Local[2] -= 1) : retry
    //  9    Param[0] < 7 ?   12: Local[1] > 0 ? (15: Local[1] -= 1) : retry
    //                        13: Local[0] > 0 ? (16: Local[0] -= 1) : retry
    //  17   MyPersonData[Temp[0]] = Param[0]   18  Temp[0] += 1   19  Temp[0] == 56 ? return true : goto 5
    int local[4] = { 3, 3, 3, 1 };
    int temp = 46;
    int guard = 0;
    while (temp != 56 && guard++ < 100000) {
        int p = rand(11);
        bool ok = false;
        if (p == 0) { if (local[3] == 1) { local[3] = 0; ok = true; } }
        else if (p < 4) { if (local[2] > 0) { local[2]--; ok = true; } }
        else if (p < 7) { if (local[1] > 0) { local[1]--; ok = true; } }
        else { if (local[0] > 0) { local[0]--; ok = true; } }
        if (!ok) continue;
        Interests[temp - 46] = (int16_t)p;
        temp++;
    }
}

int CASFamilyDesc::Adults() const {
    int n = 0;
    for (auto& m : Members) if (m.Age == CASAge::Adult) n++;
    return n;
}

}}}}
