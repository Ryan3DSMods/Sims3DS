/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.Content.TS1.TS1NeighborhoodProvider (SimsVille/ContentManager/other/TS1NeighbourProvider.cs)
//
// Provides families, neighbors, neighborhood structure and more given a current userdata folder.
//
// The C# members (MainResource, Neighbors, Neighborhood, TypeAttributes, FamilyForHouse, GameState,
// GetNeighborByID, GetFamilyForHouse, GetNeighborIDForGUID, SetToNext, Save*, GetHouse*, GetTATT/SetTATT,
// GetHouseNameDesc) are translated. FreeSims never filled FamilyForHouse/ZoningDictionary, never loaded
// LotLocations/the description iffs and left "todo: manage avatar iffs here"; those parts are new and are
// marked `TS1(sims)` (see src/simantics/STUBS_neighbourhood.md).
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <tuple>
#include "files/formats/iff/IffFile.h"
#include "files/formats/iff/chunks/NBRS.h"
#include "files/formats/iff/chunks/NGBH.h"
#include "files/formats/iff/chunks/FAMI.h"
#include "files/formats/iff/chunks/FAMs.h"
#include "files/formats/iff/chunks/TTAT.h"
#include "files/formats/iff/chunks/BMP.h"
#include "files/formats/iff/chunks/STR.h"

namespace FSO { namespace Content {
class Content;
namespace TS1 {

using FSO::Files::Formats::IFF::IffFile;
using FSO::Files::Formats::IFF::Chunks::Neighbour;
using FSO::Files::Formats::IFF::Chunks::NBRS;
using FSO::Files::Formats::IFF::Chunks::NGBH;
using FSO::Files::Formats::IFF::Chunks::FAMI;
using FSO::Files::Formats::IFF::Chunks::TATT;
using FSO::Files::Formats::IFF::Chunks::BMP;

class TS1GameState
{
public:
    FAMI* ActiveFamily = nullptr;
    uint32_t DowntownSimGUID = 0;
    int16_t LotTransitInfo = 0;

    // ---- TS1(sims): travel between lots (see simantics/engine/VMTS1Travel.h for the front-end API) ----
    /// Lot the VM asked to switch to (Generic Sims Call 17 "Change To Lot In Temp 0"), -1 = none pending.
    /// The front-end polls this (or uses VM::OnRequestLotSwitch), loads the lot and calls VMTS1Travel::Arrive.
    int16_t PendingLotSwitch = -1;
    /// House the travelling family came from (its home lot, or the lot it left), 0 = unknown.
    int16_t ReturnHouse = 0;
    /// House the family is currently visiting (0 = at home).
    int16_t VisitingHouse = 0;
    /// FIX(sims): the calendar date when the travellers left (day 0 = none): the next lot continues from it, so the
    /// home lot and the community lots no longer show different days of the month.
    int32_t TravelDay = 0, TravelMonth = 0, TravelYear = 0;
    /// TS1(sims): Makin' Magic - the spells/recipes each family has made: the Magic Controller's attributes 0..5
    /// (bit sets, MagicMasterSpells 'Made Spell/Recipe'), keyed by family number. Generic Sims Call 43 copies them into
    /// the controller of a lot; VMTS1Travel::SaveFamilySpells copies them back. FIX(sims): saved in Neighborhood.iff as private
    /// "FSPL" chunks (TS1NeighbourhoodWriter; where the original saves them is not known).
    std::map<int16_t, std::vector<int16_t>> FamilySpells;
};

/// <summary>
/// Provides families, neighbors, neighborhood structure and more given a current userdata folder. Should also allow the game to save these things.
/// </summary>
class TS1NeighborhoodProvider
{
public:
    std::unique_ptr<IffFile> MainResource;
    std::unique_ptr<IffFile> LotLocations;
    std::unique_ptr<IffFile> StreetNames;
    std::unique_ptr<IffFile> NeighbourhoodDesc;
    std::unique_ptr<IffFile> STDesc;
    std::unique_ptr<IffFile> MTDesc;
    std::unique_ptr<IffFile> DTDesc; // TS1(sims): Hot Date downtown lot names (TemplateDowntown/DTDesc.iff)
    std::unique_ptr<IffFile> VIDesc; // TS1(sims): Vacation island lot names (TemplateVacation/VIDesc.iff)
    std::unordered_map<int16_t, int16_t> ZoningDictionary;
    NBRS* Neighbors = nullptr;
    NGBH* Neighborhood = nullptr;
    TATT* TypeAttributes = nullptr;
    std::unordered_map<int16_t, FAMI*> FamilyForHouse;
    Content* ContentManager;
    TS1GameState GameState;
    std::string UserPath;
    /// TS1(sims): folder holding UserData, UserData2, ... (the front end's save area, see
    /// src/app/front/UserDataSetup.h). Empty = the install tree (Content::SimsCompleteDir), as before.
    std::string UserDataRoot;

    std::unordered_set<uint32_t> DirtyAvatars;

    explicit TS1NeighborhoodProvider(Content* contentManager);

    /// <summary>
    /// Intializes a specific neighbourhood. Also counts as a save discard, since it unloads the current neighbourhood.
    /// </summary>
    void InitSpecific(int32_t id);

    Neighbour* GetNeighborByID(int16_t ID);
    FAMI* GetFamilyForHouse(int16_t ID);
    /// C# `short?` — returns false for null.
    bool GetNeighborIDForGUID(uint32_t GUID, int16_t& result);
    int16_t SetToNext(int16_t current);
    int16_t SetToNext(int16_t current, uint32_t guid);

