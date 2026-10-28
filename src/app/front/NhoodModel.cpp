/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims front end — see NhoodModel.h. TS1(sims): new code on top of FreeSims' TS1NeighborhoodProvider.
#include "files/SafeFile.h" // FIX(sims) SAVE
#include "files/LoadProgress.h"
#include <set>
#include "files/SafeFile.h" // FIX(sims) SAVE
#include "files/LoadProgress.h"
#include "content/TS1FamilyImport.h"
#include <cstdio>
#include "app/front/NhoodModel.h"
#include "app/front/FrontStrings.h"
#include "app/front/ResGraphics.h"
#include "app/Font.h"
#include "content/ContentManager.h"
#include "content/TS1NeighbourProvider.h"
#include "files/formats/iff/IffFile.h"
#include "files/formats/iff/chunks/BMP.h"
#include "files/formats/iff/chunks/CTSS.h"
#include "files/formats/iff/chunks/FAMI.h"
#include "files/formats/iff/chunks/FAMs.h"
#include "content/TS1NeighbourhoodWriter.h"
#include "files/formats/iff/chunks/NBRS.h"
#include "files/formats/iff/chunks/OBJD.h"
#include "files/formats/iff/chunks/SIMI.h"
#include "files/formats/iff/chunks/STR.h"
#include "core/io.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace simapp { namespace front {

using namespace FSO::Files::Formats::IFF;
using namespace FSO::Files::Formats::IFF::Chunks;
using FSO::Content::TS1::TS1NeighborhoodProvider;

static TS1NeighborhoodProvider* Prov() {
    auto c = FSO::Content::Content::Get();
    return c ? c->Neighborhood.get() : nullptr;
}

// ======================================================================= areas

const AreaDef& GetAreaDef(Area a, bool unleashed) {
    // Res_Nbhd.RT: kNbhdBck_UL + kNghFrameDelta1..6_UL (Unleashed, US), kNbhdBck + kNghFrameDelta1..3 (1.0),
    // kDowntownBackground + kDowntownWater0..5, kVacationIsland (+ port/trees) + kVIWater0..4,
    // kStudiotownBackground (+ top layer), kMagiclandBackground + kMagictownWater0..5.
    static const AreaDef base = { Area::Neighbourhood, "Nbhd/NScreen.BMP", {}, {},
        { "Nbhd/DiffN1-N2_8.bmp", "Nbhd/DiffN1-N3_8.bmp", "Nbhd/DiffN1-N4_8.bmp", "Nbhd/DiffN1-N3_8.bmp" }, 300,
        1, 1, 20, "Nhood", "Nbhd/Bus_loadscreen_800x600.bmp", nullptr, nullptr, 0 };
    static const AreaDef ul = { Area::Neighbourhood, "Community/NScreen_unleashed.bmp", {}, {},
        { "Community/NScreen_unleashed_waves001.bmp", "Community/NScreen_unleashed_waves002.bmp", "Community/NScreen_unleashed_waves003.bmp",
          "Community/NScreen_unleashed_waves004.bmp", "Community/NScreen_unleashed_waves005.bmp", "Community/NScreen_unleashed_waves006.bmp" }, 220,
        4, 1, 80, "Nhood", "Community/Bus_loadscreen_800x600.bmp", nullptr, nullptr, 0 };
    static const AreaDef dt = { Area::Downtown, "Downtown/DScreen.bmp", {}, {},
        { "Downtown/dscreen00.bmp", "Downtown/dscreen01.bmp", "Downtown/dscreen02.bmp", "Downtown/dscreen03.bmp", "Downtown/dscreen04.bmp" }, 260,
        2, 21, 30, "dt_nhood", "Downtown/Taxi_loadscreen_800x600.bmp", "NghUI/Downtown.bmp", "Downtown/Return.bmp", 7 };
    static const AreaDef vi = { Area::Vacation, "VIsland/visland.bmp", { "VIsland/visland_port.bmp" }, { "VIsland/visland_trees.bmp" },
        { "VIsland/visland_waves001.bmp", "VIsland/visland_waves002.bmp", "VIsland/visland_waves003.bmp", "VIsland/visland_waves004.bmp", "VIsland/visland_waves005.bmp" }, 260,
        3, 40, 49, "Vacation", "VIsland/vacation_loadscreen_800x600.bmp", "NghUI/Vacation.bmp", "NghUI/Return.bmp", 9 };
    static const AreaDef st = { Area::StudioTown, "Studiotown/DScreen.bmp", {}, { "Studiotown/DScreen_top_layer.bmp" }, {}, 0,
        5, 81, 89, "Superstar", "Studiotown/Studiotown_loadscreen_800x600.bmp", "NghUI/Studiotown.bmp", "Studiotown/Return.bmp", 11 };
    static const AreaDef mt = { Area::MagicTown, "Magicland/DScreen.bmp", {}, {},
        { "Magicland/DScreen_waves1.bmp", "Magicland/DScreen_waves2.bmp", "Magicland/DScreen_waves3.bmp", "Magicland/DScreen_waves4.bmp",
          "Magicland/DScreen_waves5.bmp", "Magicland/DScreen_waves6.bmp" }, 240,
        7, 90, 99, "MakinMagic", "Magicland/magicland_loadscreen_800x600.bmp", "NghUI/MagicLand.bmp", "Magicland/Return.bmp", 12 };
    switch (a) {
        case Area::Downtown: return dt;
        case Area::Vacation: return vi;
        case Area::StudioTown: return st;
        case Area::MagicTown: return mt;
        default: return unleashed ? ul : base;
    }
}

