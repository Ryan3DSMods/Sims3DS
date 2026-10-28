/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.Content.TS1.TS1NeighborhoodProvider — see TS1NeighbourProvider.h
#include "files/SafeFile.h" // FIX(sims) SAVE
#include "content/TS1NeighbourProvider.h"
#include "content/TS1NeighbourhoodWriter.h"
#include "content/ContentManager.h"
#include "content/WorldContentFiles.h"
#include "files/formats/iff/chunks/OBJD.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <string>

namespace FSO { namespace Content { namespace TS1 {

using namespace FSO::Files::Formats::IFF;
using namespace FSO::Files::Formats::IFF::Chunks;

static std::string PadHouse(int32_t id) {
    // C#: id.ToString().PadLeft(2, '0')
    std::string s = std::to_string(id);
    while (s.size() < 2) s = "0" + s;
    return s;
}

static std::string Trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) a++;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) b--;
    return s.substr(a, b - a);
}

static std::vector<std::string> SplitComma(const std::string& s) {
    std::vector<std::string> r;
    size_t start = 0;
    while (true) {
        size_t c = s.find(',', start);
        r.push_back(Trim(s.substr(start, (c == std::string::npos) ? std::string::npos : c - start)));
        if (c == std::string::npos) break;
        start = c + 1;
    }
    return r;
}

TS1NeighborhoodProvider::TS1NeighborhoodProvider(Content* contentManager)
{
    ContentManager = contentManager;
    InitSpecific(1);
}

std::unique_ptr<IffFile> TS1NeighborhoodProvider::TryOpen(const std::string& path)
{
    if (!FileExists(path)) return nullptr;
    try {
        return std::unique_ptr<IffFile>(new IffFile(path));
    } catch (...) {
        return nullptr;
    }
}

/// <summary>
/// Intializes a specific neighbourhood. Also counts as a save discard, since it unloads the current neighbourhood.
/// </summary>
void TS1NeighborhoodProvider::InitSpecific(int32_t id)
{
    DirtyAvatars.clear();
    ZoningDictionary.clear();
    FamilyForHouse.clear();
    HouseThumbCache.clear();
    GameState = TS1GameState();
    Neighbors = nullptr;
    Neighborhood = nullptr;
    TypeAttributes = nullptr;

    auto udName = std::string("UserData") + ((id == 0) ? std::string("") : std::to_string(id + 1));
    //simitone shouldn't modify existing ts1 data, since our house saves are incompatible.
    //therefore we should copy to the simitone user data.

    auto userPath = UserDataRoot.empty() ? ContentManager->SimsCompleteDir : UserDataRoot; // C#: FSOEnvironment.SimsCompleteDir (TS1(sims): or the save area)

    // C#: the neighbourhood was always read from "UserData/Neighborhood.iff" (udName unused) while
    // GetHouse used UserPath + "Houses/..." (no UserData). TS1(sims): UserPath is the user data
    // folder itself; "UserData<id+1>" is used when it exists, otherwise "UserData" (the FreeSims behaviour).
    if (DirectoryExists(userPath + "/" + udName)) UserPath = userPath + "/" + udName;
    else UserPath = userPath + "/UserData";

    // TS1(sims): expansion pack templates, newest first (see TemplateDirs in the header).
    TemplateDirs.clear();
    for (auto t : { "TemplateMagicTown", "TemplateStudiotown", "TemplateCommunity", "TemplateVacation", "TemplateDowntown", "TemplateUserData" }) {
        auto dir = userPath + "/" + t;
        if (DirectoryExists(dir)) TemplateDirs.push_back(dir);
    }

    MainResource = TryOpen(UserPath + "/Neighborhood.iff");
    if (MainResource == nullptr) return;

    // C#: List<NBRS>().FirstOrDefault()
    {
        auto list = MainResource->List<NBRS>();
        Neighbors = list.empty() ? nullptr : list[0];
    }
    {
        auto list = MainResource->List<NGBH>();
        Neighborhood = list.empty() ? nullptr : list[0];
        // TS1(sims): inventories (Hot Date+) live in the NGBH chunk; keep one even if the file has none.
        if (Neighborhood == nullptr) {
            OwnedNeighborhood.reset(new NGBH());
            Neighborhood = OwnedNeighborhood.get();
        }
    }

    //todo: manage avatar iffs here

    // ---- TS1(sims): the rest of the neighbourhood (FreeSims declared but never loaded these) ----
    {
        auto list = MainResource->List<TATT>();
        TypeAttributes = list.empty() ? nullptr : list[0];
        if (TypeAttributes == nullptr) {
            OwnedTypeAttributes.reset(new TATT());
            TypeAttributes = OwnedTypeAttributes.get();
        }
    }
    for (auto fami : MainResource->List<FAMI>()) {
        // HouseNumber 0 = not living in a house (family bin / townies).
        if (fami->HouseNumber > 0) FamilyForHouse[(int16_t)fami->HouseNumber] = fami;
    }
    auto gameData = ContentManager->SimsCompleteDir + "/GameData/";
    // TS1(sims): user file, else the newest expansion template, else GameData's (base game) copy.
    LotLocations = TryOpen(FindUserOrTemplate("LotLocations.iff"));
    if (LotLocations == nullptr) LotLocations = TryOpen(gameData + "LotLocations.iff");
    StreetNames = TryOpen(FindUserOrTemplate("StreetNames.iff"));
    if (StreetNames == nullptr) StreetNames = TryOpen(gameData + "StreetNames.iff");
    NeighbourhoodDesc = TryOpen(FindUserOrTemplate("Houses/NeighborhoodDesc.iff"));
    STDesc = TryOpen(FindUserOrTemplate("Houses/STDesc.iff"));
    MTDesc = TryOpen(FindUserOrTemplate("Houses/MTDesc.iff"));
    DTDesc = TryOpen(FindUserOrTemplate("Houses/DTDesc.iff"));
    VIDesc = TryOpen(FindUserOrTemplate("Houses/VIDesc.iff"));
    LoadZoning();
    LoadLotLocations();
    TS1NeighbourhoodWriter::LoadFamilySpells(this, UserPath + "/Neighborhood.iff"); // FIX(sims): Makin' Magic family spells
}

