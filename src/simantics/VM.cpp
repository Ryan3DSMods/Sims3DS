/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.VM
#include "simantics/VM.h"
#include "hit/HITSound.h"
#include "simantics/VMContext.h"
#include "simantics/VMAI.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/entities/VMGameObject.h"
#include "simantics/entities/VMMultitileGroup.h"
#include "simantics/engine/VMThread.h"
#include "simantics/engine/VMTS1Travel.h"
#include "simantics/engine/translator/VMTranslator.h"
#include "simantics/model/TSOPlatform/VMTSOLotState.h"
#include "content/ContentManager.h"
#include <algorithm>
#include <stdexcept>

namespace FSO { namespace SimAntics {
using namespace FSO::SimAntics::Model;
using FSO::Files::Formats::IFF::Chunks::BHAV;

static bool _UseWorld = true;
bool VM::UseWorld() { return _UseWorld; }
void VM::UseWorld(bool value) {
    _UseWorld = value;
    VMContext::UseWorld = value;
    VMEntity::UseWorld = value;
}

TSOPlatform::VMTSOLotState* VM::TSOState() const {
    return (PlatformState != nullptr && PlatformState->IsTSOLotState()) ? (TSOPlatform::VMTSOLotState*)PlatformState.get() : nullptr;
}

std::unordered_map<BHAV*, std::unique_ptr<VMRoutine>>& VM::_Assembled() {
    static std::unordered_map<BHAV*, std::unique_ptr<VMRoutine>> map;
    return map;
}
std::vector<VM*>& VM::LiveVMs() { // subscribers of the static OnBHAVChange event
    static std::vector<VM*> list;
    return list;
}

VM::VM(VMContext* context, VMHeadlineRendererProvider* headline) {
    context->VM = this;
    this->Context = context;
    Headline = headline;
    TuningCache.reset(new VMTuningCache());
    FreeWill.reset(new VMFreeWill(this));
    LiveVMs().push_back(this); // OnBHAVChange += VM_OnBHAVChange;
}

VM::~VM() {
    auto& l = LiveVMs();
    l.erase(std::remove(l.begin(), l.end(), this), l.end());
    for (auto& e : EntityStore) e->Thread.reset(); // threads reference entities; drop them first
}

VMEntity* VM::OwnEntity(VMEntity* entity) {
    EntityStore.emplace_back(entity);
    return entity;
}
Entities::VMMultitileGroup* VM::OwnGroup(Entities::VMMultitileGroup* group) {
    GroupStore.emplace_back(group);
    return group;
}

/// Gets an entity from this VM.
VMEntity* VM::GetObjectById(int16_t id) {
    auto it = ObjectsById.find(id);
    if (it != ObjectsById.end()) return it->second;
    return nullptr;
}

VMEntity* VM::GetObjectByPersist(uint32_t id) {
    for (auto x : Entities) if (x->PersistID == id) return x;
    return nullptr;
}

/// Initializes this Virtual Machine.
void VM::Init() {
    Context->Globals = FSO::Content::Content::Get()->WorldObjectGlobals->Get("global", false);
    PlatformState = std::make_shared<TSOPlatform::VMTSOLotState>();
    PlatformState->ActivateValidator(this);
    GlobalState.assign(38, 0); // FIX(sims): FreeSO `new short[38]` (TS1 globals 0-37)
    GlobalState[20] = 255; //Game Edition. Basically, what "expansion packs" are running. Let's just say all of them.
    GlobalState[25] = 4; //as seen in EA-Land edith's simulator globals, this needs to be set for people to do their idle interactions.
    GlobalState[17] = 4; //Runtime Code Version, is this in EA-Land.
    // STUB(3ds): if (Driver is VMServerDriver) EODHost = new VMEODHost();
}

void VM::Reset() {
    std::vector<VMEntity*> avatars;
    for (auto x : Entities) if (dynamic_cast<VMAvatar*>(x) != nullptr && x->PersistID > 65535) avatars.push_back(x);
    //TODO: all avatars with persist ID are not npcs in TSO. right now though everything has a persist ID...
    for (auto avatar : avatars) avatar->Delete(true, Context);

    auto ents = Entities;
    for (auto ent : ents) {
        if (ent->Thread != nullptr) {
            if (ent->Thread->BlockingState != nullptr) ent->Thread->BlockingState = nullptr;
            if (ent->Thread->EODConnection != nullptr) ent->Thread->EODConnection = nullptr;
        }
        if (ent->Object->OBJ->GUID == 0x3929AADC) ent->Delete(true, Context);
    }
    //foreach (var ent in ents) ent.Reset(Context); duplicates dogs apparently?? tf
}

void VM::UpdateTuning() {
    TuningCache->UpdateTuning(this);
}

void VM::Update() {
    if (LastFrameSpeed != SpeedMultiplier) {
        // C# collected the sounds of SoundEntities, a set FreeSims never fills (so its pause never happened).
        // TS1(sims): every entity's sounds: TVs, radios and loops pause with the game, as in The Sims.
        std::vector<std::shared_ptr<FSO::HIT::HITSound>> allSounds;
        for (auto ent : Entities) {
            for (auto& x : ent->SoundThreads) if (x.Sound != nullptr) allSounds.push_back(x.Sound);
        }

        if (SpeedMultiplier < 1 && LastFrameSpeed >= 1) for (auto& x : allSounds) x->Pause();
        else if (SpeedMultiplier >= 1 && LastFrameSpeed < 1) for (auto& x : allSounds) x->Resume();
        LastFrameSpeed = SpeedMultiplier;
    }

    if (!Ready || AlternateTick) {
        Tick();
    } else {
        //fractional animation for avatars
        for (auto obj : Entities) {
            auto ava = dynamic_cast<VMAvatar*>(obj);
            if (ava != nullptr) ava->FractionalAnim(0.5f);
        }
    }
    AlternateTick = !AlternateTick;
}

void VM::Tick() {
    if (BHAVDirty) {
        for (auto ent : Entities) if (ent->Thread != nullptr) ent->Thread->RoutineDirty = true;
        BHAVDirty = false;
    }

    //Tick for the Free Will control
    // TS1(sims): FreeSims' random visitor "free will" is not TS1 behaviour (FreeSO removed it): TS1 Sims and
    // visitors choose through their own main trees ('try autonomy' -> Find Best Interaction).
    if (Context->Blueprint != nullptr && !TS1)
        FreeWill->Tick(this);

    // STUB(3ds): C# ran `Driver.Tick(this)` (VMNetDriver: local/client/server) which applied queued net
    // commands and then called InternalTick. Single player on 3DS: tick directly.
    InternalTick();
    Ready = true;
}

void VM::InternalTick() {
    // STUB(3ds): GlobalLink.Tick / EODHost.Tick — no TSO server.
    Context->Clock->Tick();
    GlobalState[6] = (int16_t)Context->Clock->Seconds();
    GlobalState[5] = (int16_t)Context->Clock->Minutes;
    GlobalState[0] = (int16_t)Context->Clock->Hours;
    // TS1(sims): FreeSO GetGlobalValue reads Day of Month / Month / Year (globals 1, 7, 8) from the clock.
    GlobalState[1] = (int16_t)Context->Clock->DayOfMonth;
    GlobalState[7] = (int16_t)Context->Clock->Month;
    GlobalState[8] = (int16_t)Context->Clock->Year;
    GlobalState[4] = (int16_t)Context->Clock->TimeOfDay();

    Context->Architecture()->Tick();

    auto entCpy = Entities;
    for (auto obj : entCpy) {
        Context->NextRandom(1);
        obj->Tick(); //run object specific tick behaviors, like lockout count decrement
    }
    if (TS1) Engine::Utils::VMTS1Travel::KeepSelectedSim(this); // FIX(sims) H1: global 3 names a living Sim
    //Context.SetToNextCache.VerifyPositions(); use only for debug!
}

/// Adds an entity to this Virtual Machine.
void VM::AddEntity(VMEntity* entity) {
    // TS1(sims): a saved object being restored takes its saved id (see PendingIDsByGUID).
    bool pending = false;
    if (!PendingIDsByGUID.empty() && entity->Object != nullptr && entity->Object->OBJ != nullptr) {
        auto pit = PendingIDsByGUID.find(entity->Object->OBJ->GUID);
        if (pit != PendingIDsByGUID.end() && pit->second > 0 && ObjectsById.find(pit->second) == ObjectsById.end()) {
            entity->ObjectID = pit->second;
            ReservedObjectIDs.erase(pit->second);
            PendingIDsByGUID.erase(pit);
            pending = true;
        }
    }
    if (!pending) {
        if (ObjectsById.find(ObjectId) != ObjectsById.end() || ReservedObjectIDs.count(ObjectId)) ObjectId = NextObjID();
        entity->ObjectID = ObjectId;
    }
    ObjectsById[entity->ObjectID] = entity;
    AddToObjList(this->Entities, entity);
    if (!entity->GhostImage) Context->SetToNextCache->NewObject(entity);
    ObjectId = NextObjID();
}

void VM::AddToObjList(std::vector<VMEntity*>& list, VMEntity* entity) {
    if (list.size() == 0) { list.push_back(entity); return; }
    int id = entity->ObjectID - 1;
    int max = (int)list.size() - 1;
    int min = 0;
    while (max - 1 > min) {
        int mid = (max + min) / 2;
        int nid = list[mid]->ObjectID;
        if (id < nid) max = mid;
        else min = mid;
    }
    int index = (list[min]->ObjectID > id) ? min : ((list[max]->ObjectID > id) ? max : max + 1);
    list.insert(list.begin() + index, entity);
}

/// Removes an entity from this Virtual Machine.
void VM::RemoveEntity(VMEntity* entity) {
    auto it = std::find(Entities.begin(), Entities.end(), entity);
    if (it != Entities.end()) {
        Context->SetToNextCache->RemoveObject(entity);
        Entities.erase(it);
        ObjectsById.erase(entity->ObjectID);
        if (entity->ObjectID < ObjectId) ObjectId = entity->ObjectID; //this id is now the smallest free object id.
    }
    entity->Dead = true;
}

void VM::DeleteFromObjList(std::vector<VMEntity*>& list, VMEntity* entity) {
    if (list.size() == 0) { return; }
    int id = entity->ObjectID;
    int max = (int)list.size();
    int min = 0;
    while (max > min) {
        int mid = (max + min) / 2;
        int nid = list[mid]->ObjectID;
        if (id < nid) max = mid;
        else if (id == nid) {
            list.erase(list.begin() + mid); //found it
            return;
        }
        else min = mid + 1;
    }
    //list.RemoveAt(min);
}

/// Finds the next free object ID and remembers it for use when making another object.
int16_t VM::NextObjID() {
    for (int16_t i = ObjectId; i > 0; i++)
        if (ObjectsById.find(i) == ObjectsById.end() && ReservedObjectIDs.count(i) == 0) return i; // TS1(sims): reserved ids
    return 0;
}

int16_t VM::GetGlobalValue(uint16_t var) {
    // should this be in VMContext?
    // FIX(sims): FreeSO bounds globals by the array (TS1 has 38: Behavior.iff STR#129, 33 "Machine Level" ..
    // 37 "Filter Flags for Transit Screen" are read by the expansion packs' controllers). FreeSims capped at 32.
    if (var >= GlobalState.size()) throw std::runtime_error("Global Access out of bounds!");
    return GlobalState[var];
}

bool VM::SetGlobalValue(uint16_t var, int16_t value) {
    if (var >= GlobalState.size()) throw std::runtime_error("Global Access out of bounds!"); // FIX(sims): see Get
    GlobalState[var] = value;
    return true;
}

VMRoutine* VM::Assemble(BHAV* bhav) {
    auto& assembled = _Assembled();
    auto it = assembled.find(bhav);
    if (it != assembled.end()) return it->second.get();
    auto routine = Engine::VMTranslator::Assemble(this, bhav);
    auto result = routine.get();
    assembled[bhav] = std::move(routine);
    return result;
}

bool VM::ReleaseAssembled() { // TS1(sims): see VM.h
    if (!LiveVMs().empty()) return false;
    auto& assembled = _Assembled();
    assembled.clear();
    std::unordered_map<BHAV*, std::unique_ptr<VMRoutine>>().swap(assembled);
    return true;
}

void VM::BHAVChanged(BHAV* bhav) {
    bhav->RuntimeVer++;
    // NOTE: C# drops the cached routine; frames may still point at it, so we keep the old one alive
    // by leaking it into a graveyard instead of freeing it.
    static std::vector<std::unique_ptr<VMRoutine>> graveyard;
    auto& assembled = _Assembled();
    auto it = assembled.find(bhav);
    if (it != assembled.end()) { graveyard.push_back(std::move(it->second)); assembled.erase(it); }
    for (auto vm : LiveVMs()) vm->VM_OnBHAVChange();
}

void VM::SignalDialog(VMDialogInfo& info) {
    if (OnDialog) OnDialog(info);
}

std::unique_ptr<VMSandboxRestoreState> VM::Sandbox() {
    std::unique_ptr<VMSandboxRestoreState> state(new VMSandboxRestoreState());
    state->Entities = Entities;
    state->ObjectId = ObjectId;
    state->ObjectsById = ObjectsById;
    state->SetToNext = std::move(Context->SetToNextCache);

    Context->SetToNextCache.reset(new VMSetToNextCache(Context));
    Entities.clear();
    ObjectsById.clear();
    ObjectId = 1;

    return state;
}

void VM::SandboxRestore(std::unique_ptr<VMSandboxRestoreState> state) {
    Entities = state->Entities;
    ObjectsById = state->ObjectsById;
    ObjectId = state->ObjectId;
    Context->SetToNextCache = std::move(state->SetToNext);
}

void VM::BreakpointHit(VMEntity* entity) {
    if (!OnBreakpoint) entity->Thread->ThreadBreak = Engine::VMThreadBreakMode::Active; //no handler..
    else OnBreakpoint(entity);
}

}}
