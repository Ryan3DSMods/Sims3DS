#include <cstdio>
#include <cctype>
/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// TS1(sims): see TS1Clothing.h (FreeSO TS1BCFProvider collections, VMTS1PurchasableOutfitHelper,
// VMSuitProvider.GetPersonSuitTS1 and VMTS1MakeNewCharacter's head/body choice, translated).
#include "content/TS1Clothing.h"
#include "vitaboy/content/TS1AvatarProvider.h"
#include "content/ContentManager.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <set>

namespace FSO { namespace Content { namespace TS1 {

const char* TS1Clothing::OutfitTypes[6] = { "b", "f", "s", "l", "w", "h" };
const int16_t TS1Clothing::OutfitTypeToInd[6] = { 1, 30, 31, 32, 33, 34 };

static std::string Lower(std::string s) { for (auto& c : s) c = (char)tolower((unsigned char)c); return s; }
static bool EndsWith(const std::string& s, const char* suf) { size_t n = strlen(suf); return s.size() >= n && s.compare(s.size() - n, n, suf) == 0; }
static bool StartsWith(const std::string& s, const std::string& p) { return s.size() >= p.size() && s.compare(0, p.size(), p) == 0; }
static std::string BS(const std::vector<std::string>& v, size_t i) { return i < v.size() ? v[i] : std::string(); }
static bool IsValid(const std::string& s) { return !(s.empty() || s == "ADDED" || s == "NONE" || s[0] == ','); }

TS1Clothing& TS1Clothing::Get() { static TS1Clothing c; return c; }

bool TS1Clothing::EnsureBuilt() {
    if (_Built) return !Collections.empty();
    auto content = TS1AvatarContent::Get();
    // the avatar content's install folder is set lazily (ContentManager VitaboyContent); set it here too
    if (!content->Initialized()) {
        auto cm = FSO::Content::Content::Get();
        if (cm != nullptr && !cm->SimsCompleteDir.empty()) TS1AvatarContent::SetBasePath(cm->SimsCompleteDir);
    }
    if (!content->EnsureInit()) return false; // not set up yet: try again later
    _Built = true;
    std::set<std::string> suits;
    for (auto& kv : content->BCF.SkinHostBCF) suits.insert(Lower(kv.first));
    for (auto& f : content->TS1Global.ListExtension(".bcf")) if (EndsWith(f, ".cmx.bcf")) suits.insert(f.substr(0, f.size() - 8));
    for (auto& f : content->TS1Global.ListExtension(".cmx")) suits.insert(f.substr(0, f.size() - 4));
    auto add = [&](const std::string& cat, const std::string& type, const std::string& item) {
        auto& v = Collections[cat][type];
        if (std::find(v.begin(), v.end(), item) == v.end()) v.push_back(item);
    };
    for (auto& item : suits) {
        if (item.size() < 5 || !isdigit((unsigned char)item[1]) || !isdigit((unsigned char)item[2]) || !isdigit((unsigned char)item[3])) continue;
        auto uindex = item.find('_');
        if (uindex == std::string::npos) uindex = item.size();
        if (uindex < 4) continue;
        auto type = item.substr(4, uindex - 4);
        if (EndsWith(type, "lgt") || EndsWith(type, "med") || EndsWith(type, "drk")) type = type.substr(0, type.size() - 3);
        add(std::string(1, item[0]), type, item);
        if (item[0] == 'b' && (EndsWith(type, "fat") || EndsWith(type, "fit") || EndsWith(type, "skn")))
            add(std::string(1, item[0]), type.substr(0, type.size() - 3), item);
    }
    for (auto& c : Collections) for (auto& t : c.second) std::sort(t.second.begin(), t.second.end());
    for (auto& f : content->TS1Global.ListExtension(".bmp")) _Textures.push_back(f.substr(0, f.size() - 4));
    std::sort(_Textures.begin(), _Textures.end());
    return !Collections.empty();
}

const std::vector<std::string>& TS1Clothing::TextureNames() { EnsureBuilt(); return _Textures; }

const std::vector<std::string>* TS1Clothing::Suits(const std::string& collection, const std::string& avatarType) const {
    auto c = Collections.find(collection);
    if (c == Collections.end()) return nullptr;
    auto t = c->second.find(avatarType);
    return t == c->second.end() ? nullptr : &t->second;
}

std::string TS1Clothing::FirstTexture(const std::string& prefix) {
    auto it = std::lower_bound(_Textures.begin(), _Textures.end(), prefix);
    if (it != _Textures.end() && StartsWith(*it, prefix)) return *it;
    return "";
}

// FreeSO ExtractID / FindHG
static std::string UpToUnderscore(const std::string& s) { auto i = s.find('_'); return i == std::string::npos ? s : s.substr(0, i); }
static std::string FromUnderscore(const std::string& s) { auto i = s.find('_'); return i == std::string::npos ? s : s.substr(i); }

static bool PetKind(const std::vector<std::string>& bodyStrings, const std::string& petType, std::string& simtype) {
    if (!petType.empty()) { simtype = petType; return true; }
    auto age = Lower(BS(bodyStrings, 0));
    if (age == "cat" || age == "kat") { simtype = "cat"; return true; }
    if (age == "dog") { simtype = "dog"; return true; }
    return false;
}

std::vector<TS1Clothing::Outfit> TS1Clothing::ValidOutfits(const std::vector<std::string>& bodyStrings, int16_t outfitType, const std::string& petType) {
    std::vector<Outfit> result;
    if (!EnsureBuilt()) return result;
    std::string skin, collection, simtype;
    if (outfitType < 0) { collection = "b"; simtype = (outfitType == -1) ? "dog" : "cat"; }
    else if (PetKind(bodyStrings, petType, simtype)) collection = "b";
    else {
        if (outfitType > 5) return result;
        collection = OutfitTypes[outfitType];
        auto body = Lower(BS(bodyStrings, 1));
        if (body.size() < 5) return result;
        simtype = body.substr(4);
        simtype = simtype.substr(0, simtype.find('_'));
        skin = Lower(BS(bodyStrings, 14));
    }
    auto bodies = Suits(collection, simtype);
    if (bodies == nullptr) return result;
    for (auto& b : *bodies) {
        auto tex = FirstTexture(UpToUnderscore(b) + skin);
        if (tex.empty()) continue;
        std::string hg = "huao" + FromUnderscore(b);
        std::string hand = std::binary_search(_Textures.begin(), _Textures.end(), hg) ? hg.substr(4) : skin;
        result.push_back({ b + ",BODY=" + tex, hand });
    }
    return result;
}

int16_t TS1Clothing::SuitIndex(const std::vector<std::string>& bodyStrings, int16_t outfitType, const std::string& petType) {
    if (outfitType < 0 || outfitType > 5) return -1;
    auto valid = ValidOutfits(bodyStrings, outfitType, petType);
    std::string petT;
    int16_t bsInd = PetKind(bodyStrings, petType, petT) ? 1 : OutfitTypeToInd[outfitType];
    auto cur = Lower(BS(bodyStrings, (size_t)bsInd));
    cur = cur.substr(0, cur.find(';'));
    for (size_t i = 0; i < valid.size(); i++) if (Lower(valid[i].BodyString) == cur) return (int16_t)i;
    return -1;
}

// FreeSO TilEQ, as intended: keep "HmLO,HAND=" and replace the texture.
static std::string TilEQ(const std::string& item) {
    auto ind = item.find('=');
    return ind == std::string::npos ? item + "=" : item.substr(0, ind + 1);
}

int16_t TS1Clothing::SetSuit(std::vector<std::string>& bodyStrings, int16_t outfitType, int16_t newIndex, const std::string& petType) {
    if (outfitType < 0 || outfitType > 5) return -2;
    auto valid = ValidOutfits(bodyStrings, outfitType, petType);
    if (newIndex < 0 || newIndex >= (int16_t)valid.size()) return -2;
    auto prev = SuitIndex(bodyStrings, outfitType, petType);
    std::string petT;
    bool pet = PetKind(bodyStrings, petType, petT);
    size_t bsInd = pet ? 1 : (size_t)OutfitTypeToInd[outfitType];
    if (bodyStrings.size() <= bsInd) bodyStrings.resize(bsInd + 1);
    auto& nsuit = valid[(size_t)newIndex];
    bodyStrings[bsInd] = nsuit.BodyString;
    if (outfitType == 0 && !pet && !nsuit.HandTexSuffix.empty() && bodyStrings.size() > 22) {
        //right now only replace the handgroup when changing the default outfit.
        static const char* kind[6] = { "huao", "huao", "huap", "huap", "huac", "huac" };
        for (int i = 0; i < 6; i++) if (!bodyStrings[17 + i].empty()) bodyStrings[17 + i] = TilEQ(bodyStrings[17 + i]) + kind[i] + nsuit.HandTexSuffix;
    }
    return prev;
}

bool TS1Clothing::RandomHeadBody(char gender, bool child, const std::string& skin, const std::function<int(int)>& rand,
                                 std::string& head, std::string& body, std::string& handSuffix) {
    if (!EnsureBuilt()) return false;
    // FreeSO VMTS1MakeNewCharacter: simtype "<m|f><c|m>" for heads, + "chd" for children's bodies; adults' bodies are
    // listed under "<g>a" (the b collection's build-less type). FreeSO used "<g>m" for adults, which no skin has.
    std::string htype = std::string(1, gender) + (child ? "c" : "a");
    std::string btype = child ? htype + "chd" : htype;
    std::vector<std::pair<std::string, std::string>> heads, bodies;
    std::vector<std::string> hands;
    if (auto hs = Suits("c", htype)) for (auto& h : *hs) { auto t = FirstTexture(UpToUnderscore(h) + skin); if (!t.empty()) heads.push_back({ h, t }); }
    if (auto bs = Suits("b", btype)) for (auto& b : *bs) {
        auto t = FirstTexture(UpToUnderscore(b) + skin);
        if (t.empty()) continue;
        // only the everyday bodies (not the costume-trunk / uniform outfits the game puts on by itself)
        if (EndsWith(t, "_ct")) continue;
        bodies.push_back({ b, t });
        std::string hg = "huao" + FromUnderscore(b);
        hands.push_back(std::binary_search(_Textures.begin(), _Textures.end(), hg) ? hg.substr(4) : skin);
    }
    if (heads.empty() || bodies.empty()) return false;
    auto& h = heads[(size_t)rand((int)heads.size())];
    size_t bi = (size_t)rand((int)bodies.size());
    head = h.first + ",HEAD-HEAD=" + h.second;
    body = bodies[bi].first + ",BODY=" + bodies[bi].second;
    handSuffix = hands[bi];
    return true;
}

// FIX(sims) QA2: House Party costume trunk outfits. The trunk asks for person suits 8 toga, 9 country, 10 luau, 11 rave,
// 12 costume, 13 formal ("nice"), 14 swimsuit, 16 disco (traced from its 'Change into Outfit' tree); no character file
// carries body strings for them, so nothing changed on screen. The game's meshes / textures are
// ExpansionPack2/Skins ct-<set>-<fafit|mafit|fc|mc>-NN (10 adult variants, 2 for children) and
// ct-<set>-<FAFit|MAFit|FC|MC><Lgt|Med|Drk>-NN; the variant is fixed per Sim (from its everyday body string), the
// original's choice rule is not known.
static std::string CostumeTrunkBody(uint16_t id, bool male, bool child, const std::string& skin, const std::string& daywear) {
    const char* set = nullptr;
    switch (id) {
        case 8: set = "toga"; break; case 9: set = "cow"; break; case 10: set = "luau"; break; case 11: set = "rave"; break;
        case 12: set = "cost"; break; case 13: set = "nice"; break; case 14: set = "swim"; break; case 16: set = "disco"; break;
        default: return "";
    }
    std::string sk = skin.empty() ? std::string("med") : skin;
    sk[0] = (char)toupper((unsigned char)sk[0]);
    unsigned h = 2166136261u;
    for (char c : daywear) h = (h ^ (unsigned char)tolower((unsigned char)c)) * 16777619u;
    int n = child ? 1 + (int)(h % 2) : 1 + (int)(h % 10);
    char num[8]; snprintf(num, sizeof num, "%02d", n);
    std::string ga = child ? std::string(male ? "mc" : "fc") : std::string(male ? "mafit" : "fafit");
    std::string GA = child ? std::string(male ? "MC" : "FC") : std::string(male ? "MAFit" : "FAFit");
    return std::string("ct-") + set + "-" + ga + "-" + num + ",BODY=ct-" + set + "-" + GA + sk + "-" + num;
}

std::string TS1PersonSuitBodyString(const std::vector<std::string>& bs, uint16_t id, bool male, bool child,
                                    const std::string& jobMaleMesh, const std::string& jobFemaleMesh, const std::string& jobSkin) {
    auto skn = Lower(BS(bs, 14));
    std::string code = "fit";
    auto b1 = Lower(BS(bs, 1));
    b1 = b1.substr(0, b1.find(',')); // TS1(sims): the mesh only (FreeSO also matched "Fatale" in the texture)
    if (b1.find("fat") != std::string::npos) code = "fat";
    else if (b1.find("skn") != std::string::npos) code = "skn";
    if (child) code = "uchd";
    else code = std::string(male ? "m" : "f") + code;

    switch (id) {
        case 0: return BS(bs, 1); // DefaultDaywear
        case 1: // Naked. TS1(sims): the character's own naked body (string 15) first.
            if (IsValid(BS(bs, 15))) return BS(bs, 15);
            return "n" + code + "_01,BODY=n" + code + skn + "_01";
        case 2: // DefaultSwimwear
            if (IsValid(BS(bs, 31))) return BS(bs, 31);
            // TS1(sims): the base game's swimwear is the character's underwear (string 16, "..._briefs01")
            if (IsValid(BS(bs, 16))) return BS(bs, 16);
            return "n" + code + "_01,BODY=u" + code + skn + "_" + ((male && !child) ? "briefs" : "undies") + "01";
        case 3: { // JobOutfit
            auto mesh = jobMaleMesh;
            if (mesh.empty()) return BS(bs, 1);
            if (!male && !jobFemaleMesh.empty()) mesh = jobFemaleMesh;
            auto rep = [&](std::string s) {
                s = Lower(s);
                auto r = [&](const std::string& from, const std::string& to) {
                    size_t p = 0;
                    while ((p = s.find(from, p)) != std::string::npos) { s.replace(p, from.size(), to); p += to.size(); }
                };
                r("$g", code.substr(0, 1)); r("$b", code.substr(1)); r("$c", skn);
                return s;
            };
            return rep(mesh) + ",BODY=" + rep(jobSkin);
        }
        case 4: // TS1Formal
            if (IsValid(BS(bs, 30))) return BS(bs, 30);
            if (IsValid(BS(bs, 27))) return BS(bs, 27); // TS1(sims): the character's formal wear (string 27)
            if (child) return "";                       // children have no formal wear (",BODY=")
            return "f" + code + "_01,BODY=f" + code + skn + "_01";
        case 5: { // DefaultSleepwear
            if (IsValid(BS(bs, 32))) return BS(bs, 32);
            std::string pj = child ? "pjs" : "pajama";
            std::string gen = child ? std::string(male ? "m" : "f") + "chd" : code;
            return pj + gen + "_01,BODY=" + pj + (male ? "m" : "f") + skn + "_01";
        }
        case 6: return std::string(child ? "skeletonchd_01" : "skeleton_01") + ",BODY=skeleton_01";  // SkeletonPlus
        case 7: return std::string(child ? "skeletonchd_01" : "skeleton_01") + ",BODY=skeleneg_01";  // SkeletonMinus
        case 8: case 9: case 10: case 11: case 12: // toga, country, luau, rave, costume (FreeSO: person strings 30)
        case 13: // TS1ExpandedFormal
            if (IsValid(BS(bs, 30))) return BS(bs, 30);
            return CostumeTrunkBody(id, male, child, skn, BS(bs, 1));
        case 14: // TS1ExpandedSwimsuit
            if (IsValid(BS(bs, 31))) return BS(bs, 31);
            return CostumeTrunkBody(id, male, child, skn, BS(bs, 1));
        case 15: return IsValid(BS(bs, 32)) ? BS(bs, 32) : std::string();  // TS1ExpandedPajamas
        case 16: // House Party "Disco" (costume trunk)
            if (IsValid(BS(bs, 33))) return BS(bs, 33);
            return CostumeTrunkBody(id, male, child, skn, BS(bs, 1));
        case 17: return IsValid(BS(bs, 33)) ? BS(bs, 33) : std::string(); // TS1Winter
        case 18: return IsValid(BS(bs, 34)) ? BS(bs, 34) : std::string();  // TS1HighFashion
        default: return "";
    }
}

}}}
