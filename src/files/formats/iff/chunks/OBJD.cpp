/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/formats/iff/chunks/OBJD.cs
#include "files/formats/iff/chunks/OBJD.h"
#include "files/formats/iff/IffFile.h"
#include <stdexcept>
#include <cstring>

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

const char* const OBJD::VERSION_142_Fields[] =
{
    "StackSize",
    "BaseGraphicID",
    "NumGraphics",
    "BHAV_MainID",
    "BHAV_GardeningID",
    "TreeTableID",
    "InteractionGroupID",
    "ObjectType",
    "MasterID",
    "SubIndex",
    "BHAV_WashHandsID",
    "AnimationTableID",
    "GUID1",
    "GUID2",
    "Disabled",
    "BHAV_Portal",
    "Price",
    "BodyStringID",
    "SlotID",
    "BHAV_AllowIntersectionID",
    "UsesFnTable",
    "BitField1",
    "BHAV_PrepareFoodID",
    "BHAV_CookFoodID",
    "BHAV_PlaceSurfaceID",
    "BHAV_DisposeID",
    "BHAV_EatID",
    "BHAV_PickupFromSlotID",
    "BHAV_WashDishID",
    "BHAV_EatSurfaceID",
    "BHAV_SitID",
    "BHAV_StandID",

    "SalePrice",
    "InitialDepreciation",
    "DailyDepreciation",
    "SelfDepreciating",
    "DepreciationLimit",
    "RoomFlags",
    "FunctionFlags",
    "CatalogStringsID",

    "Global",
    "BHAV_Init",
    "BHAV_Place",
    "BHAV_UserPickup",
    "WallStyle",
    "BHAV_Load",
    "BHAV_UserPlace",
    "ObjectVersion",
    "BHAV_RoomChange",
    "MotiveEffectsID",
    "BHAV_Cleanup",
    "BHAV_LevelInfo",
    "CatalogID",
    "BHAV_ServingSurface",
    "LevelOffset",
    "Shadow",
    "NumAttributes",

    "BHAV_Clean",
    "BHAV_QueueSkipped",
    "FrontDirection",
    "BHAV_WallAdjacencyChanged",
    "MyLeadObject",
    "DynamicSpriteBaseId",
    "NumDynamicSprites",

    "ChairEntryFlags",
    "TileWidth",
    "LotCategories",
    "BuildModeType",
    "OriginalGUID1",
    "OriginalGUID2",
    "SuitGUID1",
    "SuitGUID2",
    "BHAV_Pickup",
    "ThumbnailGraphic",
    "ShadowFlags",
    "FootprintMask",
    "BHAV_DynamicMultiTileUpdate",
    "ShadowBrightness",
    "BHAV_Repair",

    "WallStyleSpriteID",
    "RatingHunger",
    "RatingComfort",
    "RatingHygiene",
    "RatingBladder",
    "RatingEnergy",
    "RatingFun",
    "RatingRoom",
    "RatingSkillFlags",
};

const size_t OBJD::VERSION_142_Fields_Length = sizeof(OBJD::VERSION_142_Fields) / sizeof(OBJD::VERSION_142_Fields[0]);

// Reflection stand-in: table of property name -> ushort member pointer.
namespace {
    struct OBJDProp { const char* Name; uint16_t OBJD::* Field; };
    #define P(n) { #n, &OBJD::n }
    const OBJDProp OBJD_PROPS[] = {
        P(StackSize), P(BaseGraphicID), P(NumGraphics), P(TreeTableID), P(MasterID), P(AnimationTableID),
        P(Disabled), P(BHAV_Portal), P(Price), P(BodyStringID), P(SlotID), P(SalePrice), P(InitialDepreciation),
        P(DailyDepreciation), P(SelfDepreciating), P(DepreciationLimit), P(RoomFlags), P(FunctionFlags),
        P(CatalogStringsID), P(BHAV_MainID), P(BHAV_GardeningID), P(BHAV_WashHandsID), P(BHAV_AllowIntersectionID),
        P(UsesFnTable), P(BitField1), P(BHAV_PrepareFoodID), P(BHAV_CookFoodID), P(BHAV_PlaceSurfaceID),
        P(BHAV_DisposeID), P(BHAV_EatID), P(BHAV_PickupFromSlotID), P(BHAV_WashDishID), P(BHAV_EatSurfaceID),
        P(BHAV_SitID), P(BHAV_StandID), P(Global), P(BHAV_Init), P(BHAV_Place), P(BHAV_UserPickup), P(WallStyle),
        P(BHAV_Load), P(BHAV_UserPlace), P(ObjectVersion), P(BHAV_RoomChange), P(MotiveEffectsID), P(BHAV_Cleanup),
        P(BHAV_LevelInfo), P(CatalogID), P(BHAV_ServingSurface), P(LevelOffset), P(Shadow), P(NumAttributes),
        P(BHAV_Clean), P(BHAV_QueueSkipped), P(FrontDirection), P(BHAV_WallAdjacencyChanged), P(MyLeadObject),
        P(DynamicSpriteBaseId), P(NumDynamicSprites), P(ChairEntryFlags), P(TileWidth), P(LotCategories),
        P(BuildModeType), P(OriginalGUID1), P(OriginalGUID2), P(SuitGUID1), P(SuitGUID2), P(BHAV_Pickup),
        P(ThumbnailGraphic), P(ShadowFlags), P(FootprintMask), P(BHAV_DynamicMultiTileUpdate), P(ShadowBrightness),
        P(BHAV_Repair), P(WallStyleSpriteID), P(RatingHunger), P(RatingComfort), P(RatingHygiene), P(RatingBladder),
        P(RatingEnergy), P(RatingFun), P(RatingRoom), P(RatingSkillFlags),
    };
    #undef P
}

