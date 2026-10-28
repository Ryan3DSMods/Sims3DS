/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims — Create-A-Sim skin catalogue (see CASCatalog.h). TS1(sims).
#include "content/TS1SkinCatalog.h"
#include "vitaboy/content/TS1AvatarProvider.h"
#include <algorithm>
#include <cctype>
#include <cstring>

namespace FSO { namespace Content { namespace TS1 { namespace CAS {

using FSO::Content::TS1::TS1AvatarContent;

static bool EndsWith(const std::string& s, const char* suf) {
    size_t n = strlen(suf);
    return s.size() >= n && s.compare(s.size() - n, n, suf) == 0;
}
static bool Digits3(const std::string& s, size_t at) {
    return s.size() >= at + 3 && isdigit((unsigned char)s[at]) && isdigit((unsigned char)s[at + 1]) && isdigit((unsigned char)s[at + 2]);
}
static int SkinOf(const std::string& s) {
    if (s == "lgt") return 0;
    if (s == "med") return 1;
    if (s == "drk") return 2;
    return -1;
}

namespace {
struct Parsed {
    std::string Prefix; // "c010ma" / "b002mafat" (mesh family)
    std::string Name;   // texture suffix after '_'
    char Gender = 0, Age = 0;
    int Skin = -1;
};
// c###<g><a><skin>_<name>
bool ParseHead(const std::string& tex, Parsed& p) {
    if (tex.size() < 11 || tex[0] != 'c' || !Digits3(tex, 1)) return false;
    char g = tex[4], a = tex[5];
    if ((g != 'm' && g != 'f' && g != 'u') || (a != 'a' && a != 'c')) return false;
    int skin = SkinOf(tex.substr(6, 3));
    if (skin < 0 || tex[9] != '_') return false;
    p.Prefix = tex.substr(0, 6); p.Name = tex.substr(10); p.Gender = g; p.Age = a; p.Skin = skin;
    return true;
}
// b###<g><a><type><skin>_<name>
bool ParseBody(const std::string& tex, Parsed& p) {
    if (tex.size() < 14 || tex[0] != 'b' || !Digits3(tex, 1)) return false;
    char g = tex[4], a = tex[5];
    if ((g != 'm' && g != 'f' && g != 'u') || (a != 'a' && a != 'c')) return false;
    std::string type = tex.substr(6, 3);
    if (type != "fat" && type != "fit" && type != "skn" && type != "chd") return false;
    if ((a == 'c') != (type == "chd")) return false;
    int skin = SkinOf(tex.substr(9, 3));
    if (skin < 0 || tex[12] != '_') return false;
    p.Prefix = tex.substr(0, 9); p.Name = tex.substr(13); p.Gender = g; p.Age = a; p.Skin = skin;
    return true;
}
// texture "c010malgt_baldbeard01" -> mesh among "c010ma_*": the one whose name the texture name starts with
// (c010ma_baldbeard), else the only/first one (c003fa_longhair for C003FAmed_asian01, as Bella Goth has it).
std::string PickMesh(const std::vector<std::string>& meshes, const std::string& prefix, const std::string& name) {
    std::string best;
    size_t bestLen = 0;
    for (auto& m : meshes) {
        std::string mn = m.substr(prefix.size() + 1);
        if (!mn.empty() && name.compare(0, mn.size(), mn) == 0 && mn.size() > bestLen) { best = m; bestLen = mn.size(); }
    }
    if (!best.empty()) return best;
    for (auto& m : meshes) if (m.size() >= prefix.size() + 3 && m.compare(prefix.size() + 1, 2, "01") == 0) return m;
    return meshes.empty() ? std::string() : meshes.front();
}
}

bool CASCatalog::Build() {
    for (auto& v : _Heads) v.clear();
    for (auto& v : _Bodies) v.clear();
    auto content = TS1AvatarContent::Get();
    if (!content->EnsureInit()) return false;
    auto& prov = content->TS1Global;

    // mesh families: suit names from Animation.ndx / loose CMX / BCF file names ("<suit>.cmx.bcf")
    std::map<std::string, std::vector<std::string>> meshes; // prefix -> suits
    std::set<std::string> suits;
    for (auto& kv : content->BCF.SkinHostBCF) suits.insert(kv.first);
    for (auto& f : prov.ListExtension(".bcf")) if (EndsWith(f, ".cmx.bcf")) suits.insert(f.substr(0, f.size() - 8));
    for (auto& f : prov.ListExtension(".cmx")) suits.insert(f.substr(0, f.size() - 4));
    for (auto& s : suits) {
        auto us = s.find('_');
        if (us == std::string::npos || s.size() < 5 || !Digits3(s, 1)) continue;
        if (s[0] == 'c' || s[0] == 'b') meshes[s.substr(0, us)].push_back(s);
    }
    for (auto& kv : meshes) std::sort(kv.second.begin(), kv.second.end());

    for (auto& file : prov.ListExtension(".bmp")) {
        std::string tex = file.substr(0, file.size() - 4);
        Parsed p;
        bool head = ParseHead(tex, p);
        if (!head && !ParseBody(tex, p)) continue;
        // outfits the game puts on by itself, not CAS choices: House Party costume-trunk costumes (b7##..._ct) and
        // the Vacation winter / swimming head variants (c5##.._winter#, .._swim#) that go with W/S-prefixed bodies
        if (p.Name == "ct" || p.Name.compare(0, 6, "winter") == 0 || p.Name.compare(0, 4, "swim") == 0) continue;
        auto it = meshes.find(p.Prefix);
        if (it == meshes.end()) continue;
        CASOutfit o;
        o.Mesh = PickMesh(it->second, p.Prefix, p.Name);
        o.Texture = tex;
        if (o.Mesh.empty()) continue;
        CASAge age = p.Age == 'c' ? CASAge::Child : CASAge::Adult;
        for (int g = 0; g < 2; g++) {
            char gc = g == 0 ? 'm' : 'f';
            if (p.Gender != gc && p.Gender != 'u') continue;
            auto& list = head ? _Heads[Key(age, (CASGender)g, (CASSkin)p.Skin)] : _Bodies[Key(age, (CASGender)g, (CASSkin)p.Skin)];
            list.push_back(o);
        }
    }
    for (auto& v : _Heads) std::sort(v.begin(), v.end(), [](const CASOutfit& a, const CASOutfit& b) { return a.Texture < b.Texture; });
    for (auto& v : _Bodies) std::sort(v.begin(), v.end(), [](const CASOutfit& a, const CASOutfit& b) { return a.Texture < b.Texture; });
    _Built = HeadCount() > 0 && BodyCount() > 0;
    return _Built;
}

int CASCatalog::Find(const std::vector<CASOutfit>& list, const CASOutfit& o) {
    for (size_t i = 0; i < list.size(); i++) if (list[i].Texture == o.Texture) return (int)i;
    return -1;
}

// "c001malgt_ross" and "c001madrk_ross": same 6/9 character prefix and same name after the skin code
static std::string StyleKey(const std::string& tex) {
    if (tex.empty()) return tex;
    size_t skinAt = tex[0] == 'c' ? 6 : 9;
    if (tex.size() < skinAt + 4) return tex;
    return tex.substr(0, skinAt) + tex.substr(skinAt + 3);
}

int CASCatalog::FindSameStyle(const std::vector<CASOutfit>& list, const CASOutfit& o) {
    int exact = Find(list, o);
    if (exact >= 0) return exact;
    auto key = StyleKey(o.Texture);
    for (size_t i = 0; i < list.size(); i++) if (StyleKey(list[i].Texture) == key) return (int)i;
    // same number (e.g. c001ma), any name
    size_t n = o.Texture.empty() ? 0 : (o.Texture[0] == 'c' ? 6 : 9);
    for (size_t i = 0; i < list.size() && n; i++) if (list[i].Texture.compare(0, n, o.Texture, 0, n) == 0) return (int)i;
    return -1;
}

void CASCatalog::MarkBroken(const CASOutfit& o) {
    for (auto* lists : { _Heads, _Bodies })
        for (int k = 0; k < 12; k++) {
            auto& v = lists[k];
            v.erase(std::remove(v.begin(), v.end(), o), v.end());
        }
}

size_t CASCatalog::HeadCount() const { size_t n = 0; for (auto& v : _Heads) n += v.size(); return n; }
size_t CASCatalog::BodyCount() const { size_t n = 0; for (auto& v : _Bodies) n += v.size(); return n; }

}}}}