// ======================================================================= small IFF helpers

/// Reads chunks of the wanted types straight from the file (no full parse): (type, id) -> data.
static std::map<std::pair<std::string, int>, std::vector<uint8_t>> ReadChunks(const std::string& path, const std::vector<std::pair<std::string, int>>& wanted) {
    std::map<std::pair<std::string, int>, std::vector<uint8_t>> out;
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return out;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    long p = 64;
    while (p + 76 <= size) {
        uint8_t h[76];
        fseek(f, p, SEEK_SET);
        if (fread(h, 1, 76, f) != 76) break;
        std::string type((const char*)h, 4);
        uint32_t csize = ((uint32_t)h[4] << 24) | ((uint32_t)h[5] << 16) | ((uint32_t)h[6] << 8) | h[7];
        int id = (int16_t)((h[8] << 8) | h[9]);
        if (csize < 76 || p + (long)csize > size) break;
        for (auto& w : wanted) {
            if (w.first == type && (w.second == id || w.second == -99999)) {
                std::vector<uint8_t> d(csize - 76);
                if (!d.empty() && fread(d.data(), 1, d.size(), f) != d.size()) d.clear();
                out[std::make_pair(type, id)] = std::move(d);
                break;
            }
        }
        p += csize;
    }
    fclose(f);
    return out;
}

static std::unique_ptr<IffFile> OpenIff(const std::string& path) {
    if (!UserDataSetup::FileExists(path)) return nullptr;
    try { return std::unique_ptr<IffFile>(new IffFile(path)); } catch (...) { return nullptr; }
}

static std::vector<std::string> SplitComma(const std::string& s) {
    std::vector<std::string> r;
    size_t a = 0;
    while (true) {
        size_t c = s.find(',', a);
        std::string t = s.substr(a, c == std::string::npos ? std::string::npos : c - a);
        while (!t.empty() && (t[0] == ' ' || t[0] == '\t')) t.erase(0, 1);
        while (!t.empty() && (t.back() == ' ' || t.back() == '\r' || t.back() == '\t')) t.pop_back();
        r.push_back(t);
        if (c == std::string::npos) break;
        a = c + 1;
    }
    return r;
}

// ======================================================================= neighbourhood

