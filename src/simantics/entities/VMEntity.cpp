/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.VMEntity (entities/VMEntity.cs)
#include "simantics/entities/VMEntity.h"
#include "hit/HITVM.h"
#include <cmath>
#include "simantics/entities/VMAvatar.h"
#include "simantics/entities/VMGameObject.h"
#include "simantics/entities/VMMultitileGroup.h"
#include "simantics/engine/VMThread.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/model/TSOPlatform/VMTSOEntityState.h"
#include "simantics/model/VMSetToNextCache.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"
#include "simantics/VMArchitecture.h"
#include "simantics/primitives/VMFindLocationFor.h" // FIX(sims) QA4
#include "common/utils/DirectionUtils.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace FSO { namespace SimAntics {
using namespace FSO::Files::Formats::IFF::Chunks;
using namespace FSO::SimAntics::Model;
using namespace FSO::LotView::Model;
using FSO::Common::Utils::DirectionUtils;
using Engine::VMThread;
using Engine::VMStackFrame;

bool VMEntity::UseWorld = true;

Model::TSOPlatform::VMTSOEntityState* VMEntity::TSOState() const {
    return (PlatformState != nullptr && PlatformState->IsTSOEntityState()) ? (Model::TSOPlatform::VMTSOEntityState*)PlatformState.get() : nullptr;
}

int16_t VMEntity::GetAttribute(int32_t index) {
    while (index >= (int32_t)Attributes.size()) Attributes.push_back(0);
    return Attributes[index];
}

void VMEntity::SetAttribute(int32_t index, int16_t value) {
    while (index >= (int32_t)Attributes.size()) Attributes.push_back(0);
    Attributes[index] = value;
}

std::string VMEntity::Name() const {
    if (MultitileGroup->Name != "") return MultitileGroup->Name;
    if (dynamic_cast<const VMAvatar*>(this) != nullptr) return "Sim";
    else return const_cast<VMEntity*>(this)->ToString();
}

void VMEntity::Name(const std::string& value) {
    MultitileGroup->Name = value;
}

bool VMEntity::GetBadObjects() {
    bool badobject = false;
    // FIX(sims): FreeSims deleted the Vacation "Car - Vacation Wagon" (0xA7888650 / its row 0 tile 0xA7C2E4B1)
    // on every tick as a "bad object" (a workaround for its engine). The vacation ped marker's 'Generate Cab' creates
    // it to bring the family, and then reset on the empty group. FreeSO has no such list; nothing is deleted here.
    (void)badobject;
    return false;
}

bool VMEntity::GetBadNames() {
    std::string name = this->ToString();
    BadName = false;
    auto contains = [&](const char* s) { return name.find(s) != std::string::npos; };
    if (contains("Mail") || contains("Window") || contains("Door") || contains("Lamp")
        || contains("Stair") || contains("Arch") || contains("Tree") || contains("Portal")
        || contains("Trash") || contains("Phone") || contains("Fire") || contains("NPC"))
        BadName = true;
    return BadName;
}

void VMEntity::Position(const LotTilePos& value) {
    _Position = value;
    if (UseWorld && WorldUI) WorldUI->Level = _Position.Level;
    VisualPosition(st::Vector3(_Position.x / 16.0f, _Position.y / 16.0f, (_Position.Level - 1) * 2.95f));
}

/// Constructs a new VMEntity instance.
VMEntity::VMEntity(FSO::Content::GameObject* obj) {
    this->Object = obj;
    /**
     * For some reason, in the aquarium object (maybe others) the numAttributes is set to 0
     * but it should be 4. There are 4 entries in the label table. Go figure?
     */

    if (EntryPoints.size() > 0)
        Portal = EntryPoints[15].ActionFunction != 0;

    ObjectData.assign(80, 0);

    auto numAttributes = obj->OBJ->NumAttributes;

    if (obj->OBJ->UsesFnTable == 0) EntryPoints = GenerateFunctionTable(obj->OBJ);
    else {
        auto OBJfChunk = obj->Resource->Get<OBJf>(obj->OBJ->ChunkID); //objf has same id as objd
        if (OBJfChunk != nullptr) EntryPoints = OBJfChunk->functions;
    }
    if (EntryPoints.size() < 33) EntryPoints.resize(33); //guard: trees index up to 32

    if (obj->GUID == 0xa9bb3a76) EntryPoints[17] = OBJfFunctionEntry();

    SemiGlobal = obj->Resource->SemiGlobal;

    Slots = obj->Resource->Get<SLOT>(obj->OBJ->SlotID); //containment slots are dealt with in the avatar and object classes respectively.

    auto attributeTable = obj->Resource->Get<STR>(256);
    if (attributeTable != nullptr) {
        numAttributes = (uint16_t)std::max((int32_t)numAttributes, attributeTable->Length());
        RTTI.AttributeLabels.resize(numAttributes);
        for (int i = 0; i < numAttributes; i++) {
            RTTI.AttributeLabels[i] = attributeTable->GetString(i);
        }
    }

    TreeTable = obj->Resource->Get<TTAB>(obj->OBJ->TreeTableID);
    if (TreeTable != nullptr) TreeTableStrings = obj->Resource->Get<TTAs>(obj->OBJ->TreeTableID);
    if (TreeTable == nullptr && SemiGlobal != nullptr) {
        TreeTable = SemiGlobal->Get<TTAB>(obj->OBJ->TreeTableID); //tree not in local, try semiglobal
        TreeTableStrings = SemiGlobal->Get<TTAs>(obj->OBJ->TreeTableID);
    }
    //no you cannot get global tree tables don't even ask

    this->Attributes.reserve(numAttributes);
    SetFlag(VMEntityFlags::ChairFacing, true);
}

VMEntity::~VMEntity() {}