int32_t OBJD::GetPropertyByName(const std::string& name) const
{
    if (name == "GUID1") return GUID1();
    if (name == "GUID2") return GUID2();
    if (name == "GUID") return (int32_t)GUID;
    if (name == "InteractionGroupID") return InteractionGroupID;
    if (name == "SubIndex") return SubIndex;
    if (name == "ObjectType") return (int32_t)ObjectType;
    if (name == "Version") return (int32_t)Version;
    for (auto& p : OBJD_PROPS)
    {
        if (name == p.Name) return this->*(p.Field);
    }
    throw std::runtime_error("OBJD: unknown property " + name);
}

void OBJD::SetPropertyByName(const std::string& name, int32_t value)
{
    if (name == "GUID1") { GUID1((uint16_t)value); return; }
    if (name == "GUID2") { GUID2((uint16_t)value); return; }
    if (name == "GUID") { GUID = (uint32_t)value; return; }
    if (name == "InteractionGroupID") { InteractionGroupID = (int16_t)value; return; }
    if (name == "SubIndex") { SubIndex = (int16_t)value; return; }
    if (name == "ObjectType") { ObjectType = (OBJDType)value; return; }
    if (name == "Version") { Version = (uint32_t)value; return; }
    for (auto& p : OBJD_PROPS)
    {
        if (name == p.Name) { this->*(p.Field) = (uint16_t)value; return; }
    }
    throw std::runtime_error("OBJD: unknown property " + name);
}