bool NhoodModel::Switch(int index, const UserDataSetup& setup, std::string& error) {
    auto content = FSO::Content::Content::Get();
    auto prov = Prov();
    if (content == nullptr || prov == nullptr) { error = "The game data is not loaded."; return false; }
    BrokenIndex = -1;
    // FIX(sims) SAVE: repair interrupted saves before anything is unloaded (src/files/SafeFile.h). Each folder's
    // leftovers once per run (three listings, ~0.1 s on the 3DS); Neighborhood.iff itself is checked every time.
    {
        std::string dir = setup.SaveRoot() + "/" + UserDataSetup::FolderName(index);
        if (!UserDataSetup::DirExists(dir)) dir = setup.SaveRoot() + "/UserData"; // as TS1NeighborhoodProvider::InitSpecific
        static std::set<std::string> scanned;
        if (scanned.insert(dir).second) {
            int n = FSO::Files::SafeFile::RecoverDir(dir) + FSO::Files::SafeFile::RecoverDir(dir + "/Houses") +
                    FSO::Files::SafeFile::RecoverDir(dir + "/Characters");
            if (n > 0) FSO::Files::LoadProgress::Step("front: restored " + std::to_string(n) + " file(s) of an interrupted save in " + dir);
        }
        auto res = FSO::Files::SafeFile::Recover(dir + "/Neighborhood.iff", FSO::Files::SafeFile::IffValidator("NBRS"));
        if (res == FSO::Files::SafeFile::Result::Missing || res == FSO::Files::SafeFile::Result::Damaged) {
            BrokenIndex = index;
            setup.MarkBroken(index);
            error = "Neighborhood " + std::to_string(index + 1) + " could not be loaded: its save file (" + dir + "/Neighborhood.iff) is " +
                    (res == FSO::Files::SafeFile::Result::Missing ? "missing" : "damaged") + " and there is no backup of it.";
            FSO::Files::LoadProgress::Step("ERROR: " + error);
            return false;
        }
    }
    // the previous neighbourhood's characters (User*.iff GUIDs repeat between neighbourhoods)
    auto world = content->WorldObjects.get();
    if (world != nullptr && !prov->CharacterFiles.empty()) {
        std::vector<uint64_t> remove;
        for (auto& kv : world->Entries)
            for (auto& path : prov->CharacterFiles) if (kv.second.FileName == path) { remove.push_back(kv.first); break; }
        for (auto guid : remove) world->RemoveObject((uint32_t)guid);
    }
    prov->UserDataRoot = setup.SaveRoot();
    prov->InitSpecific(index);
    if (prov->MainResource == nullptr) {
        error = "Could not open " + prov->UserPath + "/Neighborhood.iff";
        // FIX(sims) SAVE: keep the neighbourhood that was loaded (was: everything unloaded -> an empty neighbourhood)
        if (Current >= 0 && Current != index) {
            prov->InitSpecific(Current);
            if (prov->MainResource != nullptr) { prov->LoadCharacters(); if (world != nullptr) world->RebuildIndexes(); RefreshFamilies(); }
        }
        return false;
    }
    prov->LoadCharacters();
    // FIX(sims): the Unleashed installer puts the Old Town families (TemplateFamilyUnleashed/*.FAM) into every
    // neighbourhood; this was only done by the simep tool, so Old Town 56/63/69/76/77 had no families and 65 missing
    // objects. Done once per neighbourhood folder (marker file), idempotent by GUID.
    {
        std::string marker = prov->UserPath + "/fam_templates.txt";
        std::string famDir = setup.DataRoot() + "/TemplateFamilyUnleashed";
        if (!UserDataSetup::FileExists(marker) && UserDataSetup::DirExists(famDir)) {
            // a save area made by an older version holds untouched TemplateCommunity copies of these houses: replace them
            for (auto& f : UserDataSetup::ListFiles(famDir)) {
                FSO::Content::TS1::TS1FamFile fam;
                if (!FSO::Content::TS1::TS1FamilyImport::Read(famDir + "/" + f, fam) || fam.HouseNumber <= 0) continue;
                char n[32]; snprintf(n, sizeof n, "/Houses/House%02d.iff", (int)fam.HouseNumber);
                std::string mine = prov->UserPath + n, tpl = setup.DataRoot() + "/TemplateCommunity" + n;
                std::vector<uint8_t> a, b;
                auto readAll = [](const std::string& p, std::vector<uint8_t>& out) {
                    FILE* fp = fopen(p.c_str(), "rb"); if (!fp) return false;
                    uint8_t buf[16384]; size_t k; while ((k = fread(buf, 1, sizeof buf, fp)) > 0) out.insert(out.end(), buf, buf + k);
                    fclose(fp); return true; };
                if (readAll(mine, a) && readAll(tpl, b) && a == b) remove(mine.c_str());
            }
            auto rs = FSO::Content::TS1::TS1FamilyImport::InstallTemplates(prov->UserPath, setup.DataRoot());
            int ok = 0;
            for (auto& r : rs) if (r.Ok) ok++;
            FSO::Files::SafeFile::WriteText(marker, "Unleashed template families installed: " + std::to_string(ok) + " of " + std::to_string(rs.size()) + "\n"); // FIX(sims) SAVE
            printf("[nhood] Unleashed template families: %d of %zu installed into %s\n", ok, rs.size(), prov->UserPath.c_str());
            if (ok > 0) {
                prov->InitSpecific(index);
                if (prov->MainResource == nullptr) { error = "Could not open " + prov->UserPath + "/Neighborhood.iff"; return false; }
                prov->LoadCharacters();
                if (world != nullptr) world->RebuildIndexes();
            }
        }
    }
    Current = index;
    UserDir = prov->UserPath;
    _DeletedFamilies.clear();
    Dirty = false;
    RefreshFamilies();
    LoadDescriptions();
    return true;
}