void VMEntity::UseTreeTableOf(FSO::Content::GameObject* obj, bool ts1) {
    if (TreeTable != nullptr) return;
    auto GLOBChunks = obj->Resource->List<GLOB>();
    FSO::Content::GameGlobal* SemiGlobal = nullptr;

    if (!GLOBChunks.empty() && GLOBChunks[0]->Name != "") SemiGlobal = FSO::Content::Content::Get()->WorldObjectGlobals->Get(GLOBChunks[0]->Name, ts1);

    TreeTable = obj->Resource->Get<TTAB>(obj->OBJ->TreeTableID);
    if (TreeTable != nullptr) TreeTableStrings = obj->Resource->Get<TTAs>(obj->OBJ->TreeTableID);
    if (TreeTable == nullptr && SemiGlobal != nullptr) {
        TreeTable = SemiGlobal->Resource->Get<TTAB>(obj->OBJ->TreeTableID); //tree not in local, try semiglobal
        TreeTableStrings = SemiGlobal->Resource->Get<TTAs>(obj->OBJ->TreeTableID);
    }
}

void VMEntity::UseSemiGlobalTTAB(const std::string& sgFile, uint16_t id, bool ts1) {
    auto obj = FSO::Content::Content::Get()->WorldObjectGlobals->Get(sgFile, ts1);
    if (obj == nullptr) return;
    TreeTable = obj->Resource->Get<TTAB>(id);
    if (TreeTable != nullptr) TreeTableStrings = obj->Resource->Get<TTAs>(id);
}

// TS1(sims): FreeSO VMEntity.GetUsers.
void VMEntity::GetUsers(VMContext* context, std::unordered_set<VMEntity*>* users) {
    for (auto ava : context->SetToNextCache->Avatars) { // C#: ObjectQueries.Avatars (here only SetToNextCache tracks avatars)
        if (ava == this || ava->Thread == nullptr) continue;
        bool found = false;
        auto& q = ava->Thread->Queue;
        for (int i = 0; i <= ava->Thread->ActiveQueueBlock && i < (int)q.size(); i++) {
            if (q[i]->Callee == this) { found = true; break; }
        }
        // TSO/TS1 expects the use count of the 2 sim interaction object to be zero until both sims are actively using it.
        // ... so only count active actions.
        if (found) users->insert(ava);
    }
}

int16_t VMEntity::UseCount() {
    if (Thread == nullptr) return 0;
    std::unordered_set<VMEntity*> users;
    if (MultitileGroup != nullptr) for (auto obj : MultitileGroup->Objects) obj->GetUsers(Thread->Context, &users);
    else GetUsers(Thread->Context, &users);
    return (int16_t)users.size();
}

bool VMEntity::IsInUse(VMContext* context, bool multitile) { return IsInUse(context, multitile, false); }

bool VMEntity::IsInUse(VMContext* context, bool multitile, bool stackObjSafety) {
    if (multitile) {
        for (auto obj : MultitileGroup->Objects) {
            if (obj->IsInUse(context, false)) return true;
        }
    } else {
        if (GetFlag(VMEntityFlags::Occupied)) return true;
        for (auto ava : context->VM->Entities) {
            if (dynamic_cast<VMAvatar*>(ava) != nullptr && ava->Thread != nullptr)
                for (auto& item : ava->Thread->Stack) {
                    if (item->Callee == this) return true;
                    if (stackObjSafety && item->StackObject == this) return true;
                }
        }
    }
    return false;
}

void VMEntity::Tick() {
    //decrement lockout count
    if (Thread != nullptr) {
        Thread->TicksThisFrame = 0;
        Thread->Tick();
        if (SoundThreads.size() > 0) TickSounds();
    }
    if (Headline != nullptr && HeadlineRenderer != nullptr) {
        auto over = HeadlineRenderer->Update();
        if (over) {
            HeadlineRenderer->Dispose();
            Headline.reset();
            HeadlineRenderer.reset();
        } else if (UseWorld && WorldUI) {
            WorldUI->Headline = HeadlineRenderer->DrawFrame(Thread->Context->World);
        }
    }
    if (UseWorld && WorldUI && Headline == nullptr && WorldUI->Headline != nullptr) {
        WorldUI->Headline = nullptr;
    }
    if (ObjectData[(int)VMStackObjectVariable::LockoutCount] > 0) ObjectData[(int)VMStackObjectVariable::LockoutCount]--;
}

