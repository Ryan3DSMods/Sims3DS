/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Contract between src/simantics (this VM port) and src/files (the sims.files port, translated in
// parallel). The VM only codes against the C# names below. When the real files/ headers exist,
// build with -DST_FILES_REAL and this header turns into a set of includes; without it, the
// declarations below (fields + method signatures only, no bodies) keep the VM compiling.
//
// Expected real headers / API (mirroring sims.files C#):
//   files/formats/iff/IffFile.h          class IffFile { IffFile(); IffFile(const std::string& path);
//                                            void Read(const uint8_t* data, size_t size);
//                                            template<class T> T* Get(uint16_t id);          // nullptr if absent
//                                            template<class T> std::vector<T*> List();       // empty if none
//                                            IffRuntimeInfo RuntimeInfo; std::string Filename; }
//   files/formats/iff/chunks/<CHUNK>.h   one class per chunk, exact C# field names.
//   files/formats/otf/OTFFile.h          OTFFile, OTFTable, OTFTableKey
//   files/FAR1/FAR1Archive.h             FAR1Archive(path, v1b), GetAllFarEntries(), GetEntry(const FarEntry&)
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <array>
#include "core/math.h"

// Identical to the macro in files/formats/iff/AbstractIffChunk.h (operator& yields int64_t).
#ifndef FSO_FLAGS_ENUM
#define FSO_FLAGS_ENUM(T) \
    inline T operator|(T a, T b) { return (T)((int64_t)a | (int64_t)b); } \
    inline T& operator|=(T& a, T b) { a = a | b; return a; } \
    inline int64_t operator&(T a, T b) { return (int64_t)a & (int64_t)b; } \
    inline T& operator&=(T& a, T b) { a = (T)((int64_t)a & (int64_t)b); return a; } \
    inline T operator~(T a) { return (T)(~(int64_t)a); } \
    inline bool operator>(T a, int b) { return (int64_t)a > b; } \
    inline bool operator==(T a, int b) { return (int64_t)a == b; } \
    inline bool operator!=(T a, int b) { return (int64_t)a != b; }
#endif

#ifdef ST_FILES_REAL
#include "files/formats/iff/IffFile.h"
#include "files/formats/iff/chunks/OBJD.h"
#include "files/formats/iff/chunks/BHAV.h"
#include "files/formats/iff/chunks/TTAB.h"
#include "files/formats/iff/chunks/TTAs.h"
#include "files/formats/iff/chunks/STR.h"
#include "files/formats/iff/chunks/CTSS.h"
#include "files/formats/iff/chunks/BCON.h"
#include "files/formats/iff/chunks/OBJf.h"
#include "files/formats/iff/chunks/GLOB.h"
#include "files/formats/iff/chunks/SLOT.h"
#include "files/formats/iff/chunks/ARRY.h"
#include "files/formats/iff/chunks/OBJM.h"
#include "files/formats/iff/chunks/OBJT.h"
#include "files/formats/iff/chunks/SIMI.h"
#include "files/formats/iff/chunks/HOUS.h"
#include "files/formats/iff/chunks/WALm.h"
#include "files/formats/iff/chunks/FWAV.h"
#include "files/formats/iff/chunks/DGRP.h"
#include "files/formats/iff/chunks/SPR.h"
#include "files/formats/iff/chunks/SPR2.h"
#include "files/formats/iff/chunks/BMP.h"
#include "files/formats/otf/OTFFile.h"
#include "files/FAR1/FAR1Archive.h"
#else

namespace FSO { namespace Files {

namespace FAR1 {
    class FarEntry {
    public:
        int32_t DataLength = 0;
        int32_t DataLength2 = 0;
        int32_t DataOffset = 0;
        int16_t FilenameLength = 0;
        std::string Filename;
    };
    class FAR1Archive {
    public:
        FAR1Archive(const std::string& path, bool v1b);
        std::vector<FarEntry>& GetAllFarEntries();
        std::vector<uint8_t> GetEntry(const FarEntry& entry, bool* found = nullptr);
        void Close();
    };
}

namespace Formats {

namespace OTF {
    class OTFTableKey { public: int32_t ID = 0; std::string Label; int32_t Value = 0; };
    class OTFTable {
    public:
        int32_t ID = 0;
        std::string Name;
        std::vector<OTFTableKey> Keys;
        OTFTableKey* GetKey(int32_t id);
    };
    class OTFFile {
    public:
        OTFFile();
        explicit OTFFile(const std::string& filepath);
        std::vector<OTFTable> Tables;
        OTFTable* GetTable(int32_t id);
        void Read(const std::vector<uint8_t>& data);
    };
}

namespace IFF {

enum class IffRuntimeState { ReadOnly, PIFFPatch, PIFFClone, Standalone };
enum class IffUseCase { Global, Object, ObjectSprites };

class IffRuntimeInfo {
public:
    IffRuntimeState State = IffRuntimeState::ReadOnly;
    IffUseCase UseCase = IffUseCase::Global;
    std::string Filename;
    std::string Path;
    bool Dirty = false;
};

class IffFile;

class IffChunk {
public:
    virtual ~IffChunk() {}
    uint16_t ChunkID = 0;
    uint16_t ChunkFlags = 0;
    std::string ChunkType;
    std::string ChunkLabel;
    bool ChunkProcessed = false;
    uint16_t OriginalID = 0;
    bool AddedByPatch = false;
    std::vector<uint8_t> ChunkData;
    IffFile* ChunkParent = nullptr;
};

class IffFile {
public:
    IffFile();
    explicit IffFile(const std::string& filepath);
    std::string Filename;
    IffRuntimeInfo RuntimeInfo;
    uint32_t ExecutableHash = 0;
    void Read(const std::vector<uint8_t>& data);
    template<class T> T* Get(uint16_t id);
    template<class T> std::vector<T*> List();
    void InitHash();
    void SetFilename(const std::string& filename);
};

namespace Chunks {

enum class OBJDType : int {
    Unknown = 0, Person = 2, Normal = 4, SimType = 7, Portal = 8, Cursor = 9,
    PrizeToken = 10, Internal = 11, Food = 34
};

class OBJD : public IffChunk {
public:
    uint32_t Version = 0;
    uint16_t GUID1() const { return (uint16_t)GUID; }
    uint16_t GUID2() const { return (uint16_t)(GUID >> 16); }
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
    bool IsMaster() const { return SubIndex == -1; }
    bool IsMultiTile() const { return MasterID != 0; }
};

class BHAVInstruction {
public:
    uint16_t Opcode = 0;
    uint8_t TruePointer = 0;
    uint8_t FalsePointer = 0;
    std::array<uint8_t, 8> Operand = {{0, 0, 0, 0, 0, 0, 0, 0}};
    bool Breakpoint = false; //only used at runtime
};

class BHAV : public IffChunk {
public:
    std::vector<BHAVInstruction> Instructions;
    uint8_t Type = 0;
    uint8_t Args = 0;
    uint16_t Locals = 0;
    uint16_t Flags = 0;
    uint32_t RuntimeVer = 0;
};

enum class TTABFlags : int32_t {
    AllowVisitors = 1,
    Joinable = 1 << 1,
    RunImmediately = 1 << 2,
    AllowConsecutive = 1 << 3,
    Debug = 1 << 7,
    AutoFirstSelect = 1 << 8,
    Leapfrog = 1 << 9,
    MustRun = 1 << 10,
    AllowDogs = 1 << 11,
    AllowCats = 1 << 12,
    TS1AllowCats = 1 << 9,
    TS1AllowDogs = 1 << 10,
    TSOAvailableCarrying = 1 << 16,
    TSOIsRepair = 1 << 17,
    TSORunCheckAlways = 1 << 18,
    TSOAvailableWhenDead = 1 << 19,
    FSOPushTail = 1 << 30,
    FSOPushHead = 1 << 29,
    TS1NoChild = 1 << 4, // TS1(sims): FreeSO
    TS1NoDemoChild = 1 << 5,
    TS1NoAdult = 1 << 6,
    FSOSkipPermissions = 1 << 28,
    FSODirectControl = 1 << 27,
};
FSO_FLAGS_ENUM(TTABFlags)

enum class TSOFlags : int32_t {
    NonEmpty = 1,
    AllowObjectOwner = 1 << 1,
    AllowRoommates = 1 << 2,
    AllowFriends = 1 << 3,
    AllowVisitors = 1 << 4,
    AllowGhost = 1 << 5,
    UnderParentalControl = 1 << 6,
    AllowCSRs = 1 << 7
};
FSO_FLAGS_ENUM(TSOFlags)

enum class InteractionMaskFlags : int32_t {
    AvailableWhenCarrying = 1, IsRepair = 1 << 1, RunCheckAlways = 1 << 2, AvailableWhenDead = 1 << 3
};
FSO_FLAGS_ENUM(InteractionMaskFlags)

struct TTABMotiveEntry {
    int32_t MotiveIndex = 0; // TS1(sims): FreeSO
    int16_t EffectRangeMinimum = 0;
    int16_t EffectRangeMaximum = 0;
    uint16_t PersonalityModifier = 0;
};

class TTABInteraction {
public:
    uint16_t ActionFunction = 0;
    uint16_t TestFunction = 0;
    std::vector<TTABMotiveEntry> MotiveEntries;
    TTABFlags Flags = (TTABFlags)0;
    uint32_t TTAIndex = 0;
    uint32_t AttenuationCode = 0;
    float AttenuationValue = 0;
    uint32_t AutonomyThreshold = 0;
    int32_t JoiningIndex = 0;
    TSOFlags Flags2 = (TSOFlags)0x1f;
    std::vector<TTABMotiveEntry> ActiveMotiveEntries; // TS1(sims): FreeSO
    bool AutoFirst() const { return (Flags & TTABFlags::AutoFirstSelect) > 0; }
    bool Debug() const { return (Flags & TTABFlags::Debug) > 0; }
    bool Leapfrog() const { return (Flags & TTABFlags::Leapfrog) > 0; }
    bool MustRun() const { return (Flags & TTABFlags::MustRun) > 0; }
    bool AllowConsecutive() const { return (Flags & TTABFlags::AllowConsecutive) > 0; }
    bool AllowCats() const { return (Flags & TTABFlags::AllowCats) > 0; }
    bool AllowDogs() const { return (Flags & TTABFlags::AllowDogs) > 0; }
    bool AllowGhosts() const { return (Flags2 & TSOFlags::AllowGhost) > 0; }
    bool AllowCSRs() const { return (Flags2 & TSOFlags::AllowCSRs) > 0; }
    bool WhenDead() const { return (Flags & TTABFlags::TSOAvailableWhenDead) > 0; }
};

class TTAB : public IffChunk {
public:
    std::vector<std::unique_ptr<TTABInteraction>> Interactions;
    std::unordered_map<uint32_t, TTABInteraction*> InteractionByIndex;
    std::vector<TTABInteraction*> AutoInteractions; // TS1(sims): FreeSO
};

enum class STRLangCode : uint8_t { Default = 0, EnglishUS = 1 };

class STRItem { public: uint8_t LanguageCode = 0; std::string Value; std::string Comment; };
class STRLanguageSet { public: std::vector<STRItem> Strings; };

class STR : public IffChunk {
public:
    std::vector<STRLanguageSet> LanguageSets;
    int32_t Length() const;
    // Returns an empty string when index is out of range (C# returns null).
    std::string GetString(int32_t index);
    bool TryGetString(int32_t index, STRLangCode language, std::string& out);
    static STRLangCode DefaultLangCode;
};

class CTSS : public STR {};
class TTAs : public STR {};

class BCON : public IffChunk {
public:
    uint8_t Flags = 0;
    std::vector<uint16_t> Constants;
};

struct OBJfFunctionEntry {
    uint16_t ConditionFunction = 0;
    uint16_t ActionFunction = 0;
};

class OBJf : public IffChunk {
public:
    std::vector<OBJfFunctionEntry> functions;
    uint32_t Version = 0;
};

class GLOB : public IffChunk {
public:
    std::string Name;
};

enum class SLOTFlags : int {
    NORTH = 1, NORTH_EAST = 2, EAST = 4, SOUTH_EAST = 8, SOUTH = 16, SOUTH_WEST = 32, WEST = 64, NORTH_WEST = 128,
    AllowAnyRotation = 256, Absolute = 512, FacingAwayFromObject = 1024, IgnoreRooms = 2048, SnapToDirection = 4096,
    RandomScoring = 8192, AllowFailureTrees = 16385, AllowDifferentAlts = 32768, UseAverageObjectLocation = 65536
};
FSO_FLAGS_ENUM(SLOTFlags)

enum class SLOTFacing : int { FaceAnywhere = -3, FaceTowardsObject = -2, FaceAwayFromObject = -1 };

class SLOTItem {
public:
    uint16_t Type = 0;
    st::Vector3 Offset;
    int32_t Standing = 1;
    int32_t Sitting = 0;
    int32_t Ground = 0;
    SLOTFlags Rsflags = (SLOTFlags)0;
    int32_t SnapTargetSlot = -1;
    int32_t MinProximity = 0;
    int32_t MaxProximity = 0;
    int32_t OptimalProximity = 0;
    float Gradient = 0;
    SLOTFacing Facing = SLOTFacing::FaceTowardsObject;
    int32_t Resolution = 16;
    int32_t Height = 0;
};

class SLOT : public IffChunk {
public:
    std::unordered_map<uint16_t, std::vector<SLOTItem>> Slots;
};

enum class ARRYType : int { RLEAlt = 4, RLEFloor = 1, RLEWalls = 8, Objects = 2 };

class ARRY : public IffChunk {
public:
    int32_t Width = 0;
    int32_t Height = 0;
    ARRYType Type = ARRYType::RLEFloor;
    std::vector<uint8_t> Data;
    std::vector<uint8_t> TransposeData() const;
    int32_t ByteSize() const { return (int32_t)Type; }
};

class OBJTEntry {
public:
    uint32_t GUID = 0;
    uint16_t Unknown1a = 0;
    uint16_t InitTreeVersion = 0;
    uint16_t Unknown2a = 0;
    uint16_t MainTreeVersion = 0;
    uint16_t TypeID = 0;
    Chunks::OBJDType OBJDType = Chunks::OBJDType::Unknown;
    std::string Name;
};

class OBJT : public IffChunk {
public:
    std::vector<OBJTEntry> Entries;
};

class OBJM : public IffChunk {
public:
    class MappedObject {
    public:
        std::string Name;
        uint32_t GUID = 0;
        int32_t ObjectID = 0;
        int32_t Direction = 0;
        int32_t ParentID = 0;
        int32_t ContainerID = 0;
        int32_t ContainerSlot = 0;
        std::vector<int16_t> Data;
        int32_t ArryX = 0;
        int32_t ArryY = 0;
        int32_t ArryLevel = 0;
    };
    std::vector<uint16_t> IDToOBJT;
    std::map<int32_t, MappedObject> ObjectData;
};

class SIMI : public IffChunk {
public:
    class SIMIBudgetDay {
    public:
        int32_t Valid = 0, MiscIncome = 0, JobIncome = 0, ServiceExpense = 0, FoodExpense = 0,
                BillsExpense = 0, MiscExpense = 0, HouseholdExpense = 0, ArchitectureExpense = 0;
    };
    uint32_t Version = 0;
    std::vector<int16_t> GlobalData; // 38 entries
    int16_t Unknown1 = 0;
    int32_t Unknown2 = 0, Unknown3 = 0, GUID1 = 0, GUID2 = 0, Unknown4 = 0, LotValue = 0, ObjectsValue = 0, ArchitectureValue = 0;
    std::vector<SIMIBudgetDay> BudgetDays;
    int32_t PurchaseValue() const { return LotValue + ObjectsValue + (ArchitectureValue * 7) / 10; }
};

class HOUS : public IffChunk {
public:
    int32_t Version = 0, UnknownFlag = 0, UnknownOne = 0, UnknownNumber = 0, UnknownNegative = 0;
    int16_t CameraDir = 0, UnknownOne2 = 0, UnknownFlag2 = 0;
    uint32_t GUID = 0;
    std::string RoofName;
};

class WALmEntry {
public:
    std::string Name;
    int32_t Unknown = 0;
    uint8_t ID = 0;
    std::vector<uint8_t> Unknown2;
};

class WALm : public IffChunk {
public:
    std::vector<WALmEntry> Entries;
};
class FLRm : public WALm {};

class FWAV : public IffChunk {
public:
    std::string Name;
};

class DGRP : public IffChunk {};
class SPR : public IffChunk {};
class SPR2 : public IffChunk {};
class BMP : public IffChunk {};

} // namespace Chunks
} // namespace IFF
} // namespace Formats
}} // namespace FSO::Files

#endif // ST_FILES_REAL
