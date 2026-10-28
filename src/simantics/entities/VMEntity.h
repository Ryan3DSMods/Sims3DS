/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.VMEntity — an object in the VM (Virtual Machine). (entities/VMEntity.cs)
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <list>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include "core/math.h"
#include "simantics/_files_contract.h"
#include "content/ContentManager.h"
#include "world/model/LotTilePos.h"
#include "world/model/Direction.h"
#include "world/model/WallTile.h"
#include "world/model/FloorTile.h"
#include "world/components/WorldComponents.h"
#include "simantics/model/VMStackObjectVariable.h"
#include "simantics/model/VMPlacementError.h"
#include "simantics/model/VMPlatformState.h"
#include "simantics/model/VMBHAVOwnerPair.h"
#include "simantics/model/VMRuntimeHeadline.h"
#include "simantics/model/Routing/VMObstacle.h"
#include "simantics/model/Sound/VMSoundEntry.h"
#include "simantics/engine/VMQueuedAction.h"
#include "simantics/VMPlacementResult.h"

namespace FSO { namespace SimAntics {

class VM;
class VMContext;
class VMArchitecture;
namespace Engine { class VMThread; }
namespace Entities { class VMMultitileGroup; }
namespace Model { namespace TSOPlatform { class VMTSOEntityState; } }

enum class VMEntityFlags : int32_t {
    ShowGhost = 1,
    DisallowPersonIntersection = 1 << 1,
    HasZeroExtent = 1 << 2, //4
    CanWalk = 1 << 3, //8
    AllowPersonIntersection = 1 << 4, //16
    Occupied = 1 << 5, //32
    NotifiedByIdleForInput = 1 << 6,
    InteractionCanceled = 1 << 7,
    ChairFacing = 1 << 8,
    Burning = 1 << 9,
    HideForCutaway = 1 << 10,
    FireProof = 1 << 11,
    TurnedOff = 1 << 12,
    NeedsMaintinance = 1 << 13,
    ShowDynObjNameInTooltip = 1 << 14
};
FSO_FLAGS_ENUM(VMEntityFlags)

enum class WallPlacementFlags : int32_t {
    WallRequiredInFront = 1,
    WallRequiredOnRight = 1 << 1,
    WallRequiredBehind = 1 << 2,
    WallRequiredOnLeft = 1 << 3,
    CornerNotAllowed = 1 << 4,
    CornerRequired = 1 << 5,
    DiagonalRequired = 1 << 6,
    DiagonalAllowed = 1 << 7,
    WallNotAllowedInFront = 1 << 8,
    WallNotAllowedOnRight = 1 << 9,
    WallNotAllowedBehind = 1 << 10,
    WallNotAllowedOnLeft = 1 << 11
};
FSO_FLAGS_ENUM(WallPlacementFlags)

enum class VMPlacementFlags : int32_t {
    OnFloor = 1,
    OnTerrain = 1 << 1,
    OnWater = 1 << 2,
    OnSurface = 1 << 3, //redundant
    OnDoor = 1 << 4, //what?
    OnWindow = 1 << 5, //curtains??
    OnLockedTile = 1 << 6, //?????
    RequireFirstLevel = 1 << 7,
    OnSlope = 1 << 8,
    InAir = 1 << 9,
    InWall = 1 << 10, //redundant?
    AllowOnPool = 1 << 11,
    RequirePool = 1 << 12,
    RequireWater = 1 << 13,
};
FSO_FLAGS_ENUM(VMPlacementFlags)

enum class VMEntityFlags2 : int32_t {
    CanBreak = 1,
    CanDie = 1 << 1,
    CanBeReposessed = 1 << 2,
    ObstructsView = 1 << 3,
    Floats = 1 << 4,
    Burns = 1 << 5,
    Fixable = 1 << 6,
    CannotBeStolen = 1 << 7,
    GeneratesHeat = 1 << 8,
    CanBeLighted = 1 << 9,
    GeneratesLight = 1 << 10,
    CanGetDirty = 1 << 11,
    ContributesToAsthetic = 1 << 12,
    unused14 = 1 << 13,
    ArchitectualWindow = 1 << 14,
    ArchitectualDoor = 1 << 15
};
FSO_FLAGS_ENUM(VMEntityFlags2)

enum class VMMovementFlags : int32_t {
    SimsCanMove = 1, //only cleared by payphone...
    PlayersCanMove = 1 << 1,
    SelfPropelled = 1 << 2, //unused
    PlayersCanDelete = 1 << 3,
    StaysAfterEvict = 1 << 4
};
FSO_FLAGS_ENUM(VMMovementFlags)

class VMPieMenuInteraction {
public:
    std::string Name;
    int16_t Param0 = 0;
    uint8_t ID = 0;
    FSO::Files::Formats::IFF::Chunks::TTABInteraction* Entry = nullptr;
    // TS1(sims): FreeSO VMPieMenuInteraction fields used by TS1 autonomy.
    bool Global = false;
    float Score = 0;
    VMEntity* Callee = nullptr;
    std::shared_ptr<std::unordered_map<int32_t, int16_t>> MotiveAdChanges; // tree ad changes made by the check tree (null = none)
};

class VMEntityRTTI {
public:
    std::vector<std::string> AttributeLabels;
};

/// An object in the VM (Virtual Machine)
class VMEntity {
public:
    using Chunks_OBJD = FSO::Files::Formats::IFF::Chunks::OBJD;
    using LotTilePos = FSO::LotView::Model::LotTilePos;
    using DirectionT = FSO::LotView::Model::Direction;

