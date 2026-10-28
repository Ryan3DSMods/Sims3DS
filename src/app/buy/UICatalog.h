/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims app — FreeSims UI/Controls/Catalog/UICatalog.cs: the buy/build catalogue.
//
// FreeSims builds its catalogue from TSO packing slips (Content/catalog.xml, ep*.xml: GUID, category, price).
// TS1(sims): The Sims has no packing slip — the catalogue is in each OBJD (FreeSO TS1ObjectProvider does the
// same): an object is for sale when (FunctionFlags > 0 || BuildModeType > 0) && Disabled == 0 && (multitile ||
// NumGraphics > 0) && (MasterID == 0 || SubIndex == -1). Buy mode sorts by FunctionFlags (bit 0 Seating, 1 Surfaces,
// 2 Appliances, 3 Electronics, 4 Plumbing, 5 Decorative, 6 Miscellaneous, 7 Lighting) or RoomFlags (bit 0 Kitchen,
// 1 Bedroom, 2 Bathroom, 3 Living Room, 4 Outside, 5 Dining Room, 6 Miscellaneous, 7 Study); FunctionSubsort picks
// the sub-category buttons (UIText.iff STR# 200-207); build mode objects use BuildModeType (1 door, 2 window,
// 3 stairs, 4 plant, 5 fireplace, 6 column, 7 pool item). An object can be in several categories (all its bits),
// lists are sorted by price (CatalogSorter).
// The scan of every object file is slow on the SD card, so the result is cached in <cacheDir>/catalog.cache
// (like objects.cache). Names, descriptions (CTSS 0/1) and thumbnails (BMP_ CatalogStringsID) are loaded lazily
// for the visible page from a throw-away copy of the object file (the 3DS cannot keep every object in memory).
// Wallpapers, floors and wall styles come from the wall/floor providers (AddWallpapers/AddFloors/AddWallStyles).
#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "app/Canvas.h"

namespace simapp {

/// C# UICatalogElement (+ the TS1 sort fields of FreeSO's ObjectCatalogItem).
struct UICatalogElement {
    enum class SpecialType : uint8_t { None, Wallpaper, Floor, WallStyle };
    uint32_t GUID = 0;
    uint32_t Price = 0;
    std::string Name;           // OBJD chunk label until the CTSS name is loaded
    uint16_t FunctionFlags = 0, RoomFlags = 0, BuildModeType = 0, Subsort = 0, CommunitySubsort = 0;
    uint16_t CatalogStringsID = 0;
    int16_t InitialDepreciation = 0, DepreciationLimit = 0;
    int32_t FarIndex = -1;      // WorldObjectProvider::Archives index (-1 loose file)
    std::string File;           // entry name (with .iff) or loose path
    SpecialType Special = SpecialType::None;
    uint16_t ResID = 0;         // wall/floor pattern id, wall style id
    uint16_t Pattern = 0;       // wall styles: draw pattern (C# WallStylePatterns)
    int16_t Ratings[7] = { 0 }; // OBJD RatingHunger..RatingRoom (catalogue info, UIText STR# 160)
    uint16_t SkillFlags = 0;    // OBJD RatingSkillFlags
};

class UICatalog {
public:
    static UICatalog& Get();
    /// Builds (or loads from <cacheDir>/catalog.cache) the object catalogue. Blocking; cheap after the first run.
    void Init(const std::string& cacheDir);
    bool Ready() const { return _Ready; }

    // Buy mode. index = UI order (0..7): Function: Seating, Surfaces, Decorative, Electronics, Appliances, Plumbing,
    // Lighting, Misc. Room: Living, Dining, Bedroom, Study, Kitchen, Bathroom, Outside, Misc.
    static const uint16_t FunctionBits[8];
    static const uint16_t RoomBits[8];
    /// subsort: -1 all, 0..3 the four sub-sort buttons, 4 = "other".
    /// FIX(sims): TS1 shows an object in the residential catalogue only when its FunctionSubsort (OBJD 92) is set and on a
    /// community lot only when its CommunitySubsort (OBJD 97) is set (community-only objects: Downtown trash cans, pay
    /// phone, dartboard, Vacation mailbox...; never: NPC pets, dragons, dead mouse, pet templates). Set before ByFunction.
    bool CommunityLot = false;
    uint16_t EffectiveSubsort(const UICatalogElement& e) const { return CommunityLot ? e.CommunitySubsort : e.Subsort; }
    std::vector<const UICatalogElement*> ByFunction(int index, int subsort) const;
    std::vector<const UICatalogElement*> ByRoom(int index) const;
    // Build mode
    std::vector<const UICatalogElement*> ByBuildType(int buildModeType) const;
    std::vector<const UICatalogElement*> Wallpapers() const;
    std::vector<const UICatalogElement*> Floors() const;
    std::vector<const UICatalogElement*> WallStyles() const;
    const UICatalogElement* ByGUID(uint32_t guid) const;

    /// Lazily loaded presentation (C# GetObjIcon + the query panel strings).
    struct Details {
        std::string Name, Description;
        Canvas Icon;            // catalogue thumbnail, magenta/corner keyed
        Canvas IconActive;      // FIX(sims) FB1: the BMP_'s active (selected) frame, empty for one-frame pictures
        bool Loaded = false;
    };
    const Details& GetDetails(const UICatalogElement& e);
    void TrimDetails(size_t keep = 96);

private:
    bool _Ready = false;
    std::vector<UICatalogElement> _Objects;
    std::vector<UICatalogElement> _Specials;
    std::map<uint64_t, Details> _Details; // key: GUID or (special << 32 | id)
    std::vector<uint64_t> _DetailOrder;
    void Scan();
    bool LoadCache(const std::string& path);
    void SaveCache(const std::string& path);
    void AddSpecials();
    static void Sort(std::vector<const UICatalogElement*>& v);
};

}