// TS1(sims): "N, residential|community" strings (STR# 1 'Lot Zoning Info').
void TS1NeighborhoodProvider::LoadZoning()
{
    // TS1(sims): the master zoning, then the newest expansion template's, then the user's (later ones win:
    // the user's own zoning of a lot overrides the templates').
    std::vector<std::string> files = { ContentManager->SimsCompleteDir + "/GameData/MasterLotZoning.iff" };
    for (auto it = TemplateDirs.rbegin(); it != TemplateDirs.rend(); ++it) files.push_back(*it + "/LotZoning.iff");
    files.push_back(UserPath + "/LotZoning.iff");
    for (auto& f : files) {
        auto zones = TryOpen(f);
        if (zones == nullptr) continue;
        auto str = zones->Get<STR>(1);
        if (str == nullptr) continue;
        for (int32_t i = 0; i < str->Length(); i++) {
            auto split = SplitComma(str->GetString(i));
            if (split.size() < 2 || split[0].empty()) continue;
            auto type = ToLowerInvariant(split[1]);
            ZoningDictionary[(int16_t)atoi(split[0].c_str())] = (int16_t)((type == "community") ? 1 : 0);
        }
    }
}

// TS1(sims): "house, x, y" strings, STR# 1 'Sims 1.0 Original Neighborhood Screen Co-Ordinates'
// (2: Hot Date downtown, 3: Vacation island, 4: Unleashed neighbourhood). Later tables add their lots.
void TS1NeighborhoodProvider::LoadLotLocations()
{
    LotPositions.clear();
    LotMaps.clear();
    if (LotLocations == nullptr) return;
    for (uint16_t id : {1, 2, 3, 4, 5, 7}) { // TS1(sims): 5 Studio Town, 7 Magic Town (6 = filter bar settings)
        auto str = LotLocations->Get<STR>(id);
        if (str == nullptr) continue;
        for (int32_t i = 0; i < str->Length(); i++) {
            auto split = SplitComma(str->GetString(i));
            if (split.size() < 3 || split[0].empty()) continue;
            int16_t house = (int16_t)atoi(split[0].c_str());
            LotMaps[id].push_back(std::make_tuple(house, atoi(split[1].c_str()), atoi(split[2].c_str())));
            if (LotPositions.find(house) != LotPositions.end()) continue;
            LotPositions[house] = std::make_pair(atoi(split[1].c_str()), atoi(split[2].c_str()));
        }
    }
}

Neighbour* TS1NeighborhoodProvider::GetNeighborByID(int16_t ID)
{
    if (Neighbors == nullptr) return nullptr;
    auto it = Neighbors->NeighbourByID.find(ID);
    return (it == Neighbors->NeighbourByID.end()) ? nullptr : it->second;
}

FAMI* TS1NeighborhoodProvider::GetFamilyForHouse(int16_t ID)
{
    auto it = FamilyForHouse.find(ID);
    return (it == FamilyForHouse.end()) ? nullptr : it->second;
}