    bool SaveNeighbourhood(bool withSims);
    bool SaveHouse(int32_t houseID, IffFile* file);
    std::unique_ptr<IffFile> GetHouse(int32_t id);
    std::string GetHousePath(int32_t id);
    /// C# returns the BMP of a freshly opened house iff; here the house iff is cached so the chunk stays valid.
    BMP* GetHouseThumb(int32_t id);
    int16_t GetTATT(uint32_t guid, int32_t index);
    std::pair<std::string, std::string> GetHouseNameDesc(int32_t houseID);
    void SetTATT(uint32_t guid, int32_t index, int16_t value);

    // ---- TS1(sims): additions (no FreeSims code behind these) ----

    /// Person data index of the TS1 "family number" (Behavior.iff STR#200 'Person Data Labels' #61).
    static constexpr int32_t PERSON_FAMILY_NUMBER = 61;
    /// Person data entries the TS1 neighbour record stores (NBRS version 4: 0xA0 bytes = 80 shorts).
    static constexpr int32_t TS1_PERSON_DATA_COUNT = 80;

    /// Family by its chunk id (the "family number" in person data 61 and in global 9 "Current Family").
    FAMI* GetFamily(int16_t familyNumber);
    /// Family name: the FAMs string table with the same chunk id as the FAMI.
    std::string GetFamilyName(int16_t familyNumber);
    /// Families in chunk-id order (FAMI #0 is the empty/townie family).
    std::vector<FAMI*> ListFamilies();
    /// House a neighbour lives in (its family's FAMI.HouseNumber), 0 if none.
    int16_t GetHouseOfNeighbor(Neighbour* neigh);
    /// Zoning of a lot: 0 residential, 1 community (LotZoning.iff / MasterLotZoning.iff). Unknown lots: 0.
    int16_t GetZoning(int16_t houseNumber);
    /// "Set to Next: Closest House" — next house number after `current` in order of increasing distance
    /// from `fromHouse` on the neighbourhood map (LotLocations.iff). -1 when the list is exhausted.
    int16_t SetToNextClosestHouse(int16_t fromHouse, int16_t current);
    /// Character iff path for a neighbour (UserData/Characters/<Name>.iff), "" if absent.
    std::string FindCharacterFile(const std::string& neighbourName);
    /// Registers every neighbour's character iff (User*.iff) with WorldObjects so their GUIDs create avatars.
    int32_t LoadCharacters();

    std::vector<std::string> CharacterFiles; // character iffs registered by LoadCharacters

    /// TS1(sims): the expansion packs' template folders, newest first (TemplateMagicTown, TemplateStudiotown,
    /// TemplateCommunity, TemplateVacation, TemplateDowntown, TemplateUserData). The original installers copy these
    /// into every neighbourhood (UserData*/Houses/House21..99.iff, LotZoning/LotLocations/StreetNames, the *Desc
    /// lot names and the downtown townies of TemplateNPCs into Characters/). This port reads them in place: a file
    /// missing from the user folder is taken from the newest template that has it (nothing is written).
    std::vector<std::string> TemplateDirs;
    /// UserPath/rel if it exists, else <template>/<file name of rel> (newest template first), else "".
    std::string FindUserOrTemplate(const std::string& rel);
    /// True if house `id` exists for this neighbourhood (user file or expansion template).
    bool HouseExists(int32_t id);
    /// Lot kind by house number range, as the expansion packs number them: 0 home lot (1-20), 1 downtown (Hot Date,
    /// 21-39), 2 vacation (40-49), 3 Old Town / community (Unleashed, 50-80), 4 Studio Town (Superstar, 81-89),
    /// 5 Magic Town (Makin' Magic, 90-99). Zoning (residential/community) is separate: see GetZoning.
    static int16_t LotKindOfHouse(int32_t id);
    /// Template NPC characters (TemplateNPCs/*.iff: the Hot Date downtownies) registered by LoadCharacters.
    std::vector<std::string> TemplateCharacterFiles;
    /// TS1(sims): per-neighbour inventories (NGBH, Hot Date+): FreeSO GetInventoryByNID / SetInventoryForNID.
    /// Creates an empty NGBH chunk when the neighbourhood has none (pre-Hot Date neighbourhoods).
    std::vector<FSO::Files::Formats::IFF::Chunks::InventoryItem>* GetInventoryByNID(int16_t id);
    std::vector<FSO::Files::Formats::IFF::Chunks::InventoryItem>& InitInventoryForNID(int16_t id);
    /// FreeSO GetMagicoinsForNeighbor: Makin' Magic's MagiCoin token count.
    int32_t GetMagicoinsForNeighbor(int16_t id);
    std::unordered_map<int16_t, std::pair<int32_t, int32_t>> LotPositions; // house -> neighbourhood screen position
    /// TS1(sims): every map of LotLocations.iff by STR# id (1 original neighbourhood, 2 Hot Date downtown,
    /// 3 Vacation island, 4 Unleashed Old Town (incl. the home lots' positions on that map), 5 Studio Town,
    /// 7 Magic Town; 6 = "Filter Bar Settings", not a map). Each entry: (house, x, y) in neighbourhood screen pixels.
    std::map<uint16_t, std::vector<std::tuple<int16_t, int32_t, int32_t>>> LotMaps;

private:
    std::unique_ptr<TATT> OwnedTypeAttributes; // used when Neighborhood.iff has no TATT chunk
    std::unique_ptr<NGBH> OwnedNeighborhood;   // TS1(sims): used when Neighborhood.iff has no NGBH chunk
    std::unordered_map<int32_t, std::unique_ptr<IffFile>> HouseThumbCache;
    std::unique_ptr<IffFile> TryOpen(const std::string& path);
    void LoadZoning();
    void LoadLotLocations();
};

}}}
