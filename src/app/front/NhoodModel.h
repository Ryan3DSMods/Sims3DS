/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims front end — the neighbourhood as the front end sees it: the areas of the map (the neighbourhood,
// Downtown, Vacation Island, Studio Town, Magic Town), their lots (LotLocations.iff positions, house pictures,
// names, addresses, value, zoning), the families (Neighborhood.iff FAMI/FAMs), and the neighbourhood screen's
// actions: move in (with purchase), evict, delete family, switch neighbourhood, save.
//
// TS1(sims): FreeSims has no TS1 neighbourhood screen; the data comes from its TS1NeighborhoodProvider
// (content/TS1NeighbourProvider) and the rules from the game's own texts (STR#131 EvictModeStrs, #132
// MoveInModeStrs, #134 NghRollover). Neighborhood.iff is saved by rewriting only the chunks that changed (FAMI /
// FAMs) into a copy of the original file, so chunks this port does not parse survive untouched.
#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "app/Canvas.h"
#include "app/front/UserDataSetup.h"

namespace simapp { namespace front {

enum class Area { Neighbourhood = 0, Downtown, Vacation, StudioTown, MagicTown, Count };

/// Static description of an area's screen (art from Res_Nbhd.RT, lot table from LotLocations.iff).
struct AreaDef {
    Area Id;
    const char* Background;              // 800x600 map
    std::vector<const char*> Under;      // full-screen layers composited under the lots
    std::vector<const char*> Over;       // ... and over them (trees, theatre roof)
    std::vector<const char*> Anim;       // animation frames (overlays; smaller frames are aligned to the map's holes)
    int AnimMs;                          // frame time
    int LotTable;                        // LotLocations.iff STR# id
    int FirstHouse, LastHouse;
    const char* Music;                   // Music/Modes/<name>
    const char* LoadScreen;              // shown while a lot of this area loads (EPs)
    const char* TravelButton;            // banner button that leads here (NghUI/...)
    const char* ReturnButton;            // button back to the neighbourhood
    int TipString;                       // STR#151 tooltip of the travel button
};
const AreaDef& GetAreaDef(Area a, bool unleashed);

struct SimInfo {
    uint32_t GUID = 0;
    int16_t NeighbourID = 0;
    std::string Name, FirstName, Bio;
    bool Child = false, Female = false;
    bool Pet = false; // FIX(sims) FB1: STR# 200 bodystring[0] dog / cat / kat
    Canvas Face;   // the character file's portrait (BMP_ 2002 'faces', first of 3; written by the game and by CAS)
};

struct FamilyInfo {
    int16_t ChunkID = 0;
    std::string Name;
    int House = 0;
    int Budget = 0, NetWorth = 0, Friends = 0;
    int MagiCoins = 0;
    std::vector<SimInfo> Members;
};

struct LotInfo {
    int House = 0;
    int X = 0, Y = 0;          // top-left of the lot picture in 800x600 map coordinates
    bool Exists = false;       // HouseNN.iff present
    bool Community = false;    // zoning
    bool EmptyLot = false;     // no house built on it (architecture value 0)
    std::string Name, Desc, Address;
    int Value = 0;             // SIMI purchase value
    int16_t Family = -1;       // FAMI chunk id of the family living here
    Canvas Thumb;              // house picture at map scale (BMP 513 roofed / 1025 for the Unleashed map), alpha keyed
    // FIX(sims) FB1: X, Y are the picture's top-left, placed like the game (FreeSO UINeighborhoodHouseButton): the
    // LotLocations point is the lot's centre, the picture is centred on it and moved by THMB 512's offsets
    int CX = 0, CY = 0;        // LotLocations point
    int OpenDY = 0;            // the open picture (BMP 512, no roof: the selected lot) is drawn OpenDY lower (THMB AddYOff)
    int Scale = 1;             // map scale divisor (2 = the Unleashed neighbourhood)
};

class NhoodModel {
public:
    /// Points the content's neighbourhood provider at the save area and loads neighbourhood `index`.
    /// FIX(sims) SAVE: the folder's interrupted saves are repaired first (once per run: Neighborhood.iff, Houses/,
    /// Characters/ from .new / .tmp / .bak); a Neighborhood.iff that is missing or damaged with no usable backup fails
    /// with BrokenIndex = index and leaves the current neighbourhood loaded (it used to be unloaded: empty screen).
    bool Switch(int index, const UserDataSetup& setup, std::string& error);
    int Current = -1;
    int BrokenIndex = -1;
    std::string UserDir;       // save-area folder of the current neighbourhood

    /// Families (FAMI #0, the townie "Default" family, is skipped).
    std::vector<FamilyInfo> Families;
    const FamilyInfo* Family(int16_t chunkId) const;
    const FamilyInfo* FamilyInHouse(int house) const;
    std::vector<const FamilyInfo*> Bin() const; // families not living in a house

    /// Lots of an area, with their pictures (loaded here: ~100 KB read per lot).
    std::vector<LotInfo> Lots;
    Area CurrentArea = Area::Neighbourhood;
    void LoadArea(Area a);
    bool AreaAvailable(Area a) const; // the area's lots exist in this neighbourhood (expansion pack installed)
    bool Unleashed() const;           // Old Town lots present: the Unleashed map
    LotInfo* Lot(int house);

    /// Rollover text of a lot (STR#134) for the selected/moving family.
    std::string Rollover(const LotInfo& lot, const FamilyInfo* movingFamily) const;

    // ---- actions (they change the in-memory neighbourhood; Save() writes it) ----
    enum class MoveResult { Ok, CantAfford, Occupied, Community, Missing };
    MoveResult CanMoveIn(int16_t family, int house) const;
    MoveResult MoveIn(int16_t family, int house);
    bool Evict(int house);
    bool DeleteFamily(int16_t family);
    /// Updates a family's money (after playing its lot).
    void SetBudget(int16_t family, int budget);
    bool Save(std::string* error = nullptr);
    bool Dirty = false;

    /// Re-reads the family list from the provider (after CAS created a family, after a lot session).
    void RefreshFamilies();
    static std::string StreetAddress(const std::string& userDir, int house);
    /// The house's big picture (BMP_ 512, 'Thumbnail', roof on) as saved by the game.
    static bool HousePicture(const std::string& userDir, int house, Canvas& out);
    /// FIX(sims) FB1: the selected lot's open picture (BMP 512, no roof) at map scale.
    bool OpenPicture(const LotInfo& lot, Canvas& out) const;

private:
    std::vector<uint32_t> _RegisteredCharacters;
    std::map<int, std::pair<int, int>> _Positions; // house -> map position, for the current area's table
    std::vector<int16_t> _DeletedFamilies;
    std::map<int, std::pair<std::string, std::string>> _Desc; // house -> name, description
    void LoadDescriptions();
    void LoadPositions(int table);
};

}}