void NhoodModel::RefreshFamilies() {
    Families.clear();
    auto prov = Prov();
    auto content = FSO::Content::Content::Get();
    if (prov == nullptr || prov->MainResource == nullptr) return;
    for (auto fami : prov->ListFamilies()) {
        if (fami->ChunkID == 0 || fami->FamilyNumber < 0) continue; // the townie "Default" family
        FamilyInfo f;
        f.ChunkID = (int16_t)fami->ChunkID;
        f.Name = ToLatin1(prov->GetFamilyName(f.ChunkID));
        f.House = fami->HouseNumber;
        f.Budget = fami->Budget;
        f.NetWorth = fami->NetWorth;
        f.Friends = fami->FamilyFriends;
        for (auto g : fami->FamilyGUIDs) {
            SimInfo s;
            s.GUID = g;
            int16_t nid = 0;
            auto nb = prov->GetNeighborIDForGUID(g, nid) ? prov->GetNeighborByID(nid) : nullptr;
            s.NeighbourID = nid;
            if (nb) s.Name = nb->Name;
            auto obj = content->WorldObjects->Get(g, true);
            if (obj != nullptr && obj->Resource != nullptr && obj->OBJ != nullptr) {
                auto ctss = obj->Resource->Get<CTSS>(obj->OBJ->CatalogStringsID);
                if (ctss != nullptr && !ctss->GetString(0).empty()) s.Name = ctss->GetString(0);
                if (ctss != nullptr && ctss->Length() > 1) s.Bio = ToLatin1(ctss->GetString(1));
                // STR# 200 'bodystring': [0] adult/child, [12] male/female (character iffs)
                // portrait: BMP_ 2002 'faces' = normal / highlighted / selected, side by side; else the 25x25 thumbnail
                if (auto faces = obj->Resource->Get<BMP>(2002)) {
                    Canvas all;
                    if (DecodeImage(faces->GetData(), all, true) && all.Width >= 3) Crop(all, s.Face, 0, 0, all.Width / 3, all.Height);
                } else if (auto th = obj->Resource->Get<BMP>(2004)) DecodeImage(th->GetData(), s.Face, true);
                auto body = obj->Resource->Get<STR>(200);
                if (body != nullptr) {
                    s.Child = body->GetString(0) == "child";
                    s.Female = body->GetString(12) == "female";
                    auto kind = body->GetString(0);
                    s.Pet = kind == "dog" || kind == "cat" || kind == "kat"; // FIX(sims) FB1
                }
            }
            s.Name = ToLatin1(s.Name);
            size_t sp = s.Name.find(' ');
            s.FirstName = sp == std::string::npos ? s.Name : s.Name.substr(0, sp);
            f.Members.push_back(s);
        }
        Families.push_back(f);
    }
}

const FamilyInfo* NhoodModel::Family(int16_t id) const {
    for (auto& f : Families) if (f.ChunkID == id) return &f;
    return nullptr;
}

const FamilyInfo* NhoodModel::FamilyInHouse(int house) const {
    if (house <= 0) return nullptr;
    for (auto& f : Families) if (f.House == house) return &f;
    return nullptr;
}

std::vector<const FamilyInfo*> NhoodModel::Bin() const {
    std::vector<const FamilyInfo*> r;
    // FIX(sims) FB1: the Unleashed "Strays" (TemplateFamilyUnleashed/Strays_4000.FAM, family 4000: eight stray dogs and
    // cats, no person) is the stray-pet NPC family of the Old Town adoption centre, not a playable household; the
    // original family bin never lists it and a household needs at least one person. It was the only family in the bin
    // of neighbourhood 2 and every new neighbourhood, and moving it in left a lot with no controllable Sim.
    for (auto& f : Families) {
        if (f.House != 0 || f.Members.empty()) continue;
        bool person = false;
        for (auto& m : f.Members) if (!m.Pet) person = true;
        if (person) r.push_back(&f);
    }
    return r;
}