bool TS1NeighborhoodProvider::GetNeighborIDForGUID(uint32_t GUID, int16_t& result)
{
    result = 0;
    if (Neighbors == nullptr) return false;
    auto it = Neighbors->DefaultNeighbourByGUID.find(GUID);
    if (it != Neighbors->DefaultNeighbourByGUID.end()) {
        result = it->second;
        return true;
    }
    return false;
}

int16_t TS1NeighborhoodProvider::SetToNext(int16_t current)
{
    if (Neighbors == nullptr) return -1;
    // C#: Neighbors.Entries.FirstOrDefault(x => x.NeighbourID > current) (entries are sorted by id)
    for (auto& x : Neighbors->Entries) {
        if (x->NeighbourID > current) return x->NeighbourID;
    }
    return -1;
}

int16_t TS1NeighborhoodProvider::SetToNext(int16_t current, uint32_t guid)
{
    if (Neighbors == nullptr) return -1;
    for (auto& x : Neighbors->Entries) {
        if (x->NeighbourID > current && x->GUID == guid) return x->NeighbourID;
    }
    return -1;
}

bool TS1NeighborhoodProvider::SaveNeighbourhood(bool withSims)
{
    (void)withSims;
    //todo: save iffs for dirty avatars.
    DirtyAvatars.clear();
    if (MainResource == nullptr) return false;

    st::Writer stream;
    MainResource->Write(stream);
    auto& data = stream.ToArray();
    return FSO::Files::SafeFile::Write(UserPath + "/Neighborhood.iff", data); // FIX(sims) SAVE (was a plain overwrite)
}

bool TS1NeighborhoodProvider::SaveHouse(int32_t houseID, IffFile* file)
{
    st::Writer stream;
    file->Write(stream);
    auto& data = stream.ToArray();
    return FSO::Files::SafeFile::Write(GetHousePath(houseID), data); // FIX(sims) SAVE (was a plain overwrite)
}

std::unique_ptr<IffFile> TS1NeighborhoodProvider::GetHouse(int32_t id)
{
    return std::unique_ptr<IffFile>(new IffFile(GetHousePath(id)));
}

std::string TS1NeighborhoodProvider::GetHousePath(int32_t id)
{
    // TS1(sims): expansion lots not yet saved in the neighbourhood are read from their template (see TemplateDirs).
    auto path = FindUserOrTemplate("Houses/House" + PadHouse(id) + ".iff");
    if (!path.empty()) return path;
    return UserPath + "/Houses/House" + PadHouse(id) + ".iff";
}

// TS1(sims): see header.
std::string TS1NeighborhoodProvider::FindUserOrTemplate(const std::string& rel)
{
    if (FileExists(UserPath + "/" + rel)) return UserPath + "/" + rel;
    auto slash = rel.find_last_of('/');
    auto file = (slash == std::string::npos) ? rel : rel.substr(slash + 1);
    for (auto& dir : TemplateDirs) {
        if (FileExists(dir + "/" + file)) return dir + "/" + file;
        if (FileExists(dir + "/" + rel)) return dir + "/" + rel; // TemplateUserData keeps the user layout
    }
    return "";
}

bool TS1NeighborhoodProvider::HouseExists(int32_t id)
{
    return !FindUserOrTemplate("Houses/House" + PadHouse(id) + ".iff").empty();
}

int16_t TS1NeighborhoodProvider::LotKindOfHouse(int32_t id)
{
    if (id >= 21 && id < 40) return 1;
    if (id >= 40 && id < 50) return 2;
    if (id >= 50 && id <= 80) return 3;
    if (id >= 81 && id <= 89) return 4;
    if (id >= 90 && id <= 99) return 5;
    return 0;
}

std::vector<InventoryItem>* TS1NeighborhoodProvider::GetInventoryByNID(int16_t id)
{
    if (Neighborhood == nullptr) return nullptr;
    auto it = Neighborhood->InventoryByID.find(id);
    return (it == Neighborhood->InventoryByID.end()) ? nullptr : &it->second;
}

std::vector<InventoryItem>& TS1NeighborhoodProvider::InitInventoryForNID(int16_t id)
{
    if (Neighborhood == nullptr) {
        OwnedNeighborhood.reset(new NGBH());
        Neighborhood = OwnedNeighborhood.get();
    }
    return Neighborhood->InventoryByID[id];
}

int32_t TS1NeighborhoodProvider::GetMagicoinsForNeighbor(int16_t id)
{
    auto inv = GetInventoryByNID(id);
    if (inv == nullptr) return 0;
    for (auto& x : *inv) if (x.GUID == 0x99E81BEC) return x.Count;
    return 0;
}