    static bool UseWorld;

    VMEntityRTTI RTTI;
    bool GhostImage = false;
    bool Reseted = false;
    bool Portal = false;
    Entities::VMMultitileGroup* GhostOriginal = nullptr; //Ignore collisions/slots from any of these objects.

    //own properties (for instance)
    int16_t ObjectID = 0;
    uint32_t PersistID = 0;
    std::shared_ptr<Model::VMPlatformState> PlatformState;
    /// C# `TSOState` (VMTSOEntityState); null when the platform state is not a TSO entity state.
    Model::TSOPlatform::VMTSOEntityState* TSOState() const;

    std::vector<int16_t> ObjectData;
    std::list<int16_t> MyList;
    std::vector<Model::Sound::VMSoundEntry> SoundThreads;

    std::unique_ptr<Model::VMRuntimeHeadline> Headline;
    std::unique_ptr<Model::VMHeadlineRenderer> HeadlineRenderer; //IS NOT serialized, but rather regenerated on deserialize.

    FSO::Content::GameObject* Object;
    std::unique_ptr<Engine::VMThread> Thread;
    Entities::VMMultitileGroup* MultitileGroup = nullptr;

    int16_t MainParam = 0; //parameters passed to main on creation.
    int16_t MainStackOBJ = 0;

    std::vector<VMEntity*> Contained;
    VMEntity* Container = nullptr;
    int16_t ContainerSlot = 0;
    bool Dead = false; //set when the entity is removed, threads owned by this object or with this object as callee will be cancelled/have their stack emptied.
    bool BadName = false;

    /** Persistent state variables controlled by bhavs **/
    //in TS1, NumAttributes can be 0 and it will dynamically resize as required.
    //for backwards compatability, we support this.
    virtual int16_t GetAttribute(int32_t index);
    virtual void SetAttribute(int32_t index, int16_t value);
    /// TS1(sims): read-only view for the lot saver (utils/VMTS1LotSaver): the attributes as they are now.
    const std::vector<int16_t>& AttributeData() const { return Attributes; }

    /** Relationship variables **/
    std::unordered_map<uint16_t, std::vector<int16_t>> MeToObject;
    //todo, special system for server persistent avatars and pets

    uint64_t DynamicSpriteFlags = 0; /** Used to show/hide dynamic sprites **/
    uint64_t DynamicSpriteFlags2 = 0;
    std::unique_ptr<Model::Routing::VMObstacle> Footprint; // null when the object has zero extent

    std::unique_ptr<FSO::LotView::Components::EntityComponent> WorldUI;

    //inferred properties (from object resource)
    FSO::Content::GameGlobalResource* SemiGlobal = nullptr;
    FSO::Files::Formats::IFF::Chunks::TTAB* TreeTable = nullptr;
    FSO::Files::Formats::IFF::Chunks::TTAs* TreeTableStrings = nullptr;
    std::unordered_map<std::string, FSO::Content::VMTreeByNameTableEntry>* TreeByName = nullptr;
    FSO::Files::Formats::IFF::Chunks::SLOT* Slots = nullptr;
    Chunks_OBJD* MasterDefinition = nullptr; //if this object is multitile, its master definition will be stored here.
    std::vector<FSO::Files::Formats::IFF::Chunks::OBJfFunctionEntry> EntryPoints;  /** Entry points for specific events, eg. init, main, clean... **/

    std::string Name() const;
    void Name(const std::string& value);

    bool GetBadObjects();
    bool GetBadNames();

    //positioning properties (C# property Position with logic -> Position()/Position(value))
    const LotTilePos& Position() const { return _Position; }
    void Position(const LotTilePos& value);

    virtual st::Vector3 VisualPosition() const = 0;
    virtual void VisualPosition(const st::Vector3& value) = 0;
    virtual DirectionT Direction() const = 0;
    virtual void Direction(DirectionT value) = 0;
    virtual float RadianDirection() const = 0;
    virtual void RadianDirection(float value) = 0;

    /// Constructs a new VMEntity instance.
    explicit VMEntity(FSO::Content::GameObject* obj);
    virtual ~VMEntity();

    /// Supply a game object who's tree table this VMEntity can use. (manually set the tree table for an object. Used for multitile objects, which inherit this from the master.)
    void UseTreeTableOf(FSO::Content::GameObject* obj, bool ts1);
    void UseSemiGlobalTTAB(const std::string& sgFile, uint16_t id, bool ts1);