void VMEntity::TickSounds() {
    if (!UseWorld) return;
    if (Thread != nullptr) {
        // C#: scrPos = WorldUI.GetScreenPos(worldState) - half the view. The world component's VM-facing
        // GetScreenPos is a stub here, so the screen position is computed from the world space directly
        // (same formula as ObjectComponent.GetScreenPos).
        auto world = Thread->Context != nullptr ? Thread->Context->World : nullptr;
        st::Vector2 scrPos(0, 0);
        float pxWidth = 1.0f;
        int zoom = 3, level = 1;
        bool havePos = false;
        if (world != nullptr && WorldUI != nullptr) {
            auto& worldState = world->State;
            auto& worldSpace = worldState.WorldSpace;
            auto dp = WorldUI->DrawPosition();
            if (!(dp.X < -2043 && dp.Y < -2043)) { // not out of world
                if (dynamic_cast<VMGameObject*>(this) != nullptr) dp = st::Vector3(dp.X + 0.5f, dp.Y + 0.5f, dp.Z);
                scrPos = worldSpace.GetScreenFromTile(dp) + worldSpace.GetScreenOffset();
                scrPos = scrPos - st::Vector2(worldSpace.WorldPxWidth / 2, worldSpace.WorldPxHeight / 2);
                pxWidth = worldSpace.WorldPxWidth > 0 ? worldSpace.WorldPxWidth : 1.0f;
                zoom = (int)worldState.Zoom();
                level = worldState.Level();
                havePos = true;
            }
        }
        for (size_t i = 0; i < SoundThreads.size(); i++) {
            if (SoundThreads[i].Sound == nullptr || SoundThreads[i].Sound->Dead) {
                auto old = SoundThreads[i];
                SoundThreads.erase(SoundThreads.begin() + i); i--;
                if (old.Loop && FSO::HIT::HITVM::Get() != nullptr) {
                    auto thread = FSO::HIT::HITVM::Get()->PlaySoundEvent(old.Name);
                    if (thread != nullptr) {
                        auto owner = this;
                        if (!thread->AlreadyOwns(owner->ObjectID)) thread->AddOwner(owner->ObjectID);

                        Model::Sound::VMSoundEntry entry;
                        entry.Sound = thread;
                        entry.Pan = old.Pan;
                        entry.Zoom = old.Zoom;
                        entry.Loop = old.Loop;
                        entry.Name = old.Name;
                        owner->SoundThreads.push_back(entry);
                    }
                }
                continue;
            }

            float pan = 0, volume = 1;
            if (havePos) {
                pan = (SoundThreads[i].Pan) ? std::max(-1.0f, std::min(1.0f, scrPos.X / pxWidth)) : 0;
                volume = (SoundThreads[i].Pan) ? 1 - (float)std::max(0.0, std::min(1.0, std::sqrt((double)scrPos.X * scrPos.X + (double)scrPos.Y * scrPos.Y) / pxWidth)) : 1;

                if (SoundThreads[i].Zoom) volume /= 4 - zoom;
                if (Position().Level > level) volume /= 4;
                else if (Position().Level != level) volume /= 2;
            }

            SoundThreads[i].Sound->SetVolume(volume, pan);
        }
    }
}

std::vector<Model::Sound::VMSoundTransfer> VMEntity::GetActiveSounds() {
    std::vector<Model::Sound::VMSoundTransfer> result;
    for (auto& snd : SoundThreads) {
        result.push_back(Model::Sound::VMSoundTransfer(ObjectID, Object->OBJ->GUID, snd));
    }
    return result;
}

std::vector<OBJfFunctionEntry> VMEntity::GenerateFunctionTable(OBJD* obj) {
    std::vector<OBJfFunctionEntry> result(33);

    result[0].ActionFunction = obj->BHAV_Init;
    result[1].ActionFunction = obj->BHAV_MainID;
    result[2].ActionFunction = obj->BHAV_Load;
    result[3].ActionFunction = obj->BHAV_Cleanup;
    result[4].ActionFunction = obj->BHAV_QueueSkipped;
    result[5].ActionFunction = obj->BHAV_AllowIntersectionID;
    result[6].ActionFunction = obj->BHAV_WallAdjacencyChanged;
    result[7].ActionFunction = obj->BHAV_RoomChange;
    result[8].ActionFunction = obj->BHAV_DynamicMultiTileUpdate;
    result[9].ActionFunction = obj->BHAV_Place;
    result[10].ActionFunction = obj->BHAV_Pickup;
    result[11].ActionFunction = obj->BHAV_UserPlace;
    result[12].ActionFunction = obj->BHAV_UserPickup;
    result[13].ActionFunction = obj->BHAV_LevelInfo;
    result[14].ActionFunction = obj->BHAV_ServingSurface;
    result[15].ActionFunction = obj->BHAV_Portal; //portal
    result[16].ActionFunction = obj->BHAV_GardeningID;
    result[17].ActionFunction = obj->BHAV_WashHandsID;
    result[18].ActionFunction = obj->BHAV_PrepareFoodID;
    result[19].ActionFunction = obj->BHAV_CookFoodID;
    result[20].ActionFunction = obj->BHAV_PlaceSurfaceID;
    result[21].ActionFunction = obj->BHAV_DisposeID;
    result[22].ActionFunction = obj->BHAV_EatID;
    result[23].ActionFunction = obj->BHAV_PickupFromSlotID; //pickup from slot
    result[24].ActionFunction = obj->BHAV_WashDishID;
    result[25].ActionFunction = obj->BHAV_EatSurfaceID;
    result[26].ActionFunction = obj->BHAV_SitID;
    result[27].ActionFunction = obj->BHAV_StandID;
    result[28].ActionFunction = obj->BHAV_Clean;
    result[29].ActionFunction = obj->BHAV_Repair; //repair
    result[30].ActionFunction = 0; //client house join
    result[31].ActionFunction = 0; //prepare for sale
    result[32].ActionFunction = 0; //house unload

    return result;
}

void VMEntity::Init(VMContext* context) {
    GenerateTreeByName(context);
    if (!GhostImage) this->Thread.reset(new VMThread(context, this, this->Object->OBJ->StackSize));

    ExecuteEntryPoint(0, context, true); //Init

    if (!GhostImage) {
        std::vector<int16_t> Args;
        const std::vector<int16_t>* ArgsPtr = nullptr;
        VMEntity* StackOBJ = nullptr;
        if (MainParam != 0) {
            Args.assign(4, 0);
            Args[0] = MainParam;
            ArgsPtr = &Args;
            MainParam = 0;
        }
        if (MainStackOBJ != 0) {
            StackOBJ = context->VM->GetObjectById(MainStackOBJ);
            MainStackOBJ = 0;
        }

        ExecuteEntryPoint(1, context, false, StackOBJ, ArgsPtr); //Main
    } else {
        SetValue(VMStackObjectVariable::Room, -1);
        auto go = dynamic_cast<VMGameObject*>(this);
        if (go != nullptr) go->RefreshLight();
    }
}