BMP* TS1NeighborhoodProvider::GetHouseThumb(int32_t id)
{
    auto it = HouseThumbCache.find(id);
    if (it == HouseThumbCache.end()) {
        auto house = TryOpen(GetHousePath(id));
        if (house == nullptr) return nullptr;
        it = HouseThumbCache.emplace(id, std::move(house)).first;
    }
    return it->second->Get<BMP>(512); //roof on
}

int16_t TS1NeighborhoodProvider::GetTATT(uint32_t guid, int32_t index)
{
    if (TypeAttributes == nullptr) return 0;
    auto it = TypeAttributes->TypeAttributesByGUID.find(guid);
    if (it != TypeAttributes->TypeAttributesByGUID.end())
    {
        auto& dat = it->second;
        if (index < 0 || index >= (int32_t)dat.size()) return 0;
        else return dat[index];
    }
    return 0;
}

std::pair<std::string, std::string> TS1NeighborhoodProvider::GetHouseNameDesc(int32_t houseID)
{
    STR* res = nullptr;
    // C# dereferenced the description iffs unconditionally (they were never loaded in FreeSims).
    // TS1(sims): downtown (21-39) and vacation (40-49) names come from their templates' DTDesc/VIDesc.
    if (houseID >= 21 && houseID < 40 && DTDesc != nullptr) res = DTDesc->Get<STR>((uint16_t)(houseID + 2000));
    else if (houseID >= 40 && houseID < 50 && VIDesc != nullptr) res = VIDesc->Get<STR>((uint16_t)(houseID + 2000));
    else if (houseID < 80) res = (NeighbourhoodDesc != nullptr) ? NeighbourhoodDesc->Get<STR>((uint16_t)(houseID + 2000)) : nullptr;
    else if (houseID < 90) res = (STDesc != nullptr) ? STDesc->Get<STR>((uint16_t)(houseID + 2000)) : nullptr;
    else res = (MTDesc != nullptr) ? MTDesc->Get<STR>((uint16_t)(houseID + 2000)) : nullptr;

    if (res == nullptr) return std::make_pair(std::string(""), std::string(""));
    else return std::make_pair(res->GetString(0), res->GetString(1));
}

void TS1NeighborhoodProvider::SetTATT(uint32_t guid, int32_t index, int16_t value)
{
    if (TypeAttributes == nullptr) return;
    auto it = TypeAttributes->TypeAttributesByGUID.find(guid);
    if (it == TypeAttributes->TypeAttributesByGUID.end())
    {
        auto obj = ContentManager->WorldObjects->Get(guid, false);
        if (obj == nullptr) return;
        it = TypeAttributes->TypeAttributesByGUID.emplace(guid, std::vector<int16_t>(32, 0)).first;
    }
    auto& dat = it->second;
    if (index < 0 || index >= (int32_t)dat.size()) return;
    else dat[index] = value;
}

// ---------------------------------------------------------------- TS1(sims) additions

FAMI* TS1NeighborhoodProvider::GetFamily(int16_t familyNumber)
{
    if (MainResource == nullptr || familyNumber < 0) return nullptr;
    return MainResource->Get<FAMI>((uint16_t)familyNumber);
}

std::string TS1NeighborhoodProvider::GetFamilyName(int16_t familyNumber)
{
    if (MainResource == nullptr || familyNumber < 0) return "";
    auto str = MainResource->Get<FSO::Files::Formats::IFF::Chunks::FAMs>((uint16_t)familyNumber);
    if (str == nullptr || str->Length() == 0) return "";
    return str->GetString(0);
}

std::vector<FAMI*> TS1NeighborhoodProvider::ListFamilies()
{
    std::vector<FAMI*> result;
    if (MainResource == nullptr) return result;
    result = MainResource->List<FAMI>();
    std::sort(result.begin(), result.end(), [](FAMI* a, FAMI* b) { return a->ChunkID < b->ChunkID; });
    return result;
}

int16_t TS1NeighborhoodProvider::GetHouseOfNeighbor(Neighbour* neigh)
{
    if (neigh == nullptr || (int32_t)neigh->PersonData.size() <= PERSON_FAMILY_NUMBER) return 0;
    auto fam = GetFamily(neigh->PersonData[PERSON_FAMILY_NUMBER]);
    if (fam == nullptr || neigh->PersonData[PERSON_FAMILY_NUMBER] <= 0) return 0;
    return (int16_t)fam->HouseNumber;
}