void NhoodModel::LoadDescriptions() {
    _Desc.clear();
    // TS1NeighborhoodProvider.GetHouseNameDesc reads NeighborhoodDesc/STDesc/MTDesc (STR# 2000+house: name, desc);
    // Downtown (DTDesc) and Vacation (VIDesc) work the same way.
    static const char* files[] = { "NeighborhoodDesc.iff", "DTDesc.iff", "VIDesc.iff", "STDesc.iff", "MTDesc.iff" };
    for (auto f : files) {
        auto iff = OpenIff(UserDir + "/Houses/" + f);
        if (!iff) continue;
        for (auto str : iff->List<STR>()) {
            int house = (int)str->ChunkID - 2000;
            if (house < 0 || house > 99 || str->Length() < 1) continue;
            std::string name = ToLatin1(str->GetString(0)), desc = str->Length() > 1 ? ToLatin1(str->GetString(1)) : "";
            while (!name.empty() && (name.back() == ' ' || name.back() == ':')) name.pop_back();
            if (name.empty() && desc.empty()) continue;
            if (!_Desc.count(house) || _Desc[house].first.empty()) _Desc[house] = std::make_pair(name, desc);
        }
    }
}

void NhoodModel::LoadPositions(int table) {
    _Positions.clear();
    std::unique_ptr<IffFile> iff = OpenIff(UserDir + "/LotLocations.iff");
    auto content = FSO::Content::Content::Get();
    if (!iff || iff->Get<STR>((uint16_t)table) == nullptr) iff = OpenIff(content->SimsCompleteDir + "/GameData/LotLocations.iff");
    if (!iff) return;
    auto str = iff->Get<STR>((uint16_t)table);
    if (!str) return;
    for (int i = 0; i < str->Length(); i++) {
        auto s = SplitComma(str->GetString(i));
        if (s.size() < 3 || s[0].empty()) continue;
        int h = atoi(s[0].c_str());
        if (!_Positions.count(h)) _Positions[h] = std::make_pair(atoi(s[1].c_str()), atoi(s[2].c_str()));
    }
}

std::string NhoodModel::StreetAddress(const std::string& userDir, int house) {
    auto& S = FrontStrings::Get();
    auto iff = OpenIff(userDir + "/StreetNames.iff");
    std::string fmt;
    if (iff) {
        auto names = iff->Get<STR>(2000), assign = iff->Get<STR>(2001);
        if (names && assign && house >= 1 && house <= assign->Length()) {
            int street = atoi(assign->GetString(house - 1).c_str());
            if (street >= 1 && street <= names->Length()) fmt = ToLatin1(names->GetString(street - 1));
        }
    }
    if (fmt.empty()) {
        // NghRollover street formats: Sim Lane / Wright Way (downtown) / Old Volcano Way (vacation) /
        // Studio Town Drive / Calamity Forest
        if (house >= 21 && house <= 30) fmt = S.Get(134, 13, "%s Wright Way");
        else if (house >= 40 && house <= 49) fmt = S.Get(134, 15, "%s Old Volcano Way");
        else if (house >= 81 && house <= 89) fmt = S.Get(134, 25, "%s Studio Town Drive");
        else if (house >= 90) fmt = S.Get(134, 27, "%s Calamity Forest");
        else fmt = S.Get(134, 7, "%s Sim Lane");
    }
    return Format(fmt, { std::to_string(house) });
}

bool NhoodModel::HousePicture(const std::string& userDir, int house, Canvas& out) {
    char n[32]; snprintf(n, sizeof n, "/Houses/House%02d.iff", house);
    auto chunks = ReadChunks(userDir + n, { { "BMP_", 512 } });
    auto it = chunks.find(std::make_pair(std::string("BMP_"), 512));
    return it != chunks.end() && DecodeImage(it->second, out);
}

