/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// TS1(sims): see VMTS1Travel.h. RequestLotSwitch / VerifyFamily / ActivateFamily / the sim data helpers are
// translated from FreeSO (VMGenericTS1Call ChangeToLotInTemp0, VMTS1LotState); Arrive is new.
#include "simantics/engine/VMTS1Travel.h"
#include "simantics/engine/VMTS1Neighbourhood.h"
#include "simantics/engine/VMTS1Outfits.h"
#include "simantics/engine/VMSuitProvider.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/entities/VMMultitileGroup.h"
#include "simantics/model/VMPersonDataVariable.h"
#include "simantics/model/VMMotive.h"
#include "simantics/model/VMBHAVOwnerPair.h"
#include "simantics/primitives/VMFindLocationFor.h"
#include "simantics/utils/VMTS1FamilyLoader.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"
#include "content/ContentManager.h"
#include "content/TS1NeighbourProvider.h"
#include "content/TS1CharacterFiles.h" // FIX(sims) QA3: Superstar townies
#include "content/TS1SimDesc.h"
#include "content/TS1SkinCatalog.h"
#include "files/formats/iff/chunks/NBRS.h"
#include "files/formats/iff/chunks/BHAV.h"
#include "files/formats/iff/chunks/TTAB.h"
#include "files/formats/iff/chunks/FAMI.h"
#include "files/formats/iff/chunks/STR.h"
#include "files/formats/iff/chunks/NGBH.h"
#include "files/formats/iff/chunks/OBJD.h"
#include <algorithm>
#include <map>
#include <functional>
#include <set>
#include <cctype>
#include <cstdlib>
#include <cstdio>

