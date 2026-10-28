# src/simantics — SimAntics VM core (C++17 port of FreeSims `SimsVille/SimsAntics`)

Namespaces mirror C#: `FSO::SimAntics`, `FSO::SimAntics::Engine`, `FSO::SimAntics::Engine::Utils` (VMMemory),
`FSO::SimAntics::Engine::Scopes`, `FSO::SimAntics::Model`, `FSO::SimAntics::Entities`,
`FSO::SimAntics::Primitives`. Stubs and deviations: see `STUBS.md`.

Build check: `make -C src/simantics simantics-check` (host g++ + arm-none-eabi, against `_files_contract.h`)
and `make -C src/simantics simantics-check-real` (same, `-DST_FILES_REAL`, against the real `src/files`).
Primitive sources dropped into `src/simantics/primitives/` are compiled by both targets automatically.

## Writing a primitive

One C# file → `primitives/VMFoo.h` + `primitives/VMFoo.cpp`, namespace `FSO::SimAntics::Primitives`.

```cpp
#include "simantics/engine/VMPrimitiveHandler.h"   // handler base + exit codes + operand base
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMThread.h"
#include "simantics/engine/VMMemory.h"              // FSO::SimAntics::Engine::Utils::VMMemory
#include "simantics/entities/VMEntity.h"            // + VMAvatar.h / VMGameObject.h / VMMultitileGroup.h
#include "simantics/VM.h"
#include "simantics/VMContext.h"

namespace FSO { namespace SimAntics { namespace Primitives {
class VMFoo : public Engine::VMPrimitiveHandler {
public:
    Engine::VMPrimitiveExitCode Execute(Engine::VMStackFrame* context, Engine::VMPrimitiveOperand* args) override {
        auto operand = static_cast<VMFooOperand*>(args);   // C#: (VMFooOperand)args
        ...
        return Engine::VMPrimitiveExitCode::GOTO_TRUE;
    }
};
class VMFooOperand : public Engine::VMPrimitiveOperand {
public:
    uint16_t Something = 0;
    void Read(const uint8_t* bytes) override {            // always the 8 BHAV operand bytes
        st::Reader io(bytes, 8, st::Endian::Little);       // IoBuffer.FromBytes(bytes, LITTLE_ENDIAN)
        Something = io.u16();
    }
    void Write(uint8_t* bytes) override { ... }
};
}}}
```

### Core classes a primitive needs (exact signatures)

`Engine::VMPrimitiveExitCode` (uint8 enum): `GOTO_TRUE, GOTO_FALSE, GOTO_TRUE_NEXT_TICK, GOTO_FALSE_NEXT_TICK, RETURN_TRUE, RETURN_FALSE, ERROR, CONTINUE_NEXT_TICK, CONTINUE, INTERRUPT`.

`Engine::VMPrimitiveHandler`: `virtual VMPrimitiveExitCode Execute(VMStackFrame* context, VMPrimitiveOperand* operand) = 0;` protected `void Trace(const std::string&)`.

`Engine::VMPrimitiveOperand`: `virtual void Read(const uint8_t* bytes) = 0; virtual void Write(uint8_t* bytes) = 0;` (8 bytes).