bool NhoodModel::OpenPicture(const LotInfo& lot, Canvas& out) const {
    // FIX(sims) FB1: BMP_ 512 (no roof), the picture the game fades in over the hovered house
    char n[32]; snprintf(n, sizeof n, "/Houses/House%02d.iff", lot.House);
    auto chunks = ReadChunks(UserDir + n, { { "BMP_", 512 }, { "BMP_", 1024 } });
    Canvas full;
    auto it = chunks.find(std::make_pair(std::string("BMP_"), 512));
    if (it == chunks.end() || !DecodeImage(it->second, full)) return false;
    if (lot.Scale <= 1) { out = std::move(full); return true; }
    auto h = chunks.find(std::make_pair(std::string("BMP_"), 1024));
    if (h != chunks.end() && DecodeImage(h->second, out) && std::abs(out.Width - full.Width / 2) <= 2) return true;
    Resample(full, out, std::max(1, full.Width / 2), std::max(1, full.Height / 2));
    return true;
}

bool NhoodModel::Unleashed() const {
    return UserDataSetup::FileExists(UserDir + "/Houses/House50.iff");
}

bool NhoodModel::AreaAvailable(Area a) const {
    const AreaDef& d = GetAreaDef(a, Unleashed());
    if (a == Area::Neighbourhood) return true;
    for (int h = d.FirstHouse; h <= d.LastHouse; h++) {
        char n[32]; snprintf(n, sizeof n, "/Houses/House%02d.iff", h);
        if (UserDataSetup::FileExists(UserDir + n)) return ResGraphics::Get().Exists(d.Background) || !ResGraphics::Get().IsOpen();
    }
    return false;
}

LotInfo* NhoodModel::Lot(int house) {
    for (auto& l : Lots) if (l.House == house) return &l;
    return nullptr;
}

void NhoodModel::LoadArea(Area a) {
    CurrentArea = a;
    Lots.clear();
    auto prov = Prov();
    const AreaDef& def = GetAreaDef(a, Unleashed());
    LoadPositions(def.LotTable);
    for (auto& kv : _Positions) {
        FSO::Files::LoadProgress::Progress("front: neighbourhood map", 0, 0); // ADDED(sims): ~40 ms per lot on the 3DS
        int h = kv.first;
        if (a != Area::Neighbourhood && (h < def.FirstHouse || h > def.LastHouse)) continue;
        LotInfo lot;
        lot.House = h;
        lot.CX = kv.second.first; lot.CY = kv.second.second;
        char n[32]; snprintf(n, sizeof n, "/Houses/House%02d.iff", h);
        std::string path = UserDir + n;
        lot.Exists = UserDataSetup::FileExists(path);
        if (!lot.Exists) continue;
        lot.Community = prov != nullptr && prov->GetZoning((int16_t)h) == 1;
        if (a == Area::Downtown || a == Area::Vacation || a == Area::StudioTown) lot.Community = true;
        if (a == Area::MagicTown) lot.Community = prov == nullptr || prov->GetZoning((int16_t)h) != 0;
        auto d = _Desc.find(h);
        if (d != _Desc.end()) { lot.Name = d->second.first; lot.Desc = d->second.second; }
        lot.Address = StreetAddress(UserDir, h);
        auto fam = FamilyInHouse(h);
        lot.Family = fam ? fam->ChunkID : -1;
        // FIX(sims) FB1: the house picture as the game places it (FreeSO UINeighborhoodHouseButton, which matches the
        // original): BMP_ 513 (roof on) at 1:1 on the 1.0 neighbourhood and the EP towns, at half size on the Unleashed
        // neighbourhood (its own half-size copy BMP_ 1025 when the file has one), centred on the LotLocations point and
        // moved by THMB 512 (base y offset, x offset) / (2 x scale). Before, the point was taken as the top-left of the
        // half-size open picture (BMP_ 1024) on every map: the EP towns' houses were drawn at half size, off their plots
        // (over the roads), and houses saved by the port showed BMP_ 512, which it wrote as an unkeyed night rectangle.
        lot.Scale = (a == Area::Neighbourhood && Unleashed()) ? 2 : 1;
        auto chunks = ReadChunks(path, { { "BMP_", 513 }, { "BMP_", 512 }, { "BMP_", 1025 }, { "THMB", 512 }, { "SIMI", 1 } });
        auto bmp = [&](int id) { auto it = chunks.find(std::make_pair(std::string("BMP_"), id)); return it == chunks.end() ? (const std::vector<uint8_t>*)nullptr : &it->second; };
        Canvas full;
        int fullW = 0, fullH = 0;
        for (int id : { 513, 512 }) if (auto b = bmp(id)) if (DecodeImage(*b, full)) { fullW = full.Width; fullH = full.Height; break; }
        if (lot.Scale == 1) lot.Thumb = std::move(full);
        else {
            Canvas half;
            if (auto b = bmp(1025)) if (DecodeImage(*b, half) && fullW > 0 && std::abs(half.Width - fullW / 2) <= 2) lot.Thumb = std::move(half);
            if (lot.Thumb.Width == 0 && fullW > 0) Resample(full, lot.Thumb, std::max(1, fullW / 2), std::max(1, fullH / 2));
        }
        int baseY = 0, offX = 0, addY = 0;
        auto th = chunks.find(std::make_pair(std::string("THMB"), 512));
        if (th != chunks.end() && th->second.size() >= 20) {
            auto le = [&](size_t o) { const uint8_t* q = th->second.data() + o; return (int)(int32_t)((uint32_t)q[0] | ((uint32_t)q[1] << 8) | ((uint32_t)q[2] << 16) | ((uint32_t)q[3] << 24)); };
            baseY = le(8); offX = le(12); addY = le(16);
        }
        int sc = lot.Scale;
        lot.X = lot.CX - (fullW > 0 ? fullW : lot.Thumb.Width * sc) / (2 * sc) + offX / (2 * sc);
        lot.Y = lot.CY - (fullH > 0 ? fullH : lot.Thumb.Height * sc) / (2 * sc) + baseY / (2 * sc);
        lot.OpenDY = -addY / sc;
        auto si = chunks.find(std::make_pair(std::string("SIMI"), 1));
        if (si != chunks.end() && !si->second.empty()) {
            SIMI simi;
            try { st::Reader r(si->second); simi.Read(nullptr, r); } catch (...) {}
            lot.Value = simi.PurchaseValue();
            lot.EmptyLot = simi.ArchitectureValue == 0;
        }
        Lots.push_back(std::move(lot));
    }
    // draw order: back to front (FIX(sims) FB1: by the lot's centre, as FreeSO orders the house buttons)
    std::stable_sort(Lots.begin(), Lots.end(), [](const LotInfo& x, const LotInfo& y) { return x.CY < y.CY; });
}

