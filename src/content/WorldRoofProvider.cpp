/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.Content.WorldRoofProvider (FreeSO tso.content/WorldRoofProvider.cs, TS1 branch):
//   new FileProvider<ITextureRef>(..., new Regex("GameData/Roofs/.*\\.bmp")), NameToID = Items.FindIndex(name).
// TS1 houses store the roof by texture name (HOUS.RoofName + ".bmp", lowercased by VMWorldActivator).
#include "content/ContentManager.h"
#include <algorithm>
#if !defined(__3DS__) || defined(ST_HAVE_DIRENT)
#include <dirent.h>
#define ST_ROOF_DIRENT 1
#endif

namespace FSO { namespace Content {

void WorldRoofProvider::Scan() {
    if (Scanned) return;
    Scanned = true;
    auto c = Content::Get();
    std::string dir = c->SimsCompleteDir + "/GameData/Roofs";
    std::vector<std::pair<std::string, std::string>> found; // lowercase name, real name
#ifdef ST_ROOF_DIRENT
    if (DIR* d = opendir(dir.c_str())) {
        while (dirent* e = readdir(d)) {
            std::string n = e->d_name;
            auto l = ToLowerInvariant(n);
            if (l.size() > 4 && l.substr(l.size() - 4) == ".bmp") found.emplace_back(l, n);
        }
        closedir(d);
    }
#endif
    if (found.empty()) {
        // no directory listing: the roofs shipped with The Sims Complete Collection
        static const char* known[] = { "gravel1.bmp", "r1_.bmp", "r2_.bmp", "r3_.bmp", "r4_.bmp", "tar1.bmp",
            "MMGypsyTentRoof.bmp", "MMOldWood1Roof.bmp", "MMOldWood2Roof.bmp", "MMSuburban1Roof.bmp", "MMSuburban2Roof.bmp",
            "MMTent1Roof.bmp", "MMTent2Roof.bmp", "SuperstarDirtyRoof.bmp", "SuperstarFashionCopperRoof.bmp",
            "SuperstarFashionRoof.bmp", "SuperstarGoldRoof.bmp", "SuperstarTentRoof.bmp" };
        for (auto k : known) if (FileExists(dir + "/" + k)) found.emplace_back(ToLowerInvariant(k), k);
    }
    std::sort(found.begin(), found.end());
    for (auto& f : found) {
        Names[f.first] = (int32_t)Items.size();
        Items.push_back(f.first);
        Paths.push_back(dir + "/" + f.second);
    }
}

int32_t WorldRoofProvider::NameToID(const std::string& name) {
    Scan();
    auto it = Names.find(ToLowerInvariant(name));
    return it == Names.end() ? 0 : it->second;
}

std::string WorldRoofProvider::IDToName(int32_t id) {
    Scan();
    return (id >= 0 && (size_t)id < Items.size()) ? Items[(size_t)id] : std::string();
}

std::string WorldRoofProvider::GetPath(int32_t id) {
    Scan();
    if (Paths.empty()) return std::string();
    if (id < 0 || (size_t)id >= Paths.size()) id = 0;
    return Paths[(size_t)id];
}

int32_t WorldRoofProvider::Count() { Scan(); return (int32_t)Items.size(); }

}}
