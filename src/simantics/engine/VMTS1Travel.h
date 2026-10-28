/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// TS1(sims): travelling between lots in The Sims 1 expansion packs — the engine side, and the API the
// front-end (neighbourhood / lot screens) drives.
//
// How the original does it (from the shipped BHAVs; FreeSO's VMGenericTS1Call/VMTS1LotState were the reference):
//  * At home a Sim calls a taxi / uses the phone ("Go Downtown", "Go On Vacation", "Go To Community", "Go to Studio
//    Lot", the Magic Town clown/blimp...). The plugin opens a lot selection dialog (dialog types 5 Downtown,
//    7 Vacation, 8 Neighborhood (Old Town), 11 Studiotown, 13 Magictown: the chosen house number comes back in temp 0),
//    stores companions in the caller's inventory (Manage Inventory modes 5/6: "autofollow" date / "follow home"), and
//    calls Generic Sims Call 17 "Change To Lot In Temp 0" (temp 0 = house, -1 = the family's home lot).
//    Call 17 saves each travelling Sim's motives and the clock as "sim data" tokens (inventory type 2, GUIDs 0-6
//    motives, 7/8 hour/minute, 9 family size) and asks the game to switch lots.
//  * On the destination lot the controllers / ped markers spawn the visitors: call 18 "Build The Downtown Sim" (one
//    Sim) or 26 "Build The Vacation Family" (the whole family when global 34 "Lot Transit Info" >= 1), call 19 spawns
//    the date (inventory slot 10), 21 applies the saved motives, 3 "Pull Down Taxi Dialog" ends the trip screen.
//  * Going home is call 17 with -1; at home call 20 spawns the "take back home" date (slot 11) and 21 the motives.
//    The original resumes the home lot's saved VM (the Sim is still inside its "Go Downtown" interaction); this port
//    restarts lots from their objects, so Arrive() performs that tail (place the Sim, motives, date) itself.
//
// Front-end API (the App / neighbourhood screen owns the screen flow):
//   1. Set `vm->OnRequestLotSwitch` (or poll `Content::Get()->Neighborhood->GameState.PendingLotSwitch` each frame).
//      When it fires: save the current lot if wanted, destroy the VM, then
//   2. `int16_t house = VMTS1Travel::ResolveLot(lotId)` (-1 -> the family's home), load
//      `Neighborhood->GetHousePath(house)` with VMWorldActivator::LoadFromIff into a new VM, and
//   3. call `VMTS1Travel::Arrive(vm, house)` instead of VMTS1FamilyLoader. It returns the controllable Sims (the
//      family at home, or [] on an expansion lot, where the ped markers spawn the travellers a few ticks later:
//      watch `vm->OnTS1Event(EventSelectedSimChanged, objectID)` / global 3 to select them).
//   4. Lot selection dialogs: VMDialogInfo.Operand.Type 5/7/8/11/13 -> show the lots of map
//      `LotMapForDialog(type)` (TS1NeighborhoodProvider::LotMaps; names: GetHouseNameDesc), answer the dialog with the
//      house number as ResponseText (and ResponseCode 0), or cancel with ResponseCode 1.
//   `vm->OnTS1Event` also reports Generic Sims Call 10/11 (build/buy disabled/enabled: `vm->TS1BuildBuyEnabled`),
//   37 (cancel the open pie menu) and 41 (re-render heads).
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "simantics/_files_contract.h"

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks { class FAMI; class InventoryItem; } } } } }
namespace FSO { namespace SimAntics {
class VM;
class VMEntity;
class VMAvatar;
namespace Engine { namespace Utils {

class VMTS1Travel {
public:
    enum Event : int32_t {
        EventBuildBuyChange = 1,     // value: 0 disabled, 1 enabled (FreeSO VMEventType.TS1BuildBuyChange)
        EventSelectedSimChanged = 2, // value: object id of the Sim the player now controls
        EventCancelPieMenu = 3,
        EventHeadFlush = 4,
        EventFameChanged = 5,        // value: object id whose star level changed
        EventTutorialComplete = 6,   // TS1(sims): Generic Sims Call 0, the house is no longer the tutorial house (global 26 = 0)
    };
    // inventory "sim data" slots (type 2), FreeSO VMGenericTS1Call.SaveIData
    static constexpr uint32_t SIMDATA_TYPE = 2;
    static constexpr uint32_t SLOT_HOURS = 7, SLOT_MINUTES = 8, SLOT_FAMILY_SIZE = 9, SLOT_AUTOFOLLOW = 10, SLOT_FOLLOWHOME = 11;

    /// Generic Sims Call 17: remember who travels (GameState), save their sim data and raise the switch request.
    static void RequestLotSwitch(VM* vm, VMAvatar* caller, int16_t lotId);
    /// -1 / 0 -> the active family's home house (FAMI.HouseNumber, else GameState.ReturnHouse); others unchanged.
    static int16_t ResolveLot(int16_t lotId);
    /// After LoadFromIff on the destination lot. See the header comment.
    static std::vector<VMAvatar*> Arrive(VM* vm, int16_t house, std::vector<std::string>* log = nullptr);
    /// Lot map (TS1NeighborhoodProvider::LotMaps id) for a lot selection dialog type; 0 = not a lot selection.
    static int16_t LotMapForDialog(uint8_t dialogType);