std::string NhoodModel::Rollover(const LotInfo& lot, const FamilyInfo* moving) const {
    auto& S = FrontStrings::Get();
    if (lot.Family >= 0) {
        auto f = Family(lot.Family);
        if (f) return Format(S.Get(134, 0, "The %s Family\nNet Worth: %s\nFriends: %d"), { f->Name, Simoleons(std::max(f->NetWorth, f->Budget + lot.Value)), std::to_string(f->Friends) });
    }
    std::string name = lot.Name.empty() ? lot.Address : lot.Name;
    std::string desc = lot.Desc.empty() ? (lot.Name.empty() ? S.Get(134, 23, "No Description") : lot.Address) : lot.Desc;
    if (lot.Community) {
        if (CurrentArea == Area::Neighbourhood || CurrentArea == Area::MagicTown) return Format(S.Get(134, 17, "%s\n%s\nThis is a community lot."), { name, desc });
        return Format(S.Get(134, 21, "%s\n%s\nThis is a Community lot. Please click here to visit."), { name, desc });
    }
    std::string cost = Simoleons(lot.Value);
    if (moving == nullptr) return Format(S.Get(134, 1, "%s\n%s\nCost: %s\nPick or create a family and move them in to play."), { name, desc, cost });
    if (moving->Budget < lot.Value)
        return Format(S.Get(134, lot.EmptyLot ? 4 : 2, "%s\n%s\nCost: %s\nThe %s family only has %s.  They can't afford this house."), { name, desc, cost, moving->Name, Simoleons(moving->Budget) });
    return Format(S.Get(134, lot.EmptyLot ? 5 : 3, "%s\n%s\nCost: %s\nThis could be the house of your dreams!  If you buy, the family will have %s to spend on household items."),
                  { name, desc, cost, Simoleons(moving->Budget - lot.Value) });
}

// ======================================================================= actions