void VMEntity::Reset(VMContext* context) {
    if (this->Thread == nullptr) return;
    this->Thread->Stack.clear();
    this->Thread->Queue.clear();
    Thread->QueueDirty = true;
    this->Thread->ActiveQueueBlock = -1; // TS1(sims): FreeSO queue model
    this->Thread->BlockingState = nullptr;
    this->Thread->EODConnection = nullptr;

    Reseted = false;

    if (EntryPoints[3].ActionFunction != 0) ExecuteEntryPoint(3, context, true); //Reset
    if (!GhostImage) ExecuteEntryPoint(1, context, false); //Main
}

void VMEntity::GenerateTreeByName(VMContext* /*context*/) {
    TreeByName = &Object->Resource->TreeByName;
}

bool VMEntity::ExecuteEntryPoint(int32_t entry, VMContext* context, bool runImmediately) {
    return ExecuteEntryPoint(entry, context, runImmediately, nullptr, nullptr);
}

bool VMEntity::ExecuteEntryPoint(int32_t entry, VMContext* context, bool runImmediately, VMEntity* stackOBJ) {
    return ExecuteEntryPoint(entry, context, runImmediately, stackOBJ, nullptr);
}

bool VMEntity::ExecuteEntryPoint(int32_t entry, VMContext* context, bool runImmediately, VMEntity* stackOBJ, const std::vector<int16_t>* argsIn) {
    std::vector<int16_t> args = (argsIn == nullptr) ? std::vector<int16_t>(4, 0) : *argsIn;
    if (entry == 11) {
        //user placement, hack to do auto floor removal/placement for stairs
        if (Object->OBJ->LevelOffset > 0 && Position() != LotTilePos::OUT_OF_WORLD) {
            auto floor = context->Architecture()->GetFloor(Position().TileX(), Position().TileY(), Position().Level);
            auto placeFlags = (VMPlacementFlags)ObjectData[(int)VMStackObjectVariable::PlacementFlags];
            if ((placeFlags & VMPlacementFlags::InAir) > 0)
                context->Architecture()->SetFloor(Position().TileX(), Position().TileY(), Position().Level, FloorTile(), true);
            if ((placeFlags & VMPlacementFlags::OnFloor) > 0 && (floor.Pattern == 0)) {
                FloorTile ft; ft.Pattern = 1;
                context->Architecture()->SetFloor(Position().TileX(), Position().TileY(), Position().Level, ft, true);
            }
        }
    }

    bool result = false;
    if (entry < (int32_t)EntryPoints.size() && EntryPoints[entry].ActionFunction > 255) {
        std::unique_ptr<VMSandboxRestoreState> SandboxState;
        if (GhostImage && runImmediately) {
            SandboxState = context->VM->Sandbox();
            for (size_t i = 0; i < MultitileGroup->Objects.size(); i++) {
                auto obj = MultitileGroup->Objects[i];
                context->VM->AddEntity(obj);
            }
        }

        uint16_t ActionID = EntryPoints[entry].ActionFunction;
        auto tree = GetBHAVWithOwner(ActionID, context);

        if (tree != nullptr) {
            auto routine = context->VM->Assemble(tree->bhav);
            auto frame = std::make_shared<VMStackFrame>();
            frame->Caller = this;
            frame->Callee = this;
            frame->CodeOwner = tree->owner;
            frame->Routine = routine;
            frame->StackObject = stackOBJ;
            frame->Args = args;

            if (runImmediately) {
                auto checkResult = VMThread::EvaluateCheck(context, this, frame);
                result = (checkResult == Engine::VMPrimitiveExitCode::RETURN_TRUE);
            } else {
                Thread->Push(frame);
            }
        }

        if (GhostImage && runImmediately) {
            //restore state
            context->VM->SandboxRestore(std::move(SandboxState));
        }
        return result;
    } else {
        return false;
    }
}

std::unique_ptr<VMBHAVOwnerPair> VMEntity::GetBHAVWithOwner(uint16_t ActionID, VMContext* context) {
    BHAV* bhav = nullptr;
    FSO::Content::GameObject* CodeOwner;

    if (ActionID < 4096) { //global
        bhav = context->Globals->Resource->Get<BHAV>(ActionID);
    } else if (ActionID < 8192) { //local
        bhav = Object->Resource->Get<BHAV>(ActionID);
    } else if (SemiGlobal != nullptr) {
        bhav = SemiGlobal->Get<BHAV>(ActionID);
    }

    CodeOwner = Object;

    if (bhav == nullptr) return nullptr;
    return std::unique_ptr<VMBHAVOwnerPair>(new VMBHAVOwnerPair(bhav, CodeOwner));
}

VMPlacementError VMEntity::IndividualUserMovable(VMContext* context, bool deleting) {
    if (dynamic_cast<VMAvatar*>(this) != nullptr) return VMPlacementError::CantBePickedup;
    auto movementFlags = (VMMovementFlags)GetValue(VMStackObjectVariable::MovementFlags);
    if ((movementFlags & VMMovementFlags::PlayersCanMove) == 0) return VMPlacementError::CantBePickedup;
    if (deleting && (movementFlags & VMMovementFlags::PlayersCanDelete) == 0) return VMPlacementError::ObjectNotOwnedByYou;
    if (context->IsUserOutOfBounds(Position())) return VMPlacementError::CantBePickedupOutOfBounds;
    if (IsInUse(context, true)) return VMPlacementError::InUse;
    auto total = TotalSlots();
    for (int i = 0; i < total; i++) {
        auto item = GetSlot(i);
        if (item != nullptr &&
            (deleting || dynamic_cast<VMAvatar*>(item) != nullptr || item->IsUserMovable(context, deleting) != VMPlacementError::Success)) return VMPlacementError::CantBePickedup;
    }
    return VMPlacementError::Success;
}

VMPlacementError VMEntity::IsUserMovable(VMContext* context, bool deleting) {
    for (auto obj : MultitileGroup->Objects) {
        auto result = obj->IndividualUserMovable(context, deleting);
        if (result != VMPlacementError::Success) return result;
    }
    return VMPlacementError::Success;
}