    // ---- helpers shared with the Generic Sims Calls ----
    static std::vector<FSO::Files::Formats::IFF::Chunks::InventoryItem>& InitInventory(int16_t neighbour);
    static int16_t GetSimData(int16_t neighbour, uint32_t slot, bool* found = nullptr);
    static void SaveSimData(int16_t neighbour, uint32_t slot, int16_t data);
    static void RemoveSimData(int16_t neighbour, uint32_t slot);
    /// FreeSO VMTS1LotState.ActivateFamily: global 9 + GameState.ActiveFamily.
    static void ActivateFamily(VM* vm, FSO::Files::Formats::IFF::Chunks::FAMI* family);
    /// FreeSO VMTS1LotState.VerifyFamily: spawns every RuntimeSubset member missing from the lot next to the mailbox.
    static std::vector<VMAvatar*> VerifyFamily(VM* vm);
    /// Makes `avatar` the Sim the player controls (global 3 "Selected Person ID") and raises EventSelectedSimChanged.
    static void SelectSim(VM* vm, VMAvatar* avatar);
    // FIX(sims) H1: global 3 (selected person) must name a living Sim. When the selected Sim dies or leaves (the
    // object id can then be reused - a fire left "Ash" with the dead Sim's id) the next family member on the lot is
    // selected, as the game's UI does; 0 when none is left. Called every tick on TS1 lots.
    static void KeepSelectedSim(VM* vm);
    /// Creates a person by GUID out of world (null if the GUID does not resolve to a person).
    static VMAvatar* CreatePerson(VM* vm, uint32_t guid);
    /// FIX(sims): dresses a Sim created on a lot in the outfit of its person data 8 (current outfit).
    static void WearCurrentOutfit(VMAvatar* avatar);
    static void Signal(VM* vm, int32_t evt, int32_t value);
    /// TS1(sims): Makin' Magic. The Magic Controller (0xB6C90029) holds the current family's made spells in its
    /// attributes 0..5. Load (Generic Sims Call 43) / save them for the family (TS1GameState::FamilySpells). Save is
    /// called by RequestLotSwitch; the front end calls it before saving or leaving a lot.
    static const uint32_t MAGIC_CONTROLLER_GUID = 0xB6C90029;
    static void LoadFamilySpells(VM* vm, VMEntity* controller);
    static void SaveFamilySpells(VM* vm);
    /// FIX(sims): the Makin' Magic spellbook (dialog type 12, 'Look Up Spells'): the adult spells/charms of
    /// MagicMasterSpells.iff STR#3 (ingredients "13,13,14" -> STR#2 names; the name from the entry's comment, the only
    /// English name the table has) and whether the family has made each one (Magic Controller attribute bits, spell n
    /// = attribute (n-1)/16 bit (n-1)%16, as 'Made Spell/Recipe' sets them).
    struct SpellbookEntry { std::string Name; std::vector<std::string> Ingredients; bool Made = false; };
    static std::vector<SpellbookEntry> Spellbook(VM* vm);
    /// FIX(sims): LoadFamilySpells into the lot's Magic Controller once the family is active.
    static void ApplyFamilySpells(VM* vm);
    /// FIX(sims): pushes the first interaction whose action BHAV label contains `label` onto `sim` (true if found).
    static bool PushTreeByLabel(VM* vm, VMAvatar* sim, const std::string& label, std::vector<std::string>* log = nullptr);
    /// FIX(sims) QA3: the Superstar townies. Studio Town's 'Controller - Celebrity Spawn' (ControllerStudioLot.iff
    /// 'Spawn Fan' / 'Spawn Celebrity', CarStudio / CarLimo* 'can visit?') only bring neighbours of the special family
    /// numbers 5500 (fans) and 5000 (celebrities) (GameData Global.iff BCON 0x10F 'Special Family Numbers' 13/12).
    /// No BHAV ever creates such neighbours: Sims.exe generates them from TemplateNPCs/SSCelebGeneric.iff (like the
    /// Hot Date / Makin' Magic 'Generic' templates). This tops the neighbourhood up to FANS fans and CELEBS
    /// celebrities (2..4 stars) with a random look; returns how many were created.
    static constexpr int STUDIO_FANS = 16, STUDIO_CELEBS = 8;
    static int EnsureStudioTownies(VM* vm, std::vector<std::string>* log = nullptr);
    /// FIX(sims) QA4: the other generated townies (same mechanism, shared helper; see STUBS_expansions.md "QA4"):
    /// Hot Date downtownies (family 2000: one of each TemplateNPCs HDT_ archetype per gender + HDT_Generic ones),
    /// Vacation tourist families 3000..3004 (HDT_Generic, adults + children), Magic Town townies (family 6000,
    /// MMClownieGeneric). Names from GameData/Live.iff. Topped up once per neighbourhood, saved with it.
    static constexpr int DOWNTOWNIES = 20, DOWNTOWN_ARCHETYPES = 12, TOURIST_FAMILIES = 5, MAGIC_TOWNIES = 16;
    static int EnsureDowntownies(VM* vm, std::vector<std::string>* log = nullptr);
    static int EnsureTourists(VM* vm, std::vector<std::string>* log = nullptr);
    static int EnsureMagicTownies(VM* vm, std::vector<std::string>* log = nullptr);
    /// Everything the town of lot `house` needs (Downtown 21-39, Vacation 40-49, Old Town 50-80: downtownies +
    /// tourists for the visitor trolley, Studio Town 81-89, Magic Town 90-99).
    static int EnsureTownies(VM* vm, int house, std::vector<std::string>* log = nullptr);
};

}}}}