namespace FSO { namespace SimAntics { namespace Engine { namespace Utils {

using FSO::Files::Formats::IFF::Chunks::FAMI;
using FSO::Files::Formats::IFF::Chunks::InventoryItem;
using FSO::LotView::Model::LotTilePos;
using FSO::LotView::Model::Direction;
using Model::VMPersonDataVariable;
using Model::VMMotive;

static const VMMotive SimDataMotives[7] = { VMMotive::Bladder, VMMotive::Comfort, VMMotive::Energy, VMMotive::Fun,
                                            VMMotive::Hunger, VMMotive::Hygiene, VMMotive::Social };

std::vector<InventoryItem>& VMTS1Travel::InitInventory(int16_t neighbour)
{
    return VMTS1Neighbourhood::Provider()->InitInventoryForNID(neighbour);
}

int16_t VMTS1Travel::GetSimData(int16_t neighbour, uint32_t slot, bool* found)
{
    if (found) *found = false;
    auto prov = VMTS1Neighbourhood::Provider();
    auto inv = (prov == nullptr) ? nullptr : prov->GetInventoryByNID(neighbour);
    if (inv == nullptr) return 0;
    for (auto& x : *inv) if (x.Type == (int32_t)SIMDATA_TYPE && x.GUID == slot) { if (found) *found = true; return (int16_t)x.Count; }
    return 0;
}

void VMTS1Travel::SaveSimData(int16_t neighbour, uint32_t slot, int16_t data)
{
    if (VMTS1Neighbourhood::Provider() == nullptr) return;
    auto& inv = InitInventory(neighbour);
    for (auto& x : inv) if (x.Type == (int32_t)SIMDATA_TYPE && x.GUID == slot) { x.Count = (uint16_t)data; return; }
    InventoryItem n; n.Type = (int32_t)SIMDATA_TYPE; n.GUID = slot; n.Count = (uint16_t)data;
    inv.push_back(n);
}

void VMTS1Travel::RemoveSimData(int16_t neighbour, uint32_t slot)
{
    auto prov = VMTS1Neighbourhood::Provider();
    auto inv = (prov == nullptr) ? nullptr : prov->GetInventoryByNID(neighbour);
    if (inv == nullptr) return;
    inv->erase(std::remove_if(inv->begin(), inv->end(), [&](const InventoryItem& x) { return x.Type == (int32_t)SIMDATA_TYPE && x.GUID == slot; }), inv->end());
}

void VMTS1Travel::Signal(VM* vm, int32_t evt, int32_t value)
{
    if (vm->OnTS1Event) vm->OnTS1Event(evt, value);
}

void VMTS1Travel::ActivateFamily(VM* vm, FAMI* family)
{
    if (family == nullptr) return;
    vm->SetGlobalValue(VMTS1Neighbourhood::GLOBAL_CURRENT_FAMILY, (int16_t)family->ChunkID);
    // TS1(sims): the family's money (globals 13/14, read by Transfer Funds and the UI) comes with it to a
    // visited lot (a Downtown/Vacation/community lot has no resident family, so they were 0: "§0 on arrival").
    vm->SetGlobalValue(VMTS1Neighbourhood::GLOBAL_BUDGET_MOD, (int16_t)(family->Budget % 10000));
    vm->SetGlobalValue(VMTS1Neighbourhood::GLOBAL_BUDGET_DIV, (int16_t)(family->Budget / 10000));
    auto prov = VMTS1Neighbourhood::Provider();
    if (prov != nullptr) prov->GameState.ActiveFamily = family;
    ApplyFamilySpells(vm); // FIX(sims)
}

void VMTS1Travel::SelectSim(VM* vm, VMAvatar* avatar)
{
    if (avatar == nullptr) return;
    vm->SetGlobalValue(VMTS1Neighbourhood::GLOBAL_SELECTED_PERSON, avatar->ObjectID);
    Signal(vm, EventSelectedSimChanged, avatar->ObjectID);
}

void VMTS1Travel::KeepSelectedSim(VM* vm)
{
    int16_t sel = vm->GetGlobalValue(VMTS1Neighbourhood::GLOBAL_SELECTED_PERSON);
    if (sel == 0) return;
    auto cur = dynamic_cast<VMAvatar*>(vm->GetObjectById(sel));
    if (cur != nullptr && !cur->Dead) return;
    int16_t family = vm->GetGlobalValue(VMTS1Neighbourhood::GLOBAL_CURRENT_FAMILY);
    if (family > 0) for (auto ent : vm->Entities) {
        auto av = dynamic_cast<VMAvatar*>(ent);
        if (av == nullptr || av->Dead || av->GetPersonData((Model::VMPersonDataVariable)VMTS1Neighbourhood::PD_FAMILY_NUMBER) != family) continue;
        SelectSim(vm, av);
        return;
    }
    vm->SetGlobalValue(VMTS1Neighbourhood::GLOBAL_SELECTED_PERSON, 0);
}

VMAvatar* VMTS1Travel::CreatePerson(VM* vm, uint32_t guid)
{
    if (guid == 0) return nullptr;
    auto group = vm->Context->CreateObjectInstance(guid, LotTilePos::OUT_OF_WORLD, Direction::NORTH, true);
    if (group == nullptr || group->Objects.empty()) return nullptr;
    auto avatar = dynamic_cast<VMAvatar*>(group->Objects[0]);
    if (avatar != nullptr) WearCurrentOutfit(avatar); // FIX(sims)
    return avatar;
}

// FIX(sims): person data 8 "current outfit" (a person suit id) is what the Sim arrives wearing. The Vacation phone
// plugin's 'Go On Vacation - Dress Appropriately' sets it for the whole family before the trip (17 winter for the snow
// lots 40-42, 2 swimwear for the beaches 46-48, 0 everyday); the travellers are created on the vacation lot from their
// neighbour records and always came in their everyday clothes. A Sim without its own winter wear (body string 33,
// characters made before Vacation) takes the first winter outfit of its kind from the clothing collections.
void VMTS1Travel::WearCurrentOutfit(VMAvatar* avatar)
{
    auto suitId = avatar->GetPersonData((VMPersonDataVariable)8);
    if (suitId <= 0 || !VMTS1Outfits::PetType(avatar).empty()) return;
    auto suit = VMSuitProvider::GetPersonSuitTS1(avatar, (uint16_t)suitId);
    if (suit.IsNull() && suitId == 17) {
        auto winter = VMTS1Outfits::ValidOutfits(avatar, 4);
        if (!winter.empty()) {
            auto oft = VMSuitProvider::RegisterTS1Outfit(winter[0].BodyString, VMSuitProvider::BodyStringsOf(avatar));
            if (oft != 0) suit = VMSuitResult::Outfit(oft);
        }
    }
    if (suit.Type == VMSuitResult::Kind::OutfitID && suit.OutfitID != 0) avatar->BodyOutfit(suit.OutfitID);
}

static VMEntity* FindMailbox(VM* vm)
{
    for (auto e : vm->Entities) {
        auto g = e->Object->OBJ->GUID;
        if (g == 0xEF121974u || g == 0x1D95C9B0u) return e; // FreeSO VerifyFamily
    }
    VMTS1FamilyLoader loader(vm);
    return loader.FindArrivalPoint();
}

std::vector<VMAvatar*> VMTS1Travel::VerifyFamily(VM* vm)
{
    std::vector<VMAvatar*> created;
    auto family = VMTS1Neighbourhood::CurrentFamily(vm);
    if (family == nullptr) {
        vm->SetGlobalValue(32, 1);
        return created;
    }
    vm->SetGlobalValue(32, 0);
    vm->SetGlobalValue(VMTS1Neighbourhood::GLOBAL_CURRENT_FAMILY, (int16_t)family->ChunkID);
    ApplyFamilySpells(vm); // FIX(sims)
    std::vector<uint32_t> missingMembers(family->RuntimeSubset.begin(), family->RuntimeSubset.end());
    for (auto e : vm->Entities) {
        if (dynamic_cast<VMAvatar*>(e) == nullptr || e->Dead) continue;
        missingMembers.erase(std::remove(missingMembers.begin(), missingMembers.end(), e->Object->OBJ->GUID), missingMembers.end());
    }
    for (auto member : missingMembers) {
        auto sim = CreatePerson(vm, member);
        if (sim == nullptr) continue;
        sim->SetPersonData((VMPersonDataVariable)VMTS1Neighbourhood::PD_FAMILY_NUMBER, (int16_t)family->ChunkID);
        auto mailbox = FindMailbox(vm);
        if (mailbox != nullptr) Primitives::VMFindLocationFor::FindLocationFor(sim, mailbox, vm->Context);
        created.push_back(sim);
    }
    return created;
}

void VMTS1Travel::LoadFamilySpells(VM* vm, VMEntity* controller)
{
    auto prov = VMTS1Neighbourhood::Provider();
    if (prov == nullptr || controller == nullptr) return;
    auto fam = vm->GetGlobalValue(VMTS1Neighbourhood::GLOBAL_CURRENT_FAMILY);
    auto it = prov->GameState.FamilySpells.find(fam);
    if (it == prov->GameState.FamilySpells.end()) return;
    for (size_t i = 0; i < it->second.size() && i < 6; i++) controller->SetAttribute((int32_t)i, it->second[i]);
}

// FIX(sims): the Magic Controller is a global object created (and its 'init'/'load' trees run, Generic Sims Call 43)
// before the lot's family is activated, so global 9 "current family" was still 0 and nothing was restored (the
// controller is not restored from the house file either: VMWorldActivator recreates global objects). Called once the
// family is known (VMTS1FamilyLoader, ActivateFamily, Arrive).
void VMTS1Travel::ApplyFamilySpells(VM* vm)
{
    for (auto e : vm->Entities)
        if (!e->Dead && e->Object != nullptr && e->Object->OBJ->GUID == MAGIC_CONTROLLER_GUID) LoadFamilySpells(vm, e);
}

std::vector<VMTS1Travel::SpellbookEntry> VMTS1Travel::Spellbook(VM* vm)
{
    std::vector<SpellbookEntry> out;
    auto content = FSO::Content::Content::Get();
    auto obj = content ? content->WorldObjects->Get(MAGIC_CONTROLLER_GUID, true) : nullptr;
    if (obj == nullptr || obj->Resource == nullptr) return out;
    auto recipes = obj->Resource->Get<FSO::Files::Formats::IFF::Chunks::STR>(3);
    auto ingredients = obj->Resource->Get<FSO::Files::Formats::IFF::Chunks::STR>(2);
    if (recipes == nullptr) return out;
    VMEntity* controller = nullptr;
    if (vm != nullptr) for (auto e : vm->Entities) if (!e->Dead && e->Object && e->Object->OBJ->GUID == MAGIC_CONTROLLER_GUID) { controller = e; break; }
    for (int32_t i = 1; i < recipes->Length(); i++) {
        auto item = recipes->GetStringEntry(i);
        if (item == nullptr || item->Value.empty()) continue;
        SpellbookEntry e;
        bool valid = true;
        size_t p = 0;
        while (p < item->Value.size()) {
            auto c = item->Value.find(',', p);
            int n = atoi(item->Value.substr(p, c == std::string::npos ? std::string::npos : c - p).c_str());
            if (n <= 0 || n > 31) { valid = false; break; } // 50/51: "TBD" placeholders
            e.Ingredients.push_back(ingredients ? ingredients->GetString(n) : std::to_string(n));
            if (c == std::string::npos) break;
            p = c + 1;
        }
        if (!valid || e.Ingredients.empty()) continue;
        // "!DoDishes : Gold Thread ..." / "!Turn To Frog? Butter-11 ..." -> "Do Dishes" / "Turn To Frog"
        std::string c = item->Comment;
        if (!c.empty() && c[0] == '!') c.erase(0, 1);
        size_t end = c.find_first_of(":?(");
        size_t dbl = c.find("  ");
        if (dbl != std::string::npos && dbl < end) end = dbl;
        c = c.substr(0, end);
        while (!c.empty() && c.back() == ' ') c.pop_back();
        for (size_t k = 1; k < c.size(); k++)
            if (isupper((unsigned char)c[k]) && islower((unsigned char)c[k - 1])) c.insert(k++, " ");
        e.Name = c.empty() ? ("Spell " + std::to_string(i)) : c;
        if (controller != nullptr) e.Made = ((controller->GetAttribute((i - 1) / 16) >> ((i - 1) % 16)) & 1) != 0;
        out.push_back(e);
    }
    return out;
}

void VMTS1Travel::SaveFamilySpells(VM* vm)
{
    auto prov = VMTS1Neighbourhood::Provider();
    if (prov == nullptr) return;
    auto fam = vm->GetGlobalValue(VMTS1Neighbourhood::GLOBAL_CURRENT_FAMILY);
    if (fam <= 0) return;
    for (auto e : vm->Entities) {
        if (e->Dead || e->Object == nullptr || e->Object->OBJ->GUID != MAGIC_CONTROLLER_GUID) continue;
        std::vector<int16_t> bits;
        bool any = false;
        for (int i = 0; i < 6; i++) { bits.push_back(e->GetAttribute(i)); any = any || bits.back() != 0; }
        if (any || prov->GameState.FamilySpells.count(fam)) prov->GameState.FamilySpells[fam] = bits;
        return;
    }
}

void VMTS1Travel::RequestLotSwitch(VM* vm, VMAvatar* caller, int16_t lotId)
{
    auto prov = VMTS1Neighbourhood::Provider();
    if (prov == nullptr) return;
    SaveFamilySpells(vm); // TS1(sims): Makin' Magic spellbook travels with the family
    //-1 is this family's home lot
    bool vacation = lotId >= 40 && lotId < 50;
    auto& crossData = prov->GameState;
    auto family = VMTS1Neighbourhood::CurrentFamily(vm);
    crossData.ActiveFamily = family;
    crossData.DowntownSimGUID = (caller != nullptr) ? caller->Object->OBJ->GUID : 0;
    crossData.LotTransitInfo = (vacation) ? (int16_t)1 : vm->GetGlobalValue(34);
    int16_t here = vm->GetGlobalValue(VMTS1Neighbourhood::GLOBAL_CURRENT_HOUSE);
    if (family != nullptr && family->HouseNumber == here) crossData.ReturnHouse = here;
    std::vector<VMAvatar*> people;
    if (caller != nullptr) people.push_back(caller);
    if (crossData.LotTransitInfo >= 1 && family != nullptr) {
        for (auto e : vm->Entities) {
            auto person = dynamic_cast<VMAvatar*>(e);
            if (person == nullptr || person == caller || person->Dead) continue;
            if (person->GetPersonData((VMPersonDataVariable)VMTS1Neighbourhood::PD_FAMILY_NUMBER) == (int16_t)family->ChunkID)
                people.push_back(person);
        }
    }
    int pi = 0;
    for (auto person : people) {
        auto nid = person->GetPersonData(VMPersonDataVariable::NeighborId);
        for (int m = 0; m < 7; m++) SaveSimData(nid, (uint32_t)m, person->GetMotiveData(SimDataMotives[m]));
        if (crossData.LotTransitInfo > 1 && family != nullptr) SaveSimData(nid, SLOT_FAMILY_SIZE, (int16_t)family->FamilyGUIDs.size());
        if (pi++ == 0) {
            SaveSimData(nid, SLOT_HOURS, (int16_t)vm->Context->Clock->Hours);
            SaveSimData(nid, SLOT_MINUTES, (int16_t)vm->Context->Clock->Minutes);
        }
        // the motive/neighbour state the travellers take with them (FreeSO saves the family on lot exit)
        VMTS1Neighbourhood::SavePersistentData(person);
    }
    //the original game sends avatar motive data along in their inventory under type 2
    //this is called "inventory sim data effects"
    // FIX(sims): the date travels too (each lot file keeps its own clock; only the hours were carried over)
    crossData.TravelDay = vm->Context->Clock->DayOfMonth;
    crossData.TravelMonth = vm->Context->Clock->Month;
    crossData.TravelYear = vm->Context->Clock->Year;
    crossData.PendingLotSwitch = lotId;
    if (vm->OnRequestLotSwitch) vm->OnRequestLotSwitch(lotId);
}

int16_t VMTS1Travel::ResolveLot(int16_t lotId)
{
    auto prov = VMTS1Neighbourhood::Provider();
    if (lotId > 0 || prov == nullptr) return lotId;
    auto fam = prov->GameState.ActiveFamily;
    if (fam != nullptr && fam->HouseNumber > 0) return (int16_t)fam->HouseNumber;
    return prov->GameState.ReturnHouse;
}

int16_t VMTS1Travel::LotMapForDialog(uint8_t dialogType)
{
    // FreeSO VMDialogPrivateStrings.TypeToNeighID
    switch (dialogType) {
        case 8: return 4;  // TS1Neighborhood (Unleashed Old Town)
        case 5: return 2;  // TS1Downtown
        case 7: return 3;  // TS1Vacation
        case 11: return 5; // TS1StudioTown
        case 13: return 7; // TS1Magictown
        default: return 0;
    }
}

// FIX(sims): push the first interaction (on any object) whose action tree has this BHAV label onto sim.
bool VMTS1Travel::PushTreeByLabel(VM* vm, VMAvatar* sim, const std::string& label, std::vector<std::string>* log)
{
    for (auto e : vm->Entities) {
        if (e->Dead || e->TreeTable == nullptr) continue;
        for (auto& ia : e->TreeTable->Interactions) {
            auto tree = e->GetBHAVWithOwner(ia->ActionFunction, vm->Context);
            if (tree == nullptr || tree->bhav == nullptr || tree->bhav->ChunkLabel.find(label) == std::string::npos) continue;
            e->PushUserInteraction((int32_t)ia->TTAIndex, sim, vm->Context);
            if (log) log->push_back("  pushed '" + label + "' (" + e->ToString() + ") on " + sim->Name());
            return true;
        }
    }
    return false;
}

std::vector<VMAvatar*> VMTS1Travel::Arrive(VM* vm, int16_t house, std::vector<std::string>* log)
{
    std::vector<VMAvatar*> result;
    char buf[256];
    auto prov = VMTS1Neighbourhood::Provider();
    if (prov == nullptr) return result;
    auto& gs = prov->GameState;
    gs.PendingLotSwitch = -1;
    vm->SetGlobalValue(VMTS1Neighbourhood::GLOBAL_CURRENT_HOUSE, house);
    auto family = gs.ActiveFamily;
    bool travelling = gs.DowntownSimGUID != 0 && family != nullptr;
    bool home = family != nullptr && family->HouseNumber == house;

    if (!travelling || home) {
        // home lot (or a plain lot load): the family lives here
        VMTS1FamilyLoader loader(vm);
        result = loader.LoadFamily(house, home ? family : nullptr);
        if (log) for (auto& l : loader.Log) log->push_back(l);
        if (travelling) {
            // the tail of the original's resumed "Go Downtown"/"Go On Vacation" interaction (see header)
            VMAvatar* returning = nullptr;
            for (auto a : result) if (a->Object->OBJ->GUID == gs.DowntownSimGUID) returning = a;
            for (auto a : result) {
                auto nid = a->GetPersonData(VMPersonDataVariable::NeighborId);
                bool any = false;
                for (int m = 0; m < 7; m++) {
                    bool found = false;
                    auto v = GetSimData(nid, (uint32_t)m, &found);
                    if (found) { a->SetMotiveData(SimDataMotives[m], v); any = true; }
                    RemoveSimData(nid, (uint32_t)m);
                }
                bool fh = false, fm = false;
                auto hh = GetSimData(nid, SLOT_HOURS, &fh), mm = GetSimData(nid, SLOT_MINUTES, &fm);
                if (fh && fm) {
                    // "Get Downtown Time From SO's Inventory": time at home moves on to when the Sim left
                    auto clock = vm->Context->Clock.get();
                    if (gs.TravelDay > 0) { // FIX(sims): continue from the date the travellers left the other lot
                        clock->DayOfMonth = gs.TravelDay; clock->Month = gs.TravelMonth; clock->Year = gs.TravelYear;
                    }
                    else if (hh * 60 + mm < clock->Hours * 60 + clock->Minutes) clock->DayOfMonth++;
                    clock->Hours = hh; clock->Minutes = mm;
                    vm->SetGlobalValue(0, hh); vm->SetGlobalValue(5, mm);
                }
                RemoveSimData(nid, SLOT_HOURS); RemoveSimData(nid, SLOT_MINUTES); RemoveSimData(nid, SLOT_FAMILY_SIZE);
                // FIX(sims): the vacation hotel key (token 0x3AC022D6) is handed back by every traveller. The phone
                // plugin's 'Spawn Vacation Purchases' ('Clear Hotel Keys') only runs for the Sim who made the trip,
                // so the rest of the family kept their keys after coming home.
                if (auto inv = prov->GetInventoryByNID(nid))
                    inv->erase(std::remove_if(inv->begin(), inv->end(), [](const FSO::Files::Formats::IFF::Chunks::InventoryItem& x) { return x.GUID == 0x3AC022D6; }), inv->end());
                if (any && log) { snprintf(buf, sizeof buf, "  %s: travel motives restored", a->Name().c_str()); log->push_back(buf); }
            }
            if (returning != nullptr) {
                // Generic Sims Call 20: the date who follows the Sim home
                auto nid = returning->GetPersonData(VMPersonDataVariable::NeighborId);
                bool found = false;
                auto dateNid = GetSimData(nid, SLOT_FOLLOWHOME, &found);
                auto neigh = found ? VMTS1Neighbourhood::GetNeighbor(dateNid) : nullptr;
                if (neigh != nullptr) {
                    auto date = CreatePerson(vm, neigh->GUID);
                    if (date != nullptr) {
                        Primitives::VMFindLocationFor::FindLocationFor(date, returning, vm->Context);
                        if (log) { snprintf(buf, sizeof buf, "  date %s came home with %s", date->Name().c_str(), returning->Name().c_str()); log->push_back(buf); }
                    }
                }
                RemoveSimData(nid, SLOT_FOLLOWHOME);
                SelectSim(vm, returning);
            }
            // FIX(sims): what the family bought on the trip (Vacation souvenirs/postcards, Downtown/Studio/Magic Town
            // purchases: inventory tokens) is laid out at home by the Vacation phone plugin's "Interaction - Spawn
            // Vacation Purchases" (its TEST looks for those tokens). The original pushes it when the family is back;
            // nothing did here, so purchases stayed tokens and never appeared at home.
            VMAvatar* spawner = returning != nullptr ? returning : (result.empty() ? nullptr : result[0]);
            if (spawner != nullptr) PushTreeByLabel(vm, spawner, "Interaction - Spawn Vacation Purchases", log);
            gs.DowntownSimGUID = 0;
            gs.TravelDay = 0; // FIX(sims)
            gs.LotTransitInfo = 0;
            gs.VisitingHouse = 0;
        }
        return result;
    }

    // visiting an expansion pack lot: the lot's ped markers / controllers spawn the travellers
    // (Generic Sims Call 18 / 26 use GameState), so only the family identity is set here.
    ActivateFamily(vm, family);
    if (gs.TravelDay > 0) { // FIX(sims): same calendar date as the lot the family came from (hours: Generic Sims Call 23)
        auto clock = vm->Context->Clock.get();
        clock->DayOfMonth = gs.TravelDay; clock->Month = gs.TravelMonth; clock->Year = gs.TravelYear;
    }
    gs.VisitingHouse = house;
    vm->SetGlobalValue(32, 0);
    EnsureTownies(vm, house, log); // FIX(sims) QA3/QA4: the generated townies of the expansion towns
    if (log) {
        snprintf(buf, sizeof buf, "house %d: visit by family #%d (Sim %08X, transit info %d); the lot spawns them",
                 house, family->ChunkID, gs.DowntownSimGUID, gs.LotTransitInfo);
        log->push_back(buf);
    }
    return result;
}

// FIX(sims) QA3/QA4: the townies Sims.exe generates (see the header). The generator itself is not readable (SafeDisc);
// what the data fixes is used: the family numbers the expansion controllers select (Global.iff BCON 0x10F 'Special
// Family Numbers'; the ped markers' 'Can this person visit?'), the TemplateNPCs templates, the name lists in
// GameData/Live.iff (STR# 150/151 'Datable NPC Name', 160-164 'Tourist ...', 165-168 'Studiotown Celeb/Fan Name',
// 181/182 'Magic Town ... First Names') and the templates' own 'init tree' (jobs, interests, personality of
// families >= 2000). How many of each are made is this port's choice (documented in STUBS_expansions.md).
namespace {
using FSO::Content::TS1::TS1CharacterFiles;
namespace CAS = FSO::Content::TS1::CAS;

// FIX(sims) QA4: GameData/Live.iff STR# name lists, read once.
const std::vector<std::string>& LiveNames(int id)
{
    static std::map<int, std::vector<std::string>> cache;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        auto c = FSO::Content::Content::Get();
        std::vector<uint8_t> raw;
        std::vector<FSO::Content::TS1::RawIffChunk> chunks;
        if (c != nullptr) {
            for (auto name : { "/GameData/Live.iff", "/GameData/live.iff", "/GameData/LIVE.IFF" })
                if (FSO::Content::TS1::RawIff::ReadFile(c->SimsCompleteDir + name, raw)) break;
        }
        if (!raw.empty() && FSO::Content::TS1::RawIff::Parse(raw, chunks)) {
            for (int k : { 150, 151, 160, 161, 162, 163, 164, 165, 166, 167, 168, 181, 182 })
                if (auto ch = FSO::Content::TS1::RawIff::Find(chunks, "STR#", (uint16_t)k)) {
                    auto v = FSO::Content::TS1::RawIff::ReadSTRValues(ch->Data);
                    std::vector<std::string> names;
                    for (auto& n : v) {
                        auto a = n.find_first_not_of(' '), b = n.find_last_not_of(' ');
                        if (a != std::string::npos) names.push_back(n.substr(a, b - a + 1));
                    }
                    cache[k] = names;
                }
        }
    }
    static const std::vector<std::string> none;
    auto it = cache.find(id);
    return it == cache.end() ? none : it->second;
}

struct TownieMaker {
    VM* vm = nullptr;
    FSO::Content::TS1::TS1NeighborhoodProvider* prov = nullptr;
    std::set<uint32_t> avoid;
    std::map<int16_t, int> perFamily;
    int made = 0;