void VMEntity::FetchTreeByName(VMContext* /*context*/) {
    TreeByName = &Object->Resource->TreeByName;
}

bool VMEntity::IsDynamicSpriteFlagSet(uint16_t index) const {
    return (index > 63) ? ((DynamicSpriteFlags2 & ((uint64_t)0x1 << (index - 64))) > 0) :
        (DynamicSpriteFlags & ((uint64_t)0x1 << index)) > 0;
}

void VMEntity::SetDynamicSpriteFlag(uint16_t index, bool set) {
    if (set) {
        if (index > 63) {
            uint64_t bitflag = ((uint64_t)0x1 << (index - 64));
            DynamicSpriteFlags2 = DynamicSpriteFlags2 | bitflag;
        } else {
            uint64_t bitflag = ((uint64_t)0x1 << index);
            DynamicSpriteFlags = DynamicSpriteFlags | bitflag;
        }
    } else {
        if (index > 63) {
            DynamicSpriteFlags2 = DynamicSpriteFlags2 & (~((uint64_t)0x1 << (index - 64)));
        } else {
            DynamicSpriteFlags = DynamicSpriteFlags & (~((uint64_t)0x1 << index));
        }
    }
}

void VMEntity::SetFlag(VMEntityFlags flag, bool set) {
    if (set) ObjectData[(int)VMStackObjectVariable::Flags] |= (int16_t)(flag);
    else ObjectData[(int)VMStackObjectVariable::Flags] &= ((int16_t)~(int32_t)(flag));

    if (flag == VMEntityFlags::HasZeroExtent) Footprint = GetObstacle(Position(), Direction());
    return;
}

bool VMEntity::GetFlag(VMEntityFlags flag) const {
    return ((VMEntityFlags)ObjectData[(int)VMStackObjectVariable::Flags] & flag) > 0;
}

int16_t VMEntity::GetValue(VMStackObjectVariable var) {
    switch (var) //special cases
    {
        case VMStackObjectVariable::ObjectId:
            return ObjectID;
        case VMStackObjectVariable::Direction:
            switch (this->Direction()) {
                case DirectionT::WEST: return 6;
                case DirectionT::SOUTH: return 4;
                case DirectionT::EAST: return 2;
                case DirectionT::NORTH: return 0;
                default: return 0;
            }
        case VMStackObjectVariable::ContainerId:
        case VMStackObjectVariable::ParentId: //TODO: different?
            return (Container == nullptr) ? (int16_t)0 : Container->ObjectID;
        case VMStackObjectVariable::SlotNumber:
            return (Container == nullptr) ? (int16_t)-1 : ContainerSlot;
        case VMStackObjectVariable::SlotCount:
            return (int16_t)TotalSlots();
        case VMStackObjectVariable::UseCount:
            // TS1(sims): FreeSO — "TS1 Person to Person: use count is checked to see if both avatars still have the
            // interaction in their queues." It is computed by the engine (no shipped tree writes object data 62).
            if (Thread != nullptr && Thread->Context->VM->TS1) return UseCount();
            break;
        default: break;
    }
    if ((int16_t)var > 79 || (int16_t)var < 0) throw std::runtime_error("Object Data out of range!");
    return ObjectData[(int16_t)var];
}

bool VMEntity::SetValue(VMStackObjectVariable var, int16_t value) {
    switch (var) //special cases
    {
        case VMStackObjectVariable::Flags: {
            // FIX(sims) QA4: FreeSO stores the new flags *before* rebuilding the footprint (UpdateFootprint). FreeSims
            // rebuilt it from the old flags, so setting 'has zero extent' kept the old footprint and clearing it left
            // none: the Unleashed rabbit ('Sub - Setup for Move' sets zero extent, creates its Reserve Tile, then snaps
            // back to its own tile - often a garden it was eating) was refused by its own stale footprint and hit Maxis'
            // breakpoint (QA3 Q6), and objects that clear the flag stayed walk-through.
            auto old = ObjectData[(int16_t)var];
            ObjectData[(int16_t)var] = value;
            if (((value ^ old) & (int32_t)VMEntityFlags::HasZeroExtent) > 0)
                Footprint = GetObstacle(Position(), Direction());
            if (dynamic_cast<VMAvatar*>(this) != nullptr && ((value ^ old) & (int32_t)VMEntityFlags::Burning) > 0)
                this->Reset(Thread->Context);
            return true;
        }
        case VMStackObjectVariable::Direction:
            value = (int16_t)(((int32_t)value + 65536) % 8);
            switch (value) {
                case 6: Direction(DirectionT::WEST); return true;
                case 4: Direction(DirectionT::SOUTH); return true;
                case 2: Direction(DirectionT::EAST); return true;
                case 0: Direction(DirectionT::NORTH); return true;
                default:
                    return true;
                    //throw new Exception("Diagonal Set Not Implemented!");
            }
        case VMStackObjectVariable::Hidden:
            if (UseWorld && WorldUI) WorldUI->Visible = value == 0;
            break;
        case VMStackObjectVariable::Category:
            if (Thread != nullptr) {
                Thread->Context->SetToNextCache->RemoveCategory(this, ObjectData[(int16_t)var]);
                Thread->Context->SetToNextCache->RegisterCategory(this, value);
            }
            break;
        // case VMStackObjectVariable.CurrentValue:
        //   MultitileGroup.InitialPrice = value;
        //   break;
        default: break;
    }

    if ((int16_t)var > 79 || (int16_t)var < 0) throw std::runtime_error("Object Data out of range!");
    ObjectData[(int16_t)var] = value;
    return true;
}

