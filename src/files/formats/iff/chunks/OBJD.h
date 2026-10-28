/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/formats/iff/chunks/OBJD.cs
#pragma once
#include "files/formats/iff/AbstractIffChunk.h"

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

/// <summary>
/// Type of OBJD.
/// </summary>
enum class OBJDType : uint16_t
{
    Unknown = 0,
    //Character or NPC
    Person = 2,
    //Buyable objects
    Normal = 4,
    //Roaches, Stoves2, TrClownGen, AnimTester, HelpSystem, JobFinder, NPCController, Stoves,
    //Tutorial, VisitGenerator, phonecall, unsnacker, CCPhonePlugin, EStove
    SimType = 7,
    //Stairs, doors, pool diving board & ladder, windows(?)
    Portal = 8,
    Cursor = 9,
    PrizeToken = 10,
    //Temporary location for drop or shoo
    Internal = 11,
    //these are mysteriously set as global sometimes. (TS1(sims): FreeSO OBJDType.GiftToken)
    GiftToken = 12,
    Food = 34
};

/// <summary>
/// This is an object definition, the main chunk for an object and the first loaded by the VM.
/// There can be multiple master OBJDs in an IFF, meaning that one IFF file can define multiple objects.
/// </summary>
class OBJD : public IffChunk
{
public:
    IFF_CHUNK_TYPE("OBJD")

    uint32_t Version = 0;

    static const char* const VERSION_142_Fields[];
    static const size_t VERSION_142_Fields_Length;

    uint16_t GUID1() const { return (uint16_t)(GUID); }
    void GUID1(uint16_t value) { GUID = (GUID & 0xFFFF0000) | value; }
    uint16_t GUID2() const { return (uint16_t)(GUID>>16); }
    void GUID2(uint16_t value) { GUID = (GUID & 0x0000FFFF) | ((uint32_t)value<<16); }

    uint16_t StackSize = 0;
    uint16_t BaseGraphicID = 0;
    uint16_t NumGraphics = 0;
    uint16_t TreeTableID = 0;
    int16_t InteractionGroupID = 0;
    OBJDType ObjectType = OBJDType::Unknown;
    uint16_t MasterID = 0;
    int16_t SubIndex = 0;
    uint16_t AnimationTableID = 0;
    uint32_t GUID = 0;
    uint16_t Disabled = 0;
    uint16_t BHAV_Portal = 0;
    uint16_t Price = 0;
    uint16_t BodyStringID = 0;
    uint16_t SlotID = 0;
    uint16_t SalePrice = 0;
    uint16_t InitialDepreciation = 0;
    uint16_t DailyDepreciation = 0;
    uint16_t SelfDepreciating = 0;
    uint16_t DepreciationLimit = 0;
    uint16_t RoomFlags = 0;
    uint16_t FunctionFlags = 0;
    uint16_t CatalogStringsID = 0;

    uint16_t BHAV_MainID = 0;
    uint16_t BHAV_GardeningID = 0;
    uint16_t BHAV_WashHandsID = 0;
    uint16_t BHAV_AllowIntersectionID = 0;
    uint16_t UsesFnTable = 0;
    uint16_t BitField1 = 0;

    uint16_t BHAV_PrepareFoodID = 0;
    uint16_t BHAV_CookFoodID = 0;
    uint16_t BHAV_PlaceSurfaceID = 0;
    uint16_t BHAV_DisposeID = 0;
    uint16_t BHAV_EatID = 0;
    uint16_t BHAV_PickupFromSlotID = 0;
    uint16_t BHAV_WashDishID = 0;
    uint16_t BHAV_EatSurfaceID = 0;
    uint16_t BHAV_SitID = 0;
    uint16_t BHAV_StandID = 0;

    uint16_t Global = 0;
    uint16_t BHAV_Init = 0;
    uint16_t BHAV_Place = 0;
    uint16_t BHAV_UserPickup = 0;
    uint16_t WallStyle = 0;
    uint16_t BHAV_Load = 0;
    uint16_t BHAV_UserPlace = 0;
    uint16_t ObjectVersion = 0;
    uint16_t BHAV_RoomChange = 0;
    uint16_t MotiveEffectsID = 0;
    uint16_t BHAV_Cleanup = 0;
    uint16_t BHAV_LevelInfo = 0;
    uint16_t CatalogID = 0;

    uint16_t BHAV_ServingSurface = 0;
    uint16_t LevelOffset = 0;
    uint16_t Shadow = 0;
    uint16_t NumAttributes = 0;

    uint16_t BHAV_Clean = 0;
    uint16_t BHAV_QueueSkipped = 0;
    uint16_t FrontDirection = 0;
    uint16_t BHAV_WallAdjacencyChanged = 0;
    uint16_t MyLeadObject = 0;
    uint16_t DynamicSpriteBaseId = 0;
    uint16_t NumDynamicSprites = 0;

    uint16_t ChairEntryFlags = 0;
    uint16_t TileWidth = 0;
    uint16_t LotCategories = 0;
    uint16_t BuildModeType = 0;
    uint16_t OriginalGUID1 = 0;
    uint16_t OriginalGUID2 = 0;
    uint16_t SuitGUID1 = 0;
    uint16_t SuitGUID2 = 0;
    uint16_t BHAV_Pickup = 0;
    uint16_t ThumbnailGraphic = 0;
    uint16_t ShadowFlags = 0;
    uint16_t FootprintMask = 0;
    uint16_t BHAV_DynamicMultiTileUpdate = 0;
    uint16_t ShadowBrightness = 0;
    uint16_t BHAV_Repair = 0;

    uint16_t WallStyleSpriteID = 0;
    uint16_t RatingHunger = 0;
    uint16_t RatingComfort = 0;
    uint16_t RatingHygiene = 0;
    uint16_t RatingBladder = 0;
    uint16_t RatingEnergy = 0;
    uint16_t RatingFun = 0;
    uint16_t RatingRoom = 0;
    uint16_t RatingSkillFlags = 0;

    std::vector<uint16_t> RawData;


    bool IsMaster() const
    {
        return !IsMultiTile();
    }

    bool IsMultiTile() const
    {
        return MasterID != 0;
    }

    /// C# uses reflection (GetProperty(name)); here a static name table. Throws std::runtime_error for unknown names.
    int32_t GetPropertyByName(const std::string& name) const;
    void SetPropertyByName(const std::string& name, int32_t value);

    void Read(IffFile* iff, st::Reader& stream) override;

    bool Write(IffFile* iff, st::Writer& stream) override;
};

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