    int rnd(int n) { return n <= 0 ? 0 : (int)vm->Context->NextRandom((uint64_t)n); }
    std::set<std::string> usedNames;
    std::string PickName(int listId, const char* fallback) {
        auto& l = LiveNames(listId);
        if (l.empty()) return fallback;
        std::string n = l[(size_t)rnd((int)l.size())];
        for (int tries = 0; tries < 8 && usedNames.count(n); tries++) n = l[(size_t)rnd((int)l.size())]; // no twins in one batch
        usedNames.insert(n);
        return n;
    }
    bool Init(VM* v) {
        vm = v;
        prov = VMTS1Neighbourhood::Provider();
        if (vm == nullptr || prov == nullptr || prov->Neighbors == nullptr) return false;
        for (auto& n : prov->Neighbors->Entries) {
            if (n->Unknown1 != 1) continue;
            avoid.insert(n->GUID);
            if (n->PersonData.size() > 61) perFamily[n->PersonData[61]]++;
        }
        return true;
    }
    static CAS::CASCatalog* Catalog() {
        static CAS::CASCatalog catalog;
        if (!catalog.Built() && !catalog.Build()) return nullptr;
        return &catalog;
    }
    /// A random CAS look / personality for `sim` (age, gender, skin set by the caller). False without avatar data.
    bool RandomLook(CAS::CASSimDesc& sim) {
        auto cat = Catalog();
        if (cat == nullptr) return false;
        auto& heads = cat->Heads(sim.Age, sim.Gender, sim.Skin);
        auto& bodies = cat->Bodies(sim.Age, sim.Gender, sim.Skin);
        if (heads.empty() || bodies.empty()) return false;
        sim.Head = heads[(size_t)rnd((int)heads.size())];
        sim.Body = bodies[(size_t)rnd((int)bodies.size())];
        return true;
    }
    void RandomPersonality(CAS::CASSimDesc& sim) {
        int points = CAS::PersonalityPoints;
        for (int t = 0; t < CAS::TraitCount; t++) sim.Traits[t] = 0;
        while (points > 0) { int t = rnd(CAS::TraitCount); if (sim.Traits[t] < CAS::TraitMaxPoints) { sim.Traits[t]++; points--; } }
        sim.InitTraits([this](int n) { return rnd(n); });
    }
    /// The Hot Date+ wardrobe every new adult gets (body strings 30..33, as Make New Character).
    static void AddWardrobe(std::vector<std::string>& bs, bool female, const std::string& bodyType, CAS::CASSkin skin) {
        std::string code = std::string(female ? "f" : "m") + "a" + bodyType;
        std::string sk = CAS::SkinCode(skin);
        if (bs.size() < 34) bs.resize(34);
        bs[30] = "f100" + code + "_original,BODY=f100" + code + sk + "_original";
        bs[31] = "s100" + code + "_original,BODY=s100" + code + sk + "_original";
        bs[32] = "l100" + code + "_original,BODY=l100" + code + sk + "_original";
        bs[33] = "w100" + code + "_original,BODY=w100" + code + sk + "_original";
    }
    /// Writes the character file + neighbour record. `bodyStrings` empty = the CAS look of `sim`.
    FSO::Files::Formats::IFF::Chunks::Neighbour* Make(uint32_t templateGUID, const CAS::CASSimDesc& sim, int16_t family,
                                                       std::vector<std::string> bodyStrings, std::vector<int16_t> pd) {
        TS1CharacterFiles::NewCharacter nc;
        nc.TemplateGUID = templateGUID;
        nc.ForcePerson = true;
        nc.FirstName = sim.FirstName;
        if (bodyStrings.empty()) {
            nc.BodyStrings = sim.BodyStrings();
            if (sim.Age == CAS::CASAge::Adult) AddWardrobe(nc.BodyStrings, sim.Gender == CAS::CASGender::Female, sim.BodyType(), sim.Skin);
        } else nc.BodyStrings = bodyStrings;
        if (pd.empty()) pd = sim.PersonData(family);
        pd.resize(88, 0);
        nc.GUID = TS1CharacterFiles::NewGUID([this]() { return (uint32_t)vm->Context->NextRandom(0xFFFFFFFFull); }, avoid);
        if (nc.GUID == 0) return nullptr;
        auto userName = TS1CharacterFiles::NextUserName(prov->UserPath + "/Characters", TS1CharacterFiles::NeighbourNames(prov));
        std::string err;
        if (userName.empty() || TS1CharacterFiles::WriteCharacter(prov->UserPath, nc, userName, &err).empty()) {
            fprintf(stderr, "townies: %s\n", err.c_str());
            return nullptr;
        }
        auto n = TS1CharacterFiles::AddNeighbour(prov, nc.GUID, userName, pd);
        if (n == nullptr) return nullptr;
        avoid.insert(nc.GUID);
        perFamily[family]++;
        made++;
        return n;
    }
    /// An adult from a 'Generic' template with a random look (Superstar, Magic Town, generic downtownies, tourists).
    FSO::Files::Formats::IFF::Chunks::Neighbour* MakeRandom(uint32_t templateGUID, int16_t family, CAS::CASAge age,
                                                             int maleNames, int femaleNames, const std::string& surname = "",
                                                             const std::function<void(std::vector<int16_t>&)>& pdEdit = nullptr) {
        CAS::CASSimDesc sim;
        sim.Age = age;
        sim.Gender = rnd(2) ? CAS::CASGender::Female : CAS::CASGender::Male;
        sim.Skin = (CAS::CASSkin)rnd(3);
        if (!RandomLook(sim)) return nullptr;
        RandomPersonality(sim);
        bool f = sim.Gender == CAS::CASGender::Female;
        sim.FirstName = PickName(f ? femaleNames : maleNames, f ? "Jane" : "John");
        if (!surname.empty()) sim.FirstName += " " + surname;
        auto pd = sim.PersonData(family);
        pd.resize(88, 0);
        if (pdEdit) pdEdit(pd);
        return Make(templateGUID, sim, family, {}, pd);
    }
};
} // namespace

int VMTS1Travel::EnsureStudioTownies(VM* vm, std::vector<std::string>* log)
{
    static const uint32_t SS_GENERIC_GUID = 0x6BD3636A; // TemplateNPCs/SSCelebGeneric.iff
    static const int16_t FAMILY_CELEB = 5000, FAMILY_FAN = 5500;
    auto content = FSO::Content::Content::Get();
    if (content == nullptr || content->WorldObjects->Get(SS_GENERIC_GUID, true) == nullptr) return 0; // no Superstar
    TownieMaker tm;
    if (!tm.Init(vm)) return 0;
    if (tm.perFamily[FAMILY_FAN] >= STUDIO_FANS && tm.perFamily[FAMILY_CELEB] >= STUDIO_CELEBS) return 0;
    // FIX(sims) QA4: names from Live.iff STR# 165/166 (celebrities) and 167/168 (fans) instead of invented ones.
    while (tm.perFamily[FAMILY_FAN] < STUDIO_FANS)
        if (tm.MakeRandom(SS_GENERIC_GUID, FAMILY_FAN, CAS::CASAge::Adult, 167, 168) == nullptr) break;
    while (tm.perFamily[FAMILY_CELEB] < STUDIO_CELEBS) {
        auto n = tm.MakeRandom(SS_GENERIC_GUID, FAMILY_CELEB, CAS::CASAge::Adult, 165, 166, "", [&tm](std::vector<int16_t>& pd) {
            static const int16_t scores[] = { 2, 9, 21, 35, 52, 78, 156, 300, 550, 900 }; // GameData/fame.iff STR#5
            int16_t level = (int16_t)(4 + tm.rnd(5)); // 2 .. 4 stars (half stars)
            pd[80] = (int16_t)(scores[level - 1] + tm.rnd(10)); pd[81] = level; pd[82] = level;
        });
        if (n == nullptr) break;
    }
    if (log && tm.made > 0) {
        char buf[160];
        snprintf(buf, sizeof buf, "Studio Town townies: %d created (now %d fans, %d celebrities)", tm.made,
                 tm.perFamily[FAMILY_FAN], tm.perFamily[FAMILY_CELEB]);
        log->push_back(buf);
    }
    return tm.made;
}

// FIX(sims) QA4: Hot Date downtownies (family 2000). Downtown's ped markers ('Can this person visit?', ExpansionPack3
// PedMarkers.iff) bring any adult who lives in a house plus neighbours of family 2000; the Unleashed visitor trolley
// brings family 2000 and the tourists 3000..3009. TemplateNPCs has six archetypes (Bombshell, DonJuan, Fatale,
// LoungeL, Party, WallFlower) in six looks each (F/M x Dark/Light/Medium, OBJD type 1024: their own outfit, fixed
// personality in 'Main', interests + job 15/3 in 'init tree' when family == 2000) and HDT_Generic (type 512: random
// personality/job in 'init tree' for any family >= 2000).
int VMTS1Travel::EnsureDowntownies(VM* vm, std::vector<std::string>* log)
{
    static const int16_t FAMILY_DOWNTOWN = 2000;
    static const uint32_t HDT_GENERIC_GUID = 0x4FD5DD92;
    // HDT_<kind>_<FD FL FM MD ML MM>.iff
    static const uint32_t Archetypes[6][6] = {
        { 0x8C340B32, 0x4FAEDC1B, 0x8EE50B33, 0x8E380B32, 0xE25BDC65, 0x88980B33 }, // Bombshell
        { 0x42640F64, 0x6DBBDC45, 0x4CE70F65, 0x43BF0F64, 0xA45DDCE9, 0x4E7D0F65 }, // DonJuan
        { 0xA3050F7C, 0x1EF6DCCA, 0xAECF0F7D, 0xAE600F7D, 0x6026DCD8, 0xA97F0F7A }, // Fatale
        { 0xEB4A0F4A, 0x0083DD03, 0xF6480F4B, 0xF4540F4A, 0x553CDD1E, 0xF7520F4B }, // LoungeL
        { 0xCD170F44, 0x9205DD6F, 0xC8350F45, 0xCF690F45, 0xCB9BDD71, 0xC9C90F45 }, // Party
        { 0x34ED0F5A, 0x7730DD59, 0x374A0F5B, 0x36910F5A, 0x4053DD55, 0x318F0F5B }, // WallFlower
    };
    static const CAS::CASSkin LookSkin[3] = { CAS::CASSkin::Dark, CAS::CASSkin::Light, CAS::CASSkin::Medium };
    auto content = FSO::Content::Content::Get();
    if (content == nullptr || content->WorldObjects->Get(HDT_GENERIC_GUID, true) == nullptr) return 0; // no Hot Date
    TownieMaker tm;
    if (!tm.Init(vm)) return 0;
    if (tm.perFamily[FAMILY_DOWNTOWN] >= DOWNTOWNIES) return 0;
    int start = tm.perFamily[FAMILY_DOWNTOWN];
    for (int i = start; i < DOWNTOWNIES && tm.perFamily[FAMILY_DOWNTOWN] < DOWNTOWNIES; i++) {
        if (i < DOWNTOWN_ARCHETYPES) {
            // one archetype of each kind per gender (kind i % 6, female for the first six), random skin tone
            int kind = i % 6, female = (i / 6) % 2 == 0 ? 1 : 0, skin = tm.rnd(3);
            uint32_t guid = Archetypes[kind][(female ? 0 : 3) + skin];
            std::vector<uint8_t> raw;
            std::vector<FSO::Content::TS1::RawIffChunk> chunks;
            std::vector<std::string> bs;
            if (content->WorldObjects->Get(guid, true) != nullptr && TS1CharacterFiles::LoadObjectIff(guid, raw) &&
                FSO::Content::TS1::RawIff::Parse(raw, chunks))
                if (auto k = FSO::Content::TS1::RawIff::Find(chunks, "STR#", 200)) bs = FSO::Content::TS1::RawIff::ReadSTRValues(k->Data);
            if (bs.size() >= 15) {
                CAS::CASSimDesc sim;
                sim.Age = CAS::CASAge::Adult;
                sim.Gender = female ? CAS::CASGender::Female : CAS::CASGender::Male;
                sim.Skin = LookSkin[skin];
                tm.RandomPersonality(sim); // the archetype's 'Main' sets its own personality
                sim.FirstName = tm.PickName(female ? 151 : 150, female ? "Jane" : "John");
                // body type from the archetype's body mesh ("B402FAfit_bmbshl01" -> "fit")
                std::string body = bs[1], type = "fit";
                auto p = body.find(female ? "FA" : "MA");
                if (p != std::string::npos && p + 5 <= body.size()) {
                    type = body.substr(p + 2, 3);
                    std::transform(type.begin(), type.end(), type.begin(), [](unsigned char ch) { return (char)std::tolower(ch); });
                }
                TownieMaker::AddWardrobe(bs, female != 0, type, sim.Skin);
                if (tm.Make(guid, sim, FAMILY_DOWNTOWN, bs, {}) != nullptr) continue;
            }
        }
        if (tm.MakeRandom(HDT_GENERIC_GUID, FAMILY_DOWNTOWN, CAS::CASAge::Adult, 150, 151) == nullptr) break;
    }
    if (log && tm.made > 0) {
        char buf[160];
        snprintf(buf, sizeof buf, "Downtown townies: %d created (now %d)", tm.made, tm.perFamily[FAMILY_DOWNTOWN]);
        log->push_back(buf);
    }
    return tm.made;
}

// FIX(sims) QA4: Vacation tourists. VacationPedMarker 'Create a Tourist Family' brings a whole family 3000 + (1..5)
// - 1 whose members exist as neighbours (no BHAV creates them); the mailbox, phone and shuttle van accept 3000..3009.
// No TemplateNPCs template is a tourist; HDT_Generic's 'init tree' handles every family >= 2000 (and PersonGlobals
// 'Init NPC Jobs' families 3000..3010), so the tourists are cloned from it with Live.iff's tourist names.
int VMTS1Travel::EnsureTourists(VM* vm, std::vector<std::string>* log)
{
    static const uint32_t HDT_GENERIC_GUID = 0x4FD5DD92;
    auto content = FSO::Content::Content::Get();
    if (content == nullptr || content->WorldObjects->Get(HDT_GENERIC_GUID, true) == nullptr) return 0;
    TownieMaker tm;
    if (!tm.Init(vm)) return 0;
    int families = 0;
    for (int f = 0; f < TOURIST_FAMILIES; f++) {
        int16_t fam = (int16_t)(3000 + f);
        if (tm.perFamily[fam] > 0) continue;
        std::string surname = tm.PickName(160, "Tourist");
        int adults = 1 + tm.rnd(2), kids = tm.rnd(3);
        if (adults + kids > 4) kids = 4 - adults;
        bool ok = true;
        for (int i = 0; i < adults + kids && ok; i++) {
            bool kid = i >= adults;
            ok = tm.MakeRandom(HDT_GENERIC_GUID, fam, kid ? CAS::CASAge::Child : CAS::CASAge::Adult,
                               kid ? 163 : 161, kid ? 164 : 162, surname) != nullptr;
        }
        if (!ok) break;
        families++;
    }
    if (log && tm.made > 0) {
        char buf[160];
        snprintf(buf, sizeof buf, "Vacation tourists: %d created in %d families", tm.made, families);
        log->push_back(buf);
    }
    return tm.made;
}

// FIX(sims) QA4: Makin' Magic townies (family 6000). PedMarkersMagic 'Can this person visit?' brings adults who live
// in a house and neighbours of family 6000 (also ClownPortal 'Only Townies'); the template is
// TemplateNPCs/MMClownieGeneric.iff (OBJD type 4096), whose 'init tree' gives family 6000 MagiCoins / ingredients,
// a job and skills. Names: Live.iff STR# 181/182.
int VMTS1Travel::EnsureMagicTownies(VM* vm, std::vector<std::string>* log)
{
    static const int16_t FAMILY_MAGIC = 6000;
    static const uint32_t MM_GENERIC_GUID = 0x22320029;
    auto content = FSO::Content::Content::Get();
    if (content == nullptr || content->WorldObjects->Get(MM_GENERIC_GUID, true) == nullptr) return 0;
    TownieMaker tm;
    if (!tm.Init(vm)) return 0;
    while (tm.perFamily[FAMILY_MAGIC] < MAGIC_TOWNIES)
        if (tm.MakeRandom(MM_GENERIC_GUID, FAMILY_MAGIC, CAS::CASAge::Adult, 181, 182) == nullptr) break;
    if (log && tm.made > 0) {
        char buf[160];
        snprintf(buf, sizeof buf, "Magic Town townies: %d created (now %d)", tm.made, tm.perFamily[FAMILY_MAGIC]);
        log->push_back(buf);
    }
    return tm.made;
}

int VMTS1Travel::EnsureTownies(VM* vm, int house, std::vector<std::string>* log)
{
    int made = 0;
    bool downtown = house >= 21 && house <= 39, vacation = house >= 40 && house <= 49, oldTown = house >= 50 && house <= 80;
    if (downtown || oldTown) made += EnsureDowntownies(vm, log);
    if (vacation || oldTown) made += EnsureTourists(vm, log);
    if (house >= 81 && house <= 89) made += EnsureStudioTownies(vm, log);
    if (house >= 90 && house <= 99) made += EnsureMagicTownies(vm, log);
    return made;
}

}}}}