int16_t TS1NeighborhoodProvider::GetZoning(int16_t houseNumber)
{
    auto it = ZoningDictionary.find(houseNumber);
    if (it != ZoningDictionary.end()) return it->second;
    // TS1(sims): FreeSO VMGenericTS1Call ReturnZoningTypeOfLotInTemp0 — lots missing from the zoning tables
    // (downtown, vacation, studio town) are community lots; Studio Town lots (81-89) answer 2.
    return (int16_t)((houseNumber >= 81 && houseNumber <= 89) ? 2 : 1);
}

int16_t TS1NeighborhoodProvider::SetToNextClosestHouse(int16_t fromHouse, int16_t current)
{
    auto from = LotPositions.find(fromHouse);
    std::vector<std::pair<int64_t, int16_t>> order; // (squared distance, house)
    for (auto& kv : LotPositions) {
        if (kv.first == fromHouse) continue;
        if (GetZoning(kv.first) != 0) continue; // only residential lots have neighbours living in them
        int64_t d = 0;
        if (from != LotPositions.end()) {
            int64_t dx = kv.second.first - from->second.first, dy = kv.second.second - from->second.second;
            d = dx * dx + dy * dy;
        }
        order.push_back(std::make_pair(d, kv.first));
    }
    std::sort(order.begin(), order.end());
    if (current <= 0) return order.empty() ? (int16_t)-1 : order[0].second;
    for (size_t i = 0; i < order.size(); i++) {
        if (order[i].second == current) return (i + 1 < order.size()) ? order[i + 1].second : (int16_t)-1;
    }
    return -1;
}

std::string TS1NeighborhoodProvider::FindCharacterFile(const std::string& neighbourName)
{
    if (neighbourName.empty()) return "";
    auto dir = UserPath + "/Characters/";
    std::vector<std::string> candidates;
    candidates.push_back(neighbourName + ".iff");
    auto lower = ToLowerInvariant(neighbourName);
    candidates.push_back(lower + ".iff");
    if (!lower.empty()) {
        auto cap = lower; cap[0] = (char)toupper((unsigned char)cap[0]);
        candidates.push_back(cap + ".iff");
        candidates.push_back(cap + ".IFF");
    }
    for (auto& c : candidates) {
        if (FileExists(dir + c)) return dir + c;
    }
    return "";
}

/// TS1(sims): character iffs (UserData/Characters/User*.iff) hold the OBJD (person), body strings
/// (STR# 200), names, suits and the user's semi-global link. The Sims 1 treats them as objects: the neighbour's
/// GUID is the OBJD GUID. Register them the way WorldObjectProvider registers loose standalone iffs.
/// The neighbour record's Name is the character file name ("user00000"), so no directory scan is needed
/// (plain arm-none-eabi newlib has no <dirent.h>).
int32_t TS1NeighborhoodProvider::LoadCharacters()
{
    CharacterFiles.clear();
    if (Neighbors == nullptr) return 0;
    auto world = ContentManager->WorldObjects.get();
    if (world == nullptr) return 0;
    int32_t count = 0;
    for (auto& neigh : Neighbors->Entries) {
        if (neigh->Unknown1 != 1 || neigh->GUID == 0) continue;
        if (world->Entries.find(neigh->GUID) != world->Entries.end()) continue; // e.g. NPCs in Objects.far
        auto path = FindCharacterFile(neigh->Name);
        if (path.empty()) continue;
        std::unique_ptr<IffFile> iff;
        try { iff.reset(new IffFile(path)); } catch (...) { continue; }
        iff->RuntimeInfo.Path = path;
        iff->RuntimeInfo.State = IffRuntimeState::Standalone;
        bool any = false;
        std::vector<OBJD*> objs;
        try { objs = iff->List<OBJD>(); } catch (...) { continue; }
        for (auto objd : objs) {
            world->AddObject(iff.get(), objd, -1);
            any = true;
        }
        if (any) { CharacterFiles.push_back(path); count++; }
    }
    // TS1(sims): the Hot Date "downtownies" (TemplateNPCs/HDT_*.iff) the installer copies into Characters/.
    // Registered as standalone objects (characters already present in the user folder win).
    TemplateCharacterFiles.clear();
    auto tdir = ContentManager->SimsCompleteDir + "/TemplateNPCs";
    for (auto& n : FSO::Content::WorldContentFiles::List(tdir)) {
        auto l = ToLowerInvariant(n);
        if (l.size() < 4 || l.compare(l.size() - 4, 4, ".iff") != 0) continue;
        if (world->AddStandaloneIff(tdir + "/" + n, false) > 0) TemplateCharacterFiles.push_back(tdir + "/" + n);
    }
    return count;
}

}}}