void VMEntity::RecurseSlotPositionChange(VMContext* context, bool noEntryPoint) {
    context->UnregisterObjectPos(this);
    auto total = TotalSlots();
    for (int i = 0; i < total; i++) {
        auto obj = GetSlot(i);
        if (obj != nullptr) obj->RecurseSlotPositionChange(context, noEntryPoint);
    }
    Position(Position());
    PositionChange(context, noEntryPoint);
}

void VMEntity::SetRoom(uint16_t room) {
    SetValue(VMStackObjectVariable::Room, (int16_t)room);
}

std::vector<VMPieMenuInteraction> VMEntity::GetPieMenu(VM* vm, VMEntity* caller, bool includeHidden) {
    std::vector<VMPieMenuInteraction> pie;
    if (TreeTable == nullptr) return pie;

    for (size_t i = 0; i < TreeTable->Interactions.size(); i++) {
        auto id = TreeTable->Interactions[i]->TTAIndex;
        auto action = GetAction((int32_t)id, caller, vm->Context);

        caller->ObjectData[(int)VMStackObjectVariable::HideInteraction] = 0;
        auto actionStrings = caller->Thread->CheckAction(action); // TS1(sims): FreeSO (FreeSims cleared MustRun = TS1AllowDogs here)
        if ((caller->ObjectData[(int)VMStackObjectVariable::Hidden] == 1 ||
            caller->ObjectData[(int)VMStackObjectVariable::HideInteraction] == 1 ||
            caller->Position() == LotTilePos::OUT_OF_WORLD) && !includeHidden) continue;

        if (actionStrings != nullptr) {
            if (actionStrings->size() > 0) {
                for (auto& actionS : *actionStrings) {
                    if (actionS.Name.empty()) actionS.Name = (TreeTableStrings != nullptr) ? TreeTableStrings->GetString((int32_t)id) : std::string("***MISSING***"); // TS1(sims): FreeSO
                    actionS.ID = (uint8_t)id;
                    actionS.Entry = TreeTable->Interactions[i].get();
                    pie.push_back(actionS);
                }
            } else {
                if (TreeTableStrings != nullptr) {
                    VMPieMenuInteraction item;
                    item.Name = TreeTableStrings->GetString((int32_t)id);
                    item.ID = (uint8_t)id;
                    item.Entry = TreeTable->Interactions[i].get();
                    pie.push_back(item);
                }
            }
        }
    }
    return pie;
}

std::shared_ptr<Engine::VMQueuedAction> VMEntity::GetAction(int32_t interaction, VMEntity* caller, VMContext* context) {
    return GetAction(interaction, caller, context, nullptr);
}

std::shared_ptr<Engine::VMQueuedAction> VMEntity::GetAction(int32_t interaction, VMEntity* /*caller*/, VMContext* context, const std::vector<int16_t>* args) {
    if (TreeTable == nullptr) return nullptr;
    auto it = TreeTable->InteractionByIndex.find((uint32_t)interaction);
    if (it == TreeTable->InteractionByIndex.end()) return nullptr;
    auto Action = it->second;

    uint16_t actionID = Action->ActionFunction;
    auto aTree = GetBHAVWithOwner(actionID, context);
    if (aTree == nullptr) return nullptr;
    auto aRoutine = context->VM->Assemble(aTree->bhav);

    VMRoutine* cRoutine = nullptr;
    uint16_t checkID = Action->TestFunction;
    if (checkID != 0) {
        auto cTree = GetBHAVWithOwner(checkID, context);
        if (cTree == nullptr) return nullptr; // TS1(sims): FreeSO "VERIFIED: TS1 ignores interactions with non-zero missing check trees."
        cRoutine = context->VM->Assemble(cTree->bhav);
    }

    auto result = std::make_shared<Engine::VMQueuedAction>();
    result->Callee = this;
    result->IconOwner(this);
    result->CodeOwner = aTree->owner;
    result->ActionRoutine = aRoutine;
    result->CheckRoutine = cRoutine;
    result->Name = (TreeTableStrings == nullptr) ? "" : TreeTableStrings->GetString((int32_t)Action->TTAIndex);
    result->StackObject = this;
    if (args != nullptr) result->Args = *args;
    result->InteractionNumber = interaction;
    result->Priority = (int16_t)Engine::VMQueuePriority::UserDriven;
    result->Flags = Action->Flags;
    result->Flags2 = Action->Flags2;
    return result;
}

void VMEntity::PushUserInteraction(int32_t interaction, VMEntity* caller, VMContext* context) {
    PushUserInteraction(interaction, caller, context, nullptr);
}

void VMEntity::PushUserInteraction(int32_t interaction, VMEntity* caller, VMContext* context, const std::vector<int16_t>* args) {
    auto action = GetAction(interaction, caller, context, args);
    if (action != nullptr) caller->Thread->EnqueueAction(action);
}

VMPlacementResult VMEntity::PositionValid(const LotTilePos& pos, DirectionT direction, VMContext* context) {
    if (pos == LotTilePos::OUT_OF_WORLD) return VMPlacementResult();
    else if (context->IsOutOfBounds(pos)) return VMPlacementResult(VMPlacementError::LocationOutOfBounds);

    //TODO: speedup with exit early checks
    //TODO: corner checks (wtf uses this)

    auto arch = context->Architecture();
    auto wall = arch->GetWall(pos.TileX(), pos.TileY(), pos.Level); //todo: preprocess to check which walls are real solid walls and not fences.

    if (dynamic_cast<VMGameObject*>(this) != nullptr) //needs special handling for avatar eventually
    {
        VMPlacementError wallValid = WallChangeValid(wall, direction, true);
        if (wallValid != VMPlacementError::Success) return VMPlacementResult(wallValid);
    }

    auto floor = arch->GetFloor(pos.TileX(), pos.TileY(), pos.Level);
    VMPlacementError floorValid = FloorChangeValid(floor, pos.Level);
    if (floorValid != VMPlacementError::Success) return VMPlacementResult(floorValid);

    // TS1(sims): FreeSO — objects on floors need level ground
    if (!context->IsUserOutOfBounds(pos) && arch->GetTerrainSloped(pos.TileX(), pos.TileY())) {
        auto slopeValid = SlopeValid();
        if (slopeValid != VMPlacementError::Success) return VMPlacementResult(slopeValid);
    }

    //we've passed the wall test, now check if we intersect any objects.
    auto valid = (dynamic_cast<VMAvatar*>(this) != nullptr) ? context->GetAvatarPlace(this, pos, direction) : context->GetObjPlace(this, pos, direction);
    return valid;
}