void OBJD::Read(IffFile* iff, st::Reader& stream)
{
    (void)iff;
    st::Reader& io = stream;
    io.ByteOrder(st::Endian::Little);
    {
        this->Version = io.ReadUInt32();

        /**136 (80 fields)
            138a (95 fields) - Used for The Sims 1 base game objects?
            138b (108 fields) - Used for The Sims 1 expansion objects?
            139 (96 fields)
            140 (97 fields)
            141 (97 fields)
            142 (105 fields)**/
        int numFields = 80;
        if (Version == 138)
        {
            numFields = 95;
        }
        else if (Version == 139)
        {
            numFields = 96;
        }
        else if (Version == 140)
        {
            numFields = 97;
        }
        else if (Version == 141)
        {
            numFields = 97;
        }
        else if (Version == 142)
        {
            numFields = 105;
        }

        numFields -= 2;
        RawData = std::vector<uint16_t>(numFields);
        io.Mark();

        for (int i = 0; i < numFields; i++)
        {
            RawData[i] = io.ReadUInt16();
        }

        io.SeekFromMark(0);

        this->StackSize = io.ReadUInt16();
        this->BaseGraphicID = io.ReadUInt16();
        this->NumGraphics = io.ReadUInt16();
        this->BHAV_MainID = io.ReadUInt16();
        this->BHAV_GardeningID = io.ReadUInt16();
        this->TreeTableID = io.ReadUInt16();
        this->InteractionGroupID = io.ReadInt16();
        this->ObjectType = (OBJDType)io.ReadUInt16();
        this->MasterID = io.ReadUInt16();
        this->SubIndex = io.ReadInt16();
        this->BHAV_WashHandsID = io.ReadUInt16();
        this->AnimationTableID = io.ReadUInt16();
        this->GUID = io.ReadUInt32();
        this->Disabled = io.ReadUInt16();
        this->BHAV_Portal = io.ReadUInt16();
        this->Price = io.ReadUInt16();
        this->BodyStringID = io.ReadUInt16();
        this->SlotID = io.ReadUInt16();
        this->BHAV_AllowIntersectionID = io.ReadUInt16();
        this->UsesFnTable = io.ReadUInt16();
        this->BitField1 = io.ReadUInt16();
        this->BHAV_PrepareFoodID = io.ReadUInt16();
        this->BHAV_CookFoodID = io.ReadUInt16();
        this->BHAV_PlaceSurfaceID = io.ReadUInt16();
        this->BHAV_DisposeID = io.ReadUInt16();
        this->BHAV_EatID = io.ReadUInt16();
        this->BHAV_PickupFromSlotID = io.ReadUInt16();
        this->BHAV_WashDishID = io.ReadUInt16();
        this->BHAV_EatSurfaceID = io.ReadUInt16();
        this->BHAV_SitID = io.ReadUInt16();
        this->BHAV_StandID = io.ReadUInt16();

        this->SalePrice = io.ReadUInt16();
        this->InitialDepreciation = io.ReadUInt16();
        this->DailyDepreciation = io.ReadUInt16();
        this->SelfDepreciating = io.ReadUInt16();
        this->DepreciationLimit = io.ReadUInt16();
        this->RoomFlags = io.ReadUInt16();
        this->FunctionFlags = io.ReadUInt16();
        this->CatalogStringsID = io.ReadUInt16();

        this->Global = io.ReadUInt16();
        this->BHAV_Init = io.ReadUInt16();
        this->BHAV_Place = io.ReadUInt16();
        this->BHAV_UserPickup = io.ReadUInt16();
        this->WallStyle = io.ReadUInt16();
        this->BHAV_Load = io.ReadUInt16();
        this->BHAV_UserPlace = io.ReadUInt16();
        this->ObjectVersion = io.ReadUInt16();
        this->BHAV_RoomChange = io.ReadUInt16();
        this->MotiveEffectsID = io.ReadUInt16();
        this->BHAV_Cleanup = io.ReadUInt16();
        this->BHAV_LevelInfo = io.ReadUInt16();
        this->CatalogID = io.ReadUInt16();
        this->BHAV_ServingSurface = io.ReadUInt16();
        this->LevelOffset = io.ReadUInt16();
        this->Shadow = io.ReadUInt16();
        this->NumAttributes = io.ReadUInt16();

        this->BHAV_Clean = io.ReadUInt16();
        this->BHAV_QueueSkipped = io.ReadUInt16();
        this->FrontDirection = io.ReadUInt16();
        this->BHAV_WallAdjacencyChanged = io.ReadUInt16();
        this->MyLeadObject = io.ReadUInt16();
        this->DynamicSpriteBaseId = io.ReadUInt16();
        this->NumDynamicSprites = io.ReadUInt16();

        this->ChairEntryFlags = io.ReadUInt16();
        this->TileWidth = io.ReadUInt16();
        this->LotCategories = io.ReadUInt16();
        this->BuildModeType = io.ReadUInt16();
        this->OriginalGUID1 = io.ReadUInt16();
        this->OriginalGUID2 = io.ReadUInt16();
        this->SuitGUID1 = io.ReadUInt16();
        this->SuitGUID2 = io.ReadUInt16();
        this->BHAV_Pickup = io.ReadUInt16();
        this->ThumbnailGraphic = io.ReadUInt16();
        this->ShadowFlags = io.ReadUInt16();
        this->FootprintMask = io.ReadUInt16();
        this->BHAV_DynamicMultiTileUpdate = io.ReadUInt16();
        this->ShadowBrightness = io.ReadUInt16();

        // FIX(sims): FreeSims read BHAV_Repair unconditionally, which runs past the end of version 136 OBJDs
        // (78 fields: TS1 Cursors.iff "Tile Cursor", People\Bones.iff) and dropped those objects; FreeSO reads it
        // only when numFields > 78.
        if (numFields > 78) this->BHAV_Repair = io.ReadUInt16();

        if (numFields > 80)
        {
            this->WallStyleSpriteID = io.ReadUInt16();
            this->RatingHunger = io.ReadUInt16();
            this->RatingComfort = io.ReadUInt16();
            this->RatingHygiene = io.ReadUInt16();
            this->RatingBladder = io.ReadUInt16();
            this->RatingEnergy = io.ReadUInt16();
            this->RatingFun = io.ReadUInt16();
            this->RatingRoom = io.ReadUInt16();
            this->RatingSkillFlags = io.ReadUInt16();
        }
    }
}

bool OBJD::Write(IffFile* iff, st::Writer& stream)
{
    (void)iff;
    st::Writer& io = stream;
    io.ByteOrder(st::Endian::Little);
    {
        io.WriteUInt32(142);
        for (size_t i = 0; i < VERSION_142_Fields_Length; i++)
        {
            io.WriteUInt16((uint16_t)GetPropertyByName(VERSION_142_Fields[i]));
        }
        for (size_t i = VERSION_142_Fields_Length; i < 105; i++)
        {
            io.WriteUInt16(0);
        }
    }
    return true;
}

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