NhoodModel::MoveResult NhoodModel::CanMoveIn(int16_t family, int house) const {
    auto f = Family(family);
    auto prov = Prov();
    if (f == nullptr || prov == nullptr) return MoveResult::Missing;
    { bool person = false; for (auto& m : f->Members) if (!m.Pet) person = true; if (!person) return MoveResult::Missing; } // FIX(sims) FB1
    if (FamilyInHouse(house) != nullptr) return MoveResult::Occupied;
    if (prov->GetZoning((int16_t)house) == 1 || (house >= 21 && house <= 49) || (house >= 81 && house <= 89)) return MoveResult::Community;
    int value = 0;
    for (auto& l : Lots) if (l.House == house) value = l.Value;
    if (f->Budget < value) return MoveResult::CantAfford;
    return MoveResult::Ok;
}

NhoodModel::MoveResult NhoodModel::MoveIn(int16_t family, int house) {
    auto r = CanMoveIn(family, house);
    if (r != MoveResult::Ok) return r;
    auto prov = Prov();
    auto fami = prov->GetFamily(family);
    if (fami == nullptr) return MoveResult::Missing;
    int value = 0;
    for (auto& l : Lots) if (l.House == house) { value = l.Value; l.Family = family; }
    // MoveInModeStrs 0/1 "Purchase House?": the family pays the house value and keeps the rest for household items
    fami->Budget -= value;
    fami->HouseNumber = house;
    fami->NetWorth = fami->Budget + value;
    prov->FamilyForHouse[(int16_t)house] = fami;
    Dirty = true;
    RefreshFamilies();
    return MoveResult::Ok;
}

bool NhoodModel::Evict(int house) {
    auto prov = Prov();
    if (prov == nullptr) return false;
    auto fami = prov->GetFamilyForHouse((int16_t)house);
    if (fami == nullptr) return false;
    int value = 0;
    for (auto& l : Lots) if (l.House == house) { value = l.Value; l.Family = -1; }
    // EvictModeStrs 2/3 "The %s family has net worth of %s.  Do you want to evict them?": the house is sold,
    // the family moves to the family bin with its net worth in cash.
    fami->Budget = std::max(fami->NetWorth, fami->Budget + value);
    fami->NetWorth = fami->Budget;
    fami->HouseNumber = 0;
    prov->FamilyForHouse.erase((int16_t)house);
    Dirty = true;
    RefreshFamilies();
    return true;
}

bool NhoodModel::DeleteFamily(int16_t family) {
    auto prov = Prov();
    if (prov == nullptr || prov->MainResource == nullptr) return false;
    auto fami = prov->GetFamily(family);
    if (fami == nullptr || fami->HouseNumber != 0) return false;
    auto fams = prov->MainResource->Get<FAMs>((uint16_t)family);
    prov->MainResource->FullRemoveChunk(fami);
    if (fams) prov->MainResource->FullRemoveChunk(fams);
    _DeletedFamilies.push_back(family);
    Dirty = true;
    RefreshFamilies();
    return true;
}

void NhoodModel::SetBudget(int16_t family, int budget) {
    auto prov = Prov();
    auto fami = prov ? prov->GetFamily(family) : nullptr;
    if (fami == nullptr || fami->Budget == budget) return;
    int house = fami->HouseNumber;
    int value = 0;
    for (auto& l : Lots) if (l.House == house) value = l.Value;
    fami->Budget = budget;
    fami->NetWorth = budget + value;
    Dirty = true;
    RefreshFamilies();
}

bool NhoodModel::Save(std::string* error) {
    auto prov = Prov();
    if (prov == nullptr || prov->MainResource == nullptr) { if (error) *error = "no neighbourhood"; return false; }
    std::string path = UserDir + "/Neighborhood.iff";
    // TS1(sims): the complete neighbourhood (neighbour records incl. person data and relationships, NGBH data
    // and inventories, type attributes, families; deleted families' FAMI/FAMs removed) through the single
    // Neighborhood.iff writer, content/TS1NeighbourhoodWriter (every other chunk byte for byte). This used to rewrite
    // only the FAMI chunks, so run-time neighbour state (trips, persistent person data, inventories) was lost.
    std::vector<std::string> log;
    if (!FSO::Content::TS1::TS1NeighbourhoodWriter::Save(prov, path, _DeletedFamilies, error, &log)) return false;
    for (auto& l : log) printf("[nhood] %s\n", l.c_str());
    _DeletedFamilies.clear();
    Dirty = false;
    return true;
}

}}