VMPlacementError VMEntity::SlopeValid() {
    if (dynamic_cast<VMAvatar*>(this) != nullptr) return VMPlacementError::Success;
    auto placeFlags = (VMPlacementFlags)ObjectData[(int)VMStackObjectVariable::PlacementFlags];
    if ((placeFlags & VMPlacementFlags::OnFloor) > 0 && (placeFlags & VMPlacementFlags::OnSlope) == 0) {
        return VMPlacementError::CantPlaceOnSlope;
    }
    return VMPlacementError::Success;
}

VMPlacementError VMEntity::FloorChangeValid(const FloorTile& floor, int8_t level) {
    auto placeFlags = (VMPlacementFlags)ObjectData[(int)VMStackObjectVariable::PlacementFlags];

    if (floor.Pattern == 65535) {
        if ((placeFlags & (VMPlacementFlags::AllowOnPool | VMPlacementFlags::RequirePool)) == 0) return VMPlacementError::CantPlaceOnWater;
    } else {
        if ((placeFlags & VMPlacementFlags::RequirePool) > 0) return VMPlacementError::MustPlaceOnPool;
        if (floor.Pattern == 65534) { // FIX(sims): was 63354 (FreeSO: 65534 = water)
            if ((placeFlags & (VMPlacementFlags::OnWater | VMPlacementFlags::RequireWater)) == 0) return VMPlacementError::CantPlaceOnWater;
        } else {
            if ((placeFlags & VMPlacementFlags::RequireWater) > 0) return VMPlacementError::MustPlaceOnWater;
            if (floor.Pattern == 0) {
                if (level == 1) {
                    if ((placeFlags & VMPlacementFlags::OnTerrain) == 0) return VMPlacementError::NotAllowedOnTerrain;
                } else {
                    if ((placeFlags & VMPlacementFlags::InAir) == 0 && Object->OBJ->LevelOffset == 0) return VMPlacementError::CantPlaceInAir;
                    //TODO: special hack check that determines if we need can add/remove a tile to fulfil this if LevelOffset > 0
                }
            } else {
                if ((placeFlags & VMPlacementFlags::OnFloor) == 0 &&
                    ((Object->OBJ->LevelOffset == 0) || (placeFlags & VMPlacementFlags::InAir) == 0)) return VMPlacementError::NotAllowedOnFloor;
            }
        }
    }
    return VMPlacementError::Success;
}

VMPlacementError VMEntity::WallChangeValid(const WallTile& wall, DirectionT direction, bool checkUnused) {
    auto placeFlags = (WallPlacementFlags)ObjectData[(int)VMStackObjectVariable::WallPlacementFlags];

    bool diag = ((wall.Segments & (WallSegments::HorizontalDiag | WallSegments::VerticalDiag)) > 0);
    if (diag && (placeFlags & WallPlacementFlags::DiagonalAllowed) == 0) return VMPlacementError::CantBeThroughWall; //does not allow diagonal and one is present
    else if (!diag && ((placeFlags & WallPlacementFlags::DiagonalRequired) > 0)) return VMPlacementError::MustBeOnDiagonal; //needs diagonal and one is not present

    int rotate = (DirectionToWallOff(direction) + 1) % 4;
    int rotPart = RotateWallSegs(wall.Segments, rotate);
    int useRotPart = RotateWallSegs(wall.OccupiedWalls, rotate);

    if (((int)placeFlags & rotPart) != ((int)placeFlags & 15)) return VMPlacementError::MustBeAgainstWall; //walls required are not there in this configuration

    //walls that we are attaching to must not be in use!
    if (checkUnused && ((int)placeFlags & useRotPart) > 0) return VMPlacementError::MustBeAgainstUnusedWall;

    if (((int)placeFlags & (rotPart << 8)) > 0) return VMPlacementError::CantBeThroughWall; //walls not allowed are there in this configuration

    return VMPlacementError::Success;
}

int32_t VMEntity::RotateWallSegs(WallSegments ws, int32_t rotate) {
    int wallSides = (int)ws;
    int rotPart = ((wallSides << (4 - rotate)) & 15) | ((wallSides & 15) >> rotate);
    return rotPart;
}

void VMEntity::SetWallUse(VMArchitecture* arch, bool set) {
    auto wall = arch->GetWall(Position().TileX(), Position().TileY(), Position().Level);

    auto placeFlags = (WallPlacementFlags)ObjectData[(int)VMStackObjectVariable::WallPlacementFlags];
    int rotate = (8 - (DirectionToWallOff(Direction()) + 1)) % 4;
    uint8_t rotPart = (uint8_t)RotateWallSegs((WallSegments)((int)placeFlags % 15), rotate);
    SetValue(VMStackObjectVariable::WallAdjacencyFlags, (int16_t)RotateWallSegs(wall.Segments, rotate));

    if (rotPart == 0) return;

    if (UseWorld && WorldUI) ((FSO::LotView::Components::ObjectComponent*)WorldUI.get())->AdjacentWall = (WallSegments)rotPart;

    if (set) wall.OccupiedWalls |= (WallSegments)rotPart;
    else wall.OccupiedWalls &= (WallSegments)~rotPart;

    arch->SetWall(Position().TileX(), Position().TileY(), Position().Level, wall);
}