`Engine::VMStackFrame` (fields are public, C# names):
`VMThread* Thread; VMRoutine* Routine; uint16_t InstructionPointer; VMEntity* Caller; VMEntity* Callee; VMEntity* StackObject; VMSpecialResult SpecialResult (TS1(sims): FreeSO, replaces bool DiscardResult); bool ActionTree; FSO::Content::GameObject* CodeOwner; std::vector<int16_t> Locals; std::vector<int16_t> Args;`
methods (C# properties): `GameIffResource* ScopeResource()`, `GameObjectResource* CallerPrivate()/CalleePrivate()/StackObjPrivate()`, `GameGlobal* Global()`, `FSO::SimAntics::VM* VM()`, `VMInstruction& GetCurrentInstruction()`, `template<T> T* GetCurrentOperand()`, `virtual bool IsRoutingFrame()` (C# `is VMRoutingFrame`).

`Engine::VMThread`: `VMContext* Context; std::vector<std::shared_ptr<VMStackFrame>> Stack; std::vector<std::shared_ptr<VMQueuedAction>> Queue; bool QueueDirty; uint8_t ActiveQueueBlock; std::array<int16_t,20> TempRegisters; std::array<int32_t,2> TempXL; VMPrimitiveExitCode LastStackExitCode; std::shared_ptr<Model::TSOPlatform::VMAsyncState> BlockingState; bool IsCheck; std::vector<VMPieMenuInteraction>* ActionStrings; bool Interrupt; int32_t DialogCooldown; VMThreadBreakMode ThreadBreak;`
`static VMPrimitiveExitCode EvaluateCheck(VMContext*, VMEntity*, std::shared_ptr<VMStackFrame> [, std::shared_ptr<VMQueuedAction> [, std::vector<VMPieMenuInteraction>*]])`,
`bool RunInMyStack(BHAV*, GameObject* codeOwner, const int16_t passVars[4], VMEntity* stackObj)`, `bool AttemptPush()`,
`VMRoutingFrame* PushNewRoutingFrame(VMStackFrame* frame, bool failureTrees)`,
`void ExecuteSubRoutine(VMStackFrame*, BHAV*, GameObject* codeOwner, const Primitives::VMSubRoutineOperand&)`,
`void Push(std::shared_ptr<VMStackFrame>)`, `void Pop(VMPrimitiveExitCode)`, `void EnqueueAction(std::shared_ptr<VMQueuedAction>)`,
`void CancelAction(uint16_t uid)`, `std::unique_ptr<std::vector<VMPieMenuInteraction>> CheckAction(std::shared_ptr<VMQueuedAction>)` (null == not allowed), `VMEntity* GetEntity()`.

`Engine::VMQueuedAction`: `ActionRoutine, CheckRoutine (VMRoutine*), Callee, StackObject, CodeOwner, Name, std::vector<int16_t> Args (empty == null), InteractionNumber, Cancelled, int16_t Priority, VMQueueMode Mode, TTABFlags Flags, TSOFlags Flags2, uint16_t UID, std::shared_ptr<VMActionCallback> Callback`, `IconOwner()/IconOwner(VMEntity*)`, `ToStackFrame(VMEntity*)`. Enums `VMQueuePriority`, `VMQueueMode`.
`Engine::VMActionCallback(VM*, int16_t interaction, VMEntity* target, VMEntity* stackObj, VMEntity* caller, bool paramAsObjectID, bool isTree)`.

`Engine::Utils::VMMemory` (all static, every scope 0–59 translated):
`int16_t GetVariable(VMStackFrame*, Scopes::VMVariableScope, int16_t data)`, `bool SetVariable(..., int16_t data, int16_t value)`,
`int32_t GetBigVariable(...)`, `bool SetBigVariable(..., int32_t value)` (TempXL aware), `std::list<int16_t>& GetList(VMStackFrame*, scope)`,
`int16_t GetTuningVariable(VMEntity*, uint16_t data, VMStackFrame*)`, `std::string GetTuningVariableLabel(GameObject*, uint16_t)`,
`int16_t GetEntityDefinitionVar(OBJD*, Model::VMOBJDVariable, VMStackFrame*)`,
`Vitaboy::Animation* GetAnimation(VMStackFrame*, Scopes::VMAnimationScope, uint16_t)`, `Vitaboy::Appearance* GetSuit(VMStackFrame*, Scopes::VMSuitScope, uint16_t)`,
`SLOTItem* GetSlot(VMStackFrame*, Scopes::VMSlotScope, uint16_t)`. Errors throw `Engine::VMSimanticsException(msg, frame)`.

`VMEntity` (abstract; `VMGameObject`, `VMAvatar` derive). C# properties with logic are getter/setter pairs:
`Position()/Position(LotTilePos)`, `Direction()/Direction(Direction)`, `RadianDirection()/RadianDirection(float)`, `VisualPosition()/VisualPosition(Vector3)`, `Name()/Name(str)`, `TSOState()`.
Fields: `ObjectID, PersistID, PlatformState, ObjectData (80), MyList (std::list<int16_t>), Object (GameObject*), Thread (unique_ptr<VMThread>), MultitileGroup, MainParam, MainStackOBJ, Contained, Container, ContainerSlot, Dead, MeToObject (unordered_map<uint16_t, vector<int16_t>>), DynamicSpriteFlags(2), Footprint (unique_ptr<VMObstacle>), SemiGlobal, TreeTable, TreeTableStrings, TreeByName, Slots, MasterDefinition, EntryPoints (vector<OBJfFunctionEntry>, ≥33), Headline/HeadlineRenderer (unique_ptr), GhostImage, GhostOriginal`.
Methods: `GetAttribute/SetAttribute(int, int16_t)`, `GetValue/SetValue(VMStackObjectVariable, ...)`, `GetFlag/SetFlag(VMEntityFlags)`, `IsDynamicSpriteFlagSet/SetDynamicSpriteFlag`, `ExecuteEntryPoint(int entry, VMContext*, bool runImmediately [, VMEntity* stackOBJ [, const std::vector<int16_t>* args]])`, `GetBHAVWithOwner(uint16_t, VMContext*)` → `unique_ptr<VMBHAVOwnerPair>` (`bhav`, `owner`), `GetAction(int, VMEntity* caller, VMContext* [, const std::vector<int16_t>*])` → `shared_ptr<VMQueuedAction>`, `PushUserInteraction(...)`, `GetPieMenu(VM*, VMEntity*, bool)`, `PositionValid(pos, dir, ctx)` → `VMPlacementResult`, `SetPosition(pos, dir, ctx)`, `Delete(bool cleanupAll, VMContext*)`, `Reset(VMContext*)`, `IsInUse(ctx, multitile [, stackObjSafety])`, `IsUserMovable`, `TotalSlots/PlaceInSlot/GetSlot/ClearSlot/GetSlotHeight`, `GetObstacle`, `UpdateDynamicMultitile`, `ToString()`. Enums `VMEntityFlags, VMEntityFlags2, WallPlacementFlags, VMPlacementFlags, VMMovementFlags`; class `VMPieMenuInteraction`.
Type tests: use `dynamic_cast<VMAvatar*>(e)` / `dynamic_cast<VMGameObject*>(e)` for C# `is`.

`VMAvatar`: `GetPersonData/SetPersonData(VMPersonDataVariable, int16_t)`, `GetMotiveData/SetMotiveData(VMMotive, int16_t)`, `SetMotiveChange(VMMotive, int16_t perHour, int16_t max)`, `HasMotiveChange`, `ClearMotiveChanges`, `IsPet()/IsDog()/IsCat()`, `std::vector<std::shared_ptr<Model::VMAnimationState>> Animations`, `CurrentAnimationState()`, `shared_ptr<VMAnimationState> CarryAnimationState`, `WalkAnimations/SwimAnimations (array<string,50>)`, `Message()/Message(str)`, `DefaultSuits`, `BoundAppearances`, `BodyOutfit()/HeadOutfit()/SkinTone()` (+setters), `AvatarState` (VMTSOAvatarState*), `Avatar` (SimAvatar), `UserLeaveLot()`, `Visitor`, statics `TEMPLATE_PERSON` etc.

`VMGameObject`: `Disabled` (`VMGameObjectDisableFlags`), `RefreshGraphic()`, `RefreshLight()`, `PartOfPortal()`, `DisableIfTSOCategoryWrong(ctx)`.

`Entities::VMMultitileGroup`: `MultiTile, Name, Price, SalePrice, Objects (vector<VMEntity*>), Offsets, BaseObject()`, `ChangePosition(pos, dir, ctx)`, `GetInteractionGroupLeader`, `ExecuteEntryPoint`, `Delete`, `Init`, `Combine`, `AddObject`.

`VMContext`: `Architecture()` (VMArchitecture*), `Blueprint`, `Clock` (`->Hours/Minutes/Seconds()/TimeOfDay()`), `Globals` (GameGlobal*), `RoomInfo` (vector<VMRoomInfo>), `SetToNextCache` (`GetObjectsAt/GetObjectsByGUID/GetObjectsByCategory/Avatars`), `ObjectQueries`, `Ambience`, `VM`, `Primitives` (array of 256 `unique_ptr<VMPrimitiveRegistration>`),
`uint64_t NextRandom(uint64_t max)`, `CreateObjectInstance(uint32_t GUID, LotTilePos, Direction, bool ts1)` / `(…, bool ghostImage, bool ts1)` / `(…, int16_t MainStackOBJ, int16_t MainParam, bool ghostImage, bool ts1)` → `VMMultitileGroup*` (null if GUID unknown), `RemoveObjectInstance`, `GhostCopyGroup`, `GetRoomAt(pos)`, `GetObjectRoom(e)`, `GetRoomScore(room)`, `IsOutOfBounds(pos)`, `IsUserOutOfBounds(pos)`, `SolidToAvatars(pos)` → `VMSolidResult {Solid, Chair}`, `GetAvatarPlace/GetObjPlace`, `RegisterObjectPos/UnregisterObjectPos`, `RefreshLighting`, `RegeneratePortalInfo`, `AddPrimitive(...)`.

`VM`: `Context`, `Entities` (vector<VMEntity*>, sorted by ObjectID), `GlobalState`, `TS1`, `PlatformState`, `TSOState()`, `TuningCache`, `MyInventory`, `GetObjectById(int16_t)`, `GetObjectByPersist(uint32_t)`, `GetGlobalValue/SetGlobalValue(uint16_t, int16_t)`, `VMRoutine* Assemble(BHAV*)`, `SignalDialog(VMDialogInfo&)`, `OnDialog` (std::function), `GlobalLink()` (null on 3DS), `CheckGlobalLink` (VMTSOGlobalLinkStub: `PerformTransaction(vm, testOnly, uid1, uid2, amount[, callback])`), statics `UseWorld()`, `AddToObjList`, `DeleteFromObjList`.

Routing (`engine/VMRoutingFrame.h`): `VMRoutingFrame : VMStackFrame` with `bool InitRoutes(SLOTItem*, VMEntity* target)`, `bool InitRoutes(const std::vector<VMFindLocationResult>&)`, `State`, `WaitTime`, `CallFailureTrees`, `WalkInterrupt(int)`, `InvalidateRoomRoute()`.
`engine/VMSlotParser.h`: `VMSlotParser(SLOTItem*)`, `FindAvaliableLocations(VMEntity* obj, VMContext*, VMEntity* caller)` → `std::vector<VMFindLocationResult>`, `FailCode`, `Blocker`, statics `GetSearchDirection`, `RadianToFlags`, `GetDirectionTo`.
`engine/VMSuitProvider.h`: `VMSuitProvider::GetSuit(frame, VMSuitScope, uint16_t)` → `VMSuitResult` (C# `object`: `OutfitID` ulong / `AppearanceName` string / null).
`engine/VMDialogHandler.h`: `ShowDialog(frame, VMDialogOperand*, STR*)`, `ParseDialogString(frame, str, STR*, bool hasInput=true)`.

Model enums/classes (exact C# names): `VMVariableScope, VMAnimationScope, VMSlotScope, VMSuitScope, VMPersonSuits, VMMotive, VMPersonDataVariable, VMStackObjectVariable, VMOBJDVariable, VMPlacementError, VMPlaceRequestFlags, VMGenericTSOCallMode, VMRouteFailCode, VMAnimationState, VMMotiveChange, VMRuntimeHeadline, VMDialogInfo, VMBudget, VMInventoryItem, VMRoomInfo/VMRoom/VMRoomPortal, VMTSOAvatarState/VMTSOAvatarPermissions, VMTSOObjectState, VMTSOLotState, VMAsyncState`.

### Operand classes owned by the core (do NOT redefine in primitives — include these headers)

| C# class | Header |
|---|---|
| `VMSubRoutineOperand` (from VMSubRoutine.cs) | `simantics/engine/VMSubRoutineOperand.h` |
| `VMDialogOperand`, `VMDialogFlags`, `VMDialogType`, `VMDialogResult` (from VMDialogPrivateStrings.cs) | `simantics/engine/VMDialogOperand.h` |
| `VMSetBalloonHeadlineOperand`, `VMSetBalloonHeadlineOperandGroup` | `simantics/engine/VMSetBalloonHeadlineOperand.h` |
| `VMChangeActionString` + operand (engine/ in C#) | `simantics/engine/VMChangeActionString.h` |

Expected from the primitive agents: `primitives/VMFindLocationFor.h` declaring
`static bool VMFindLocationFor::FindLocationFor(VMEntity* obj, VMEntity* refObj, VMContext* context);` (used by `VMAvatar::Tick`, picked up via `__has_include`).

## Registration

C# registered every primitive inline in the `VMContext` constructor. Here the constructor calls
`void FSO::SimAntics::RegisterDefaultPrimitives(VMContext* context)` (declared in `VMContext.h`). The core ships a
**weak** empty definition; the primitives work defines the strong one in `primitives/VMPrimitiveTable.cpp`:

```cpp
#include "simantics/VMContext.h"
using namespace FSO::SimAntics::Engine;
namespace FSO { namespace SimAntics {
void RegisterDefaultPrimitives(VMContext* c) {
    auto reg = new VMPrimitiveRegistration(new Primitives::VMSleep());   // registration owns the handler
    reg->Opcode = 0;
    reg->Name = "sleep";
    reg->OperandModel = VMPrimitiveRegistration::Model<Primitives::VMSleepOperand>(); // C# typeof(VMSleepOperand)
    c->AddPrimitive(reg);   // or AddPrimitive(std::unique_ptr<VMPrimitiveRegistration>)
    ...
}
}}
```

The table (opcode / Name / handler / OperandModel) exactly as in FreeSims `VMContext.cs`:

| Op | Name | Handler | OperandModel |
|---|---|---|---|
| 0 | sleep | VMSleep | VMSleepOperand |
| 1 | generic_sims_online_call | VMGenericTSOCall | VMGenericTSOCallOperand |
| 2 | expression | VMExpression | VMExpressionOperand |
| 4 | grab | VMGrab | VMGrabOperand |
| 5 | drop | VMDrop | VMDropOperand |
| 6 | change_suit_or_accessory | VMChangeSuitOrAccessory | VMChangeSuitOrAccessoryOperand |
| 7 | refresh | VMRefresh | VMRefreshOperand |
| 8 | random_number | VMRandomNumber | VMRandomNumberOperand |
| 11 | get_distance_to | VMGetDistanceTo | VMGetDistanceToOperand |
| 12 | get_direction_to | VMGetDirectionTo | VMGetDirectionToOperand |
| 13 | push_interaction | VMPushInteraction | VMPushInteractionOperand |
| 14 | find_best_object_for_function | VMFindBestObjectForFunction | VMFindBestObjectForFunctionOperand |
| 15 | breakpoint | VMBreakPoint | VMBreakPointOperand |
| 16 | find_location_for | VMFindLocationFor | VMFindLocationForOperand |
| 17 | idle_for_input | VMIdleForInput | VMIdleForInputOperand |
| 18 | remove_object_instance | VMRemoveObjectInstance | VMRemoveObjectInstanceOperand |
| 20 | run_functional_tree | VMRunFunctionalTree | VMRunFunctionalTreeOperand |
| 22 | look_towards | VMLookTowards | VMLookTowardsOperand |
| 23 | play_sound | VMPlaySound | VMPlaySoundOperand |
| 24 | old_relationship | VMRelationship | VMOldRelationshipOperand |
| 25 | transfer_funds | VMTransferFunds | VMTransferFundsOperand |
| 26 | relationship | VMRelationship | VMRelationshipOperand |
| 27 | goto_relative | VMGotoRelativePosition | VMGotoRelativePositionOperand |
| 28 | run_tree_by_name | VMRunTreeByName | VMRunTreeByNameOperand |
| 29 | set_motive_deltas | VMSetMotiveChange | VMSetMotiveChangeOperand |
| 30 | syslog | VMSysLog | VMSysLogOperand |
| 31 | set_to_next | VMSetToNext | VMSetToNextOperand |
| 32 | test_object_type | VMTestObjectType | VMTestObjectTypeOperand |
| 35 | special_effect | VMSpecialEffect | VMSpecialEffectOperand |
| 36 | dialog_private | VMDialogPrivateStrings | VMDialogOperand |
| 37 | test_sim_interacting_with | VMTestSimInteractingWith | VMTestSimInteractingWithOperand |
| 38 | dialog_global | VMDialogGlobalStrings | VMDialogOperand |
| 39 | dialog_semiglobal | VMDialogSemiGlobalStrings | VMDialogOperand |
| 40 | online_jobs_call | VMOnlineJobsCall | VMOnlineJobsCallOperand |
| 41 | set_balloon_headline | VMSetBalloonHeadline | VMSetBalloonHeadlineOperand |
| 42 | create_object_instance | VMCreateObjectInstance | VMCreateObjectInstanceOperand |
| 43 | drop_onto | VMDropOnto | VMDropOntoOperand |
| 44 | animate | VMAnimateSim | VMAnimateSimOperand |
| 45 | goto_routing_slot | VMGotoRoutingSlot | VMGotoRoutingSlotOperand |
| 46 | snap | VMSnap | VMSnapOperand |
| 47 | reach | VMReach | VMReachOperand |
| 48 | stop_all_sounds | VMStopAllSounds | VMStopAllSoundsOperand |
| 49 | stackobj_notify_out_of_idle | VMNotifyOutOfIdle | VMAnimateSimOperand |
| 50 | change_action_string | VMChangeActionString (core) | VMChangeActionStringOperand |
| 62 | invoke_plugin | VMInvokePlugin | VMInvokePluginOperand |
| 63 | get_terrain_info | VMGetTerrainInfo | VMGetTerrainInfoOperand |
| 65 | find_best_action | VMFindBestAction | VMFindBestActionOperand |
| 67 | inventory_operations | VMInventoryOperations | VMInventoryOperationsOperand |

Opcodes ≥ 256 are subroutine calls (handled by `VMThread`, operand `VMSubRoutineOperand`); unregistered opcodes < 256 act as `GOTO_TRUE`.

## Running a lot (host or 3DS)

```cpp
FSO::Content::Content::Init("/path/to/The Sims", "/path/for/objects.cache");  // needs GameData/Objects/Objects.far, GameData/Global/Global.far
FSO::SimAntics::VM::UseWorld(false);                       // no renderer yet
FSO::SimAntics::VMContext ctx(nullptr);
FSO::SimAntics::Model::VMNullHeadlineProvider headlines;
FSO::SimAntics::VM vm(&ctx, &headlines);
vm.Init();                                                 // loads global.iff, global state
FSO::Files::Formats::IFF::IffFile house("House02.iff");
VMWorldActivator(&vm, nullptr).LoadFromIff(&house);        // architecture + objects (ARRY/OBJM/OBJT decode)
for (;;) vm.Update();                                      // 2 updates per VM tick (alternate tick, like FreeSims)
```

First `Content::Init` scans every OBJD in the archives and writes `objects.cache` (GUID → archive entry);
later runs load the cache. Verified on host with the test data: 739 objects indexed, House02/House05 load
(57/104 entities, rooms generated) and tick.