    bool IsInUse(VMContext* context, bool multitile);
    bool IsInUse(VMContext* context, bool multitile, bool stackObjSafety);
    /// TS1(sims): FreeSO GetUsers — avatars whose running interactions (queue up to the active block) target
    /// this object or another part of its multitile group. Backs the engine-computed "use count" (object data 62).
    void GetUsers(VMContext* context, std::unordered_set<VMEntity*>* users);
    int16_t UseCount();

    virtual void Tick();
    void TickSounds();
    std::vector<Model::Sound::VMSoundTransfer> GetActiveSounds();

    std::vector<FSO::Files::Formats::IFF::Chunks::OBJfFunctionEntry> GenerateFunctionTable(Chunks_OBJD* obj);

    virtual void Init(VMContext* context);
    virtual void Reset(VMContext* context);
    void GenerateTreeByName(VMContext* context);

    bool ExecuteEntryPoint(int32_t entry, VMContext* context, bool runImmediately);
    bool ExecuteEntryPoint(int32_t entry, VMContext* context, bool runImmediately, VMEntity* stackOBJ);
    bool ExecuteEntryPoint(int32_t entry, VMContext* context, bool runImmediately, VMEntity* stackOBJ, const std::vector<int16_t>* args);

    /// Returns null when the tree cannot be found. (caller owns the result)
    std::unique_ptr<Model::VMBHAVOwnerPair> GetBHAVWithOwner(uint16_t ActionID, VMContext* context);

    Model::VMPlacementError IsUserMovable(VMContext* context, bool deleting);
    void FetchTreeByName(VMContext* context);

    bool IsDynamicSpriteFlagSet(uint16_t index) const;
    virtual void SetDynamicSpriteFlag(uint16_t index, bool set);

    void SetFlag(VMEntityFlags flag, bool set);
    bool GetFlag(VMEntityFlags flag) const;

    virtual int16_t GetValue(Model::VMStackObjectVariable var);
    virtual bool SetValue(Model::VMStackObjectVariable var, int16_t value);

    // Begin Container SLOTs interface
    virtual int32_t TotalSlots() = 0;
    virtual bool PlaceInSlot(VMEntity* obj, int32_t slot, bool cleanOld, VMContext* context) = 0;
    virtual VMEntity* GetSlot(int32_t slot) = 0;
    virtual void ClearSlot(int32_t slot) = 0;
    virtual int32_t GetSlotHeight(int32_t slot) = 0;
    void RecurseSlotPositionChange(VMContext* context, bool noEntryPoint);
    // End Container SLOTs interface

    virtual void SetRoom(uint16_t room);

    std::vector<VMPieMenuInteraction> GetPieMenu(VM* vm, VMEntity* caller, bool includeHidden);
    std::shared_ptr<Engine::VMQueuedAction> GetAction(int32_t interaction, VMEntity* caller, VMContext* context);
    std::shared_ptr<Engine::VMQueuedAction> GetAction(int32_t interaction, VMEntity* caller, VMContext* context, const std::vector<int16_t>* args);
    void PushUserInteraction(int32_t interaction, VMEntity* caller, VMContext* context);
    void PushUserInteraction(int32_t interaction, VMEntity* caller, VMContext* context, const std::vector<int16_t>* args);

    VMPlacementResult PositionValid(const LotTilePos& pos, DirectionT direction, VMContext* context);
    Model::VMPlacementError SlopeValid(); // TS1(sims): FreeSO
    Model::VMPlacementError FloorChangeValid(const FSO::LotView::Model::FloorTile& floor, int8_t level);
    Model::VMPlacementError WallChangeValid(const FSO::LotView::Model::WallTile& wall, DirectionT direction, bool checkUnused);
    int32_t RotateWallSegs(FSO::LotView::Model::WallSegments ws, int32_t rotate);
    void SetWallUse(VMArchitecture* arch, bool set);

    void Delete(bool cleanupAll, VMContext* context);

    /// Returns null for zero-extent objects. (caller owns the result)
    virtual std::unique_ptr<Model::Routing::VMObstacle> GetObstacle(const LotTilePos& pos, DirectionT dir) = 0;

    virtual void PrePositionChange(VMContext* context);
    virtual void PositionChange(VMContext* context, bool noEntryPoint);
    virtual VMPlacementResult SetPosition(const LotTilePos& pos, DirectionT direction, VMContext* context);
    virtual void SetIndivPosition(const LotTilePos& pos, DirectionT direction, VMContext* context, const VMPlacementResult& info);

    int32_t DirectionToWallOff(DirectionT dir);
    void SetWallStyle(int32_t side, VMArchitecture* arch, uint16_t value);
    void UpdateDynamicMultitile(VMContext* context);

    virtual void* GetIcon(void* gd, int32_t store) = 0; // STUB(3ds): Texture2D GetIcon(GraphicsDevice, int)

    virtual std::string ToString() = 0;

private:
    std::vector<int16_t> Attributes;
    LotTilePos _Position = LotTilePos::OUT_OF_WORLD;
    Model::VMPlacementError IndividualUserMovable(VMContext* context, bool deleting);
};

}}