void VMEntity::Delete(bool cleanupAll, VMContext* context) {
    if (cleanupAll) MultitileGroup->Delete(context);
    else {
        auto& threads = SoundThreads;
        for (size_t i = 0; i < threads.size(); i++) {
            if (threads[i].Sound != nullptr) threads[i].Sound->RemoveOwner(ObjectID);
        }
        threads.clear();

        PrePositionChange(context);
        context->RemoveObjectInstance(this);
        MultitileGroup->RemoveObject(this); //we're no longer part of the multitile group

        // FIX(sims) QA4: FreeSO first takes each contained object out of the world (always succeeds and detaches it),
        // unhides it, then finds it a place next to us. FreeSims only tried SetPosition(our position), which fails
        // when that spot is not valid for the object (a meal held by a Sim who leaves Downtown): the meal kept a
        // deleted Sim as its container and FoodHD 'held by person' ('so id 0 := so 2' -> 'so def 9') reset the
        // janitor on every check (Downtown 24, seed 4).
        int slots = TotalSlots();
        for (int i = 0; i < slots; i++) {
            auto obj = GetSlot(i);
            if (obj != nullptr) {
                auto here = obj->Position();
                obj->SetPosition(LotTilePos::OUT_OF_WORLD, obj->Direction(), context);
                if (obj->GetValue(VMStackObjectVariable::Hidden) > 0) obj->SetValue(VMStackObjectVariable::Hidden, 0);
                if (here != LotTilePos::OUT_OF_WORLD) {
                    Position(here);
                    FSO::SimAntics::Primitives::VMFindLocationFor::FindLocationFor(obj, this, context);
                }
            }
        }
    }
}

void VMEntity::PrePositionChange(VMContext* /*context*/) {
}

void VMEntity::PositionChange(VMContext* context, bool noEntryPoint) {
    Footprint = GetObstacle(Position(), Direction());
    if (!(GhostImage || noEntryPoint)) ExecuteEntryPoint(9, context, true); //Placement
}

VMPlacementResult VMEntity::SetPosition(const LotTilePos& pos, DirectionT direction, VMContext* context) {
    return MultitileGroup->ChangePosition(pos, direction, context);
}

void VMEntity::SetIndivPosition(const LotTilePos& pos, DirectionT direction, VMContext* context, const VMPlacementResult& info) {
    Direction(direction);
    if (UseWorld && dynamic_cast<VMGameObject*>(this) != nullptr && context->Blueprint != nullptr) context->Blueprint->ChangeObjectLocation((FSO::LotView::Components::ObjectComponent*)WorldUI.get(), pos);
    Position(pos);
    if (info.Object != nullptr) info.Object->PlaceInSlot(this, 0, false, context);
}

int32_t VMEntity::DirectionToWallOff(DirectionT dir) {
    switch (dir) {
        case DirectionT::NORTH: return 0;
        case DirectionT::EAST: return 1;
        case DirectionT::SOUTH: return 2;
        case DirectionT::WEST: return 3;
    }
    return 0;
}

void VMEntity::SetWallStyle(int32_t side, VMArchitecture* arch, uint16_t value) {
    //0=top right, 1=bottom right, 2=bottom left, 3 = top left
    WallTile targ;
    switch (side) {
        case 0:
            targ = arch->GetWall(Position().TileX(), Position().TileY(), Position().Level);
            targ.ObjSetTRStyle = value;
            if (((VMEntityFlags2)ObjectData[(int)VMStackObjectVariable::FlagField2] & VMEntityFlags2::ArchitectualDoor) > 0) targ.TopRightDoor = value != 0;
            arch->SetWall(Position().TileX(), Position().TileY(), Position().Level, targ);
            break;
        case 1:
            //this seems to be the rule... only set if wall is top left/right. Fixes multitile windows (like really long ones)
            return;
        case 2:
            return;
        case 3:
            targ = arch->GetWall(Position().TileX(), Position().TileY(), Position().Level);
            targ.ObjSetTLStyle = value;
            if (((VMEntityFlags2)ObjectData[(int)VMStackObjectVariable::FlagField2] & VMEntityFlags2::ArchitectualDoor) > 0) targ.TopLeftDoor = value != 0;
            arch->SetWall(Position().TileX(), Position().TileY(), Position().Level, targ);
            break;
    }
}

void VMEntity::UpdateDynamicMultitile(VMContext* context) {
    //check adjacent tiles for objects of this type and add them to our multitile group
    //also build adjacency flags for use by the Dynamic Multitile Update entry point
    int flags = 0;
    auto entities = context->VM->Entities; //copy: Combine mutates groups
    for (auto ent : entities) {
        if (ent == this) continue;
        auto diff = ent->Position() - Position();
        if (ent->Object->OBJ->GUID == this->Object->OBJ->GUID && std::abs(diff.TileX()) < 2 && std::abs(diff.TileY()) < 2) {
            if (ent->MultitileGroup != MultitileGroup) {
                auto oldObjects = ent->MultitileGroup->Objects;
                MultitileGroup->Combine(ent->MultitileGroup);
                for (auto obj : oldObjects) obj->UpdateDynamicMultitile(context);
            }

            auto direction = DirectionUtils::Normalize(std::atan2((double)(ent->Position().x - Position().x), (double)(Position().y - ent->Position().y)));
            auto result = (int)std::nearbyint((DirectionUtils::PosMod(direction, M_PI * 2) / M_PI) * 4);

            auto dirDiff = (int)DirectionUtils::PosMod(result - DirectionToWallOff(Direction()) * 2, 8);

            flags |= 1 << dirDiff;
        }
    }
    std::vector<int16_t> args = { (int16_t)flags, 0, 0, 0 };
    ExecuteEntryPoint(8, context, true, nullptr, &args);
}

}}
