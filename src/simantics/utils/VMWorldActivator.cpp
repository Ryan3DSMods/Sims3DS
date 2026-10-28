/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
#include <cstdlib>
#include <cstdio>
#include <cstring>
// VMWorldActivator (utils/VMWorldActivator.cs)
#include "simantics/utils/VMWorldActivator.h"
#include "simantics/utils/VMTS1FamilyLoader.h"
#include "simantics/engine/VMTS1Neighbourhood.h"
#include "content/TS1NeighbourProvider.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"
#include "simantics/VMArchitecture.h"
#include "simantics/VMClock.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/entities/VMMultitileGroup.h"
#include "simantics/entities/VMGameObject.h"
#include "simantics/model/VMStackObjectVariable.h"
#include "simantics/model/VMPersonDataVariable.h"
#include "simantics/model/VMMotive.h"
#include <cmath>
#include "simantics/engine/VMAmbientSound.h"
#include "content/ContentManager.h"
#include <algorithm>
#include <stdexcept>

using namespace FSO::Files::Formats::IFF::Chunks;
using namespace FSO::LotView::Model;
using namespace FSO::SimAntics;
using namespace FSO::SimAntics::Model;
using FSO::Files::Formats::IFF::IffFile;
using FSO::LotView::Components::TerrainComponent;

const std::unordered_set<uint32_t> VMWorldActivator::ControllerObjects = { 2933422533u, 3323953410u, 3936853382u, 2800949331u, 2880036682u, 2568581908u };
int16_t VMWorldActivator::MachineLevel = 2; // TS1(sims)
// TS1(sims): see LoadFromIff (Controller - Village, Controller - NPCs, Controller - Unleashed - Pet Show,
// Neighborhood Ped Marker - Spawn).
bool VMWorldActivator::IsLateController(uint32_t guid) {
    return guid == 0x49398BB2u; // FIX(sims): NPC - Vacation - Controller (see LoadFromIff)
}
bool VMWorldActivator::IsCommunityOnlyController(uint32_t guid) {
    return guid == 0xDDEBF1B8u || guid == 0xC2F1EBCEu || guid == 0xC1BA4467u || guid == 0x3F79F92Cu;
}
const std::unordered_set<uint16_t> VMWorldActivator::ValidWallStyles = { 2, 12, 14, 13 };

VMWorldActivator::VMWorldActivator(FSO::SimAntics::VM* vm, FSO::LotView::World* world) {
    VM = vm;
    World = world;
}

static void InstallBlueprint(FSO::SimAntics::VM* vm, int32_t size, VMWorldActivator::Blueprint*& out) {
    vm->Context->OwnedBlueprint.reset(new VMWorldActivator::Blueprint(size, size));
    out = vm->Context->OwnedBlueprint.get();
    vm->Context->Blueprint = out;
    vm->Context->Architecture(std::unique_ptr<VMArchitecture>(new VMArchitecture(size, size, out, vm->Context)));
}

VMWorldActivator::Blueprint* VMWorldActivator::LoadFromXML(XmlHouseData& model) {
    auto size = 0;
    if (size == 0) size = model.Size;

    InstallBlueprint(VM, model.Size, BlueprintPtr);
    VMArchitecture* arch = VM->Context->Architecture();
    for (auto& floor : model.World.Floors) {
        FloorTile ft; ft.Pattern = (uint16_t)floor.Value;
        arch->SetFloor(floor.X, floor.Y, (int8_t)(floor.Level + 1), ft, true);
    }
    for (auto& pool : model.World.Pools) {
        FloorTile ft; ft.Pattern = UINT16_MAX;
        arch->SetFloor(pool.X, pool.Y, 1, ft, true);
    }
    for (auto& wall : model.World.Walls) {
        WallTile wt;
        wt.Segments = wall.Segments;
        wt.TopLeftPattern = (uint16_t)wall.TopLeftPattern;
        wt.TopRightPattern = (uint16_t)wall.TopRightPattern;
        wt.BottomLeftPattern = (uint16_t)wall.BottomLeftPattern;
        wt.BottomRightPattern = (uint16_t)wall.BottomRightPattern;
        wt.TopLeftStyle = (uint16_t)wall.LeftStyle;
        wt.TopRightStyle = (uint16_t)wall.RightStyle;
        arch->SetWall((int16_t)wall.X, (int16_t)wall.Y, (int8_t)(wall.Level + 1), wt);
    }
    arch->RegenRoomMap();
    VM->Context->RegeneratePortalInfo();
    for (auto& obj2 : model.Objects) {
        CreateObject(obj2);
    }
    if (FSO::SimAntics::VM::UseWorld()) {
        for (auto& obj : model.Sounds) {
            VM->Context->Ambience->SetAmbience(VM->Context->Ambience->GetAmbienceFromGUID(obj.ID), obj.On == 1);
        }
        BlueprintPtr->Terrain = CreateTerrain((int16_t)model.Size);
    }
    XmlHouseDataObject testObject;
    testObject.GUID = "0x70F69082";
    testObject.X = 0;
    testObject.Y = 0;
    testObject.Level = 1;
    testObject.Dir = 0;

    if (FSO::SimAntics::VM::UseWorld() && World != nullptr) World->State.WorldSize = size;

    CreateObject(testObject);
    arch->Tick();
    return BlueprintPtr;
}

template<class T> static T* Req(T* p, const char* what) {
    if (p == nullptr) throw std::runtime_error(std::string("house iff is missing chunk ") + what);
    return p;
}

VMWorldActivator::Blueprint* VMWorldActivator::LoadFromIff(IffFile* iff) {
    SIMI* simi = Req(iff->Get<SIMI>(1), "SIMI#1");
    HOUS* hous = Req(iff->Get<HOUS>(0), "HOUS#0");
    int16_t size = simi->GlobalData[23];
    int16_t type = simi->GlobalData[35];
    (void)type;
    InstallBlueprint(VM, size, BlueprintPtr);
    FlipRoad = (hous->CameraDir & 1) > 0;
    VM->GlobalState = simi->GlobalData;
    if (VM->GlobalState.size() < 38) VM->GlobalState.resize(38, 0); // FIX(sims): 38 TS1 globals (FreeSO)
    VM->GlobalState[20] = 255;
    VM->GlobalState[25] = 4;
    VM->GlobalState[17] = 4;
    // FIX(sims): TS1 house files store their own house number in global 10 "Current House" (House21.iff: 21);
    // the expansion pack lots' BHAVs read it (zoning: "Is Lot a Community Lot?", lot descriptions, transit). FreeSims
    // forced 1; FreeSO's VMTS1Activator sets the house number it was given.
    if (VM->GlobalState[10] <= 0) VM->SetGlobalValue(10, 1);
    VM->SetGlobalValue(32, 0);
    VM->SetGlobalValue(33, MachineLevel);
    // TS1(sims): FreeSO VMTS1Activator — "Selected Sim ID. Default to 0." / "Active Family ID. Default to 0." The
    // saved values name objects of the session that saved the lot (Downtown Motives controller cast a stale id to a Sim);
    // VMTS1FamilyLoader / VMTS1Travel set them for the family that is actually here.
    VM->SetGlobalValue(3, 0);
    VM->SetGlobalValue(9, 0); // TS1(sims): FreeSO VMTS1Activator "machine level" (traffic/NPC density)
    VM->Context->Clock->Hours = VM->GlobalState[0];
    VM->Context->Clock->DayOfMonth = VM->GlobalState[1];
    VM->Context->Clock->Minutes = VM->GlobalState[5];
    VM->Context->Clock->MinuteFractions = VM->GlobalState[6] * VM->Context->Clock->TicksPerMinute;
    VM->Context->Clock->Month = VM->GlobalState[7];
    VM->Context->Clock->Year = VM->GlobalState[8];

    std::vector<WALmEntry> floorM;
    if (auto f = iff->Get<FLRm>(1)) floorM = f->Entries; else if (auto f0 = iff->Get<FLRm>(0)) floorM = f0->Entries;
    std::vector<WALmEntry> wallM;
    if (auto w = iff->Get<WALm>(1)) wallM = w->Entries; else if (auto w0 = iff->Get<WALm>(0)) wallM = w0->Entries;

    auto floorDict = BuildFloorDict(floorM);
    auto wallDict = BuildWallDict(wallM);
    VMArchitecture* arch = VM->Context->Architecture();
    ARRY* advFloors = iff->Get<ARRY>(11);
    std::vector<uint8_t> flags = Req(iff->Get<ARRY>(8), "ARRY#8")->TransposeData();
    if (advFloors != nullptr) {
        arch->Floors[0] = RemapFloors(DecodeAdvFloors(advFloors->TransposeData()), floorDict, flags);
        arch->Floors[1] = RemapFloors(DecodeAdvFloors(Req(iff->Get<ARRY>(111), "ARRY#111")->TransposeData()), floorDict, flags);
        arch->Walls[0] = RemapWalls(DecodeAdvWalls(Req(iff->Get<ARRY>(12), "ARRY#12")->TransposeData()), wallDict, floorDict);
        arch->Walls[1] = RemapWalls(DecodeAdvWalls(Req(iff->Get<ARRY>(112), "ARRY#112")->TransposeData()), wallDict, floorDict);
    } else {
        arch->Floors[0] = RemapFloors(DecodeFloors(Req(iff->Get<ARRY>(1), "ARRY#1")->TransposeData()), floorDict, flags);
        arch->Floors[1] = RemapFloors(DecodeFloors(Req(iff->Get<ARRY>(101), "ARRY#101")->TransposeData()), floorDict, flags);
        arch->Walls[0] = RemapWalls(DecodeWalls(Req(iff->Get<ARRY>(2), "ARRY#2")->TransposeData()), wallDict, floorDict);
        arch->Walls[1] = RemapWalls(DecodeWalls(Req(iff->Get<ARRY>(102), "ARRY#102")->TransposeData()), wallDict, floorDict);
    }
    arch->SignalRedraw();
    std::vector<uint8_t> pools = Req(iff->Get<ARRY>(9), "ARRY#9")->TransposeData();
    std::vector<uint8_t> water = Req(iff->Get<ARRY>(10), "ARRY#10")->TransposeData();
    for (size_t j = 0; j < pools.size() && j < arch->Floors[0].size(); j++) {
        if (pools[j] != UINT8_MAX && pools[j] != 0) {
            arch->Floors[0][j].Pattern = UINT16_MAX;
        }
        if (j < water.size() && water[j] != UINT8_MAX && water[j] != 0) {
            arch->Floors[0][j].Pattern = 65534;
        }
    }
    arch->Floors[0] = ResizeFloors(arch->Floors[0], size);
    arch->Floors[1] = ResizeFloors(arch->Floors[1], size);
    arch->Walls[0] = ResizeWalls(arch->Walls[0], size);
    arch->Walls[1] = ResizeWalls(arch->Walls[1], size);
    arch->RoofStyle = (uint32_t)FSO::Content::Content::Get()->WorldRoofs->NameToID(FSO::Content::ToLowerInvariant(hous->RoofName) + ".bmp");
    {   // HOOK(world): the TS1 HOUS field C# reads as "GUID" is the roof pitch (float: 0.4 / 0.6 / 1.0, 0 = flat roof)
        float pitch; uint32_t bits = hous->GUID; std::memcpy(&pitch, &bits, sizeof pitch);
        if (std::isfinite(pitch) && pitch >= 0.0f && pitch <= 4.0f) arch->RoofPitch = pitch;
        if (BlueprintPtr != nullptr) BlueprintPtr->RoofComp.SetStylePitch(arch->RoofStyle, arch->RoofPitch);
    }
    if (FSO::SimAntics::VM::UseWorld()) {
        if (World != nullptr) World->State.WorldSize = size;
        BlueprintPtr->Terrain = CreateTerrain(size);
        // HOOK(world): TS1 lot type by house number (FreeSO VMTS1Activator.HouseNumToType) and the lawn state
        // (ARRY 6, "GrassState = 127 - x"), cropped to the lot size like ResizeGrass.
        BlueprintPtr->Terrain->SetTS1HouseNumber(VM->GlobalState[10]);
        if (auto grass = iff->Get<ARRY>(6)) {
            auto raw = grass->TransposeData();
            int32_t gw = std::min<int32_t>(size, 64);
            std::vector<uint8_t> state((size_t)(gw * gw), 64);
            for (int32_t y = 0; y < gw; y++) for (int32_t x = 0; x < gw; x++)
                if ((size_t)(y * 64 + x) < raw.size()) state[(size_t)(y * gw + x)] = (uint8_t)(127 - raw[(size_t)(y * 64 + x)]);
            BlueprintPtr->Terrain->SetGrassState(state, gw);
        }
    }
    {   // TS1(sims): FreeSO VMTS1Activator: buildable area (ARRY 8 flag 32) and altitude (ARRY 0, byte 1 of each
        // tile = its corner vertex, x10), applied to floors/walls/objects/Sims by the renderer (Blueprint::InterpAltitude).
        arch->FineBuildableArea = ResizeFlags(flags, size);
        arch->BuildableFloors = 2; // TS1: two stories (FreeSO leaves 0, which made IsUserOutOfBounds always false)
        {   // lawn state, as the terrain mesh reads it
            if (auto grass = iff->Get<ARRY>(6)) { auto graw = grass->TransposeData();
                for (int32_t y = 0; y < size && y < 64; y++) for (int32_t x = 0; x < size && x < 64; x++)
                    if ((size_t)(y * 64 + x) < graw.size()) arch->Terrain->GrassState[(size_t)(y * size + x)] = (uint8_t)(127 - graw[(size_t)(y * 64 + x)]); }
        }
        if (auto alt = iff->Get<ARRY>(0)) {
            auto raw = alt->TransposeData();
            auto& H = arch->Terrain->Heights;
            for (int32_t y = 0; y < size && y < 64; y++) for (int32_t x = 0; x < size && x < 64; x++) {
                size_t si = (size_t)(y * 64 + x) * 4 + 1;
                if (si < raw.size()) H[(size_t)(y * size + x)] = (int16_t)(raw[si] * 10);
            }
        }
        arch->Terrain->RegenerateCenters();
        arch->SyncTerrain(false);
    }
    arch->RegenWallsAt();
    arch->RegenRoomMap();
    VM->Context->RegeneratePortalInfo();
    OBJM* objm = Req(iff->Get<OBJM>(1), "ObjM#1");
    OBJT* objt = Req(iff->Get<OBJT>(0), "objt#0");

    // TS1(sims): decode the per-object records (FreeSO OBJM.Prepare). On success objm->ObjectData holds one
    // MappedObject per saved object with its full saved state (Instance); otherwise the FreeSims heuristic stays.
    {
        auto contentP = FSO::Content::Content::Get();
        objm->Prepare([&](uint16_t typeID) {
            OBJMResource r;
            for (auto& e : objt->Entries) if (e.TypeID == typeID) { r.OBJT = &e; break; }
            if (r.OBJT != nullptr) {
                auto res = contentP->WorldObjects->Get(r.OBJT->GUID, false);
                if (res != nullptr) r.OBJD = res->OBJ;
            }
            return r;
        });
        if (getenv("WA_DEBUG")) printf("WA: OBJM version %X: %s (%zu records)%s%s\n", objm->Version, objm->Decoded ? "decoded" : "NOT decoded",
            objm->ObjectData.size(), objm->Decoded ? "" : ", ", objm->DecodeError.c_str());
    }

    // C#: builds an (unused) OBJMResource from objt.Entries[objt.OriginalID - 1].
    if (objt->OriginalID >= 1 && objt->OriginalID - 1 < (int)objt->Entries.size()) {
        auto& entr = objt->Entries[objt->OriginalID - 1];
        (void)FSO::Content::Content::Get()->WorldObjects->Get(entr.GUID, false);
    }

    for (size_t k = 0; k < objm->IDToOBJT.size(); k += 2) {
        // FIX(sims): IDToOBJT is a list of (objectID, OBJT TypeID) pairs. FreeSims indexed the
        // OBJT table with the object ID (IDToOBJT[k] - 1) instead of the paired TypeID, so most TS1
        // objects got no GUID (and in C# threw out-of-range). Verified on House02: 388/388 resolve.
        if (k + 1 >= objm->IDToOBJT.size()) break;
        auto it = objm->ObjectData.find(objm->IDToOBJT[k]);
        if (objm->IDToOBJT[k] != 0 && it != objm->ObjectData.end()) {
            uint16_t typeID = objm->IDToOBJT[k + 1];
            const OBJTEntry* entry = nullptr;
            for (auto& e : objt->Entries) if (e.TypeID == typeID) { entry = &e; break; }
            if (entry == nullptr) continue;
            it->second.Name = entry->Name;
            it->second.GUID = entry->GUID;
        }
    }

    std::vector<uint16_t> objFlrs[2];
    bool hasFlr[2];
    {
        ARRY* a3 = iff->Get<ARRY>(3);
        ARRY* a103 = iff->Get<ARRY>(103);
        std::vector<uint8_t> d3, d103;
        if (a3) d3 = a3->TransposeData();
        if (a103) d103 = a103->TransposeData();
        hasFlr[0] = DecodeObjID(a3 ? &d3 : nullptr, objFlrs[0]);
        hasFlr[1] = DecodeObjID(a103 ? &d103 : nullptr, objFlrs[1]);
    }
    for (int flr = 0; flr < 2; flr++) {
        auto& objs = objFlrs[flr];
        if (!hasFlr[flr]) {
            continue;
        }
        for (size_t i = 0; i < objs.size(); i++) {
            uint16_t obj = objs[i];
            if (obj != 0) {
                int x2 = (int)(i % 64);
                int y = (int)(i / 64);
                auto it = objm->ObjectData.find(obj);
                if (it != objm->ObjectData.end()) {
                    it->second.ArryX = x2;
                    it->second.ArryY = y;
                    it->second.ArryLevel = flr + 1;
                }
            }
        }
    }
    if (getenv("WA_DEBUG") && objm->Decoded) {
        // TS1(sims): compare the saved positions with the ARRY#3/#103 object grid.
        int agree = 0, disagree = 0, out = 0;
        for (auto& kv : objm->ObjectData) {
            auto& mo = kv.second;
            if (!mo.Instance || mo.ArryX == 0) continue;
            if (mo.Instance->X == -16) { out++; continue; }
            if (mo.Instance->X / 16 == mo.ArryX && mo.Instance->Y / 16 == mo.ArryY && mo.Instance->Level == mo.ArryLevel) agree++;
            else { disagree++; if (disagree <= 5) printf("WA: pos mismatch %s: obj (%d,%d,L%d) arry (%d,%d,L%d)\n", mo.Name.c_str(), mo.Instance->X, mo.Instance->Y, mo.Instance->Level, mo.ArryX, mo.ArryY, mo.ArryLevel); }
        }
        printf("WA: saved position vs ARRY grid: %d agree, %d disagree, %d out of world\n", agree, disagree, out);
    }
    // TS1(sims): keep the saved object ids free for the saved objects (see VM::ReservedObjectIDs).
    if (objm->Decoded) for (auto& kv : objm->ObjectData) if (kv.first > 0) VM->ReservedObjectIDs.insert((int16_t)kv.first);
    auto content = FSO::Content::Content::Get();
    // TS1(sims): FreeSO VMTS1Activator — every "global sim object" (OBJD.Global == 1, not a gift token) is
    // created on every lot: NPC/visitor controllers, the expansion packs' downtown/vacation/studio/magic town
    // controllers, phone plugins, help systems, plumb bobs. FreeSims' fixed list (phone line + 5 phone plugins) is
    // used when the content has no TS1 index.
    std::unordered_set<uint32_t> controllers(ControllerObjects);
    if (!content->WorldObjects->ControllerObjects.empty())
        controllers = std::unordered_set<uint32_t>(content->WorldObjects->ControllerObjects.begin(), content->WorldObjects->ControllerObjects.end());
    // FIX(sims): created before the lot objects, as FreeSO (a phone's init links it to the Phone Line controller), except
    // the few whose init needs the lot's own objects (IsLateController), created after them below.
    for (uint32_t controller : content->WorldObjects->ControllerObjects.empty()
             ? std::vector<uint32_t>(ControllerObjects.begin(), ControllerObjects.end()) : content->WorldObjects->ControllerObjects) {
        // TS1(sims): Unleashed's Old Town community controllers are not on residential lots. None of the 32 saved
        // residential lots of the install (incl. the Unleashed-era Old Town homes 56/63/69/76/77) contains them, while
        // all 45 Old Town community lots do, and their own code assumes a community lot (the visitor trolley "Controller
        // - Village" brings parks forever idle on a residential lot - CarVisitorTrolley 'main' - and blocked the school
        // bus there). FreeSO creates every global object everywhere.
        // FIX(sims) H1: not on any lot that is not a community lot. All four have a load tree that runs global 'Allow On
        // Community Only' (remove unless GSC 28 zoning == 1); Studio Town lots answer 2 (FreeSO), so on Studio Town the
        // Village controller brought Unleashed visitor trolleys whose 'main' idles forever unless 'Is Lot A Community
        // Lot?' - two of them parked at the curb for good and the car called by the phone never found a spot.
        if (IsCommunityOnlyController(controller) && FSO::SimAntics::Engine::Utils::VMTS1Neighbourhood::Provider() != nullptr
            && FSO::SimAntics::Engine::Utils::VMTS1Neighbourhood::Provider()->GetZoning(VM->GetGlobalValue(10)) != 1) continue;
        if (IsLateController(controller)) continue;
        VM->Context->CreateObjectInstance(controller, LotTilePos::OUT_OF_WORLD, Direction::NORTH, false);
    }
    std::vector<std::pair<VMEntity*, OBJM::MappedObject*>> ents;
    int dbgTotal=0,dbgNoRes=0,dbgMasterSkip=0,dbgCreated=0,dbgPlaceFail=0,dbgNoArry=0; bool dbg = getenv("WA_DEBUG")!=nullptr;
    for (auto& kv : objm->ObjectData) {
        dbgTotal++;
        OBJM::MappedObject* obj3 = &kv.second;
        if (controllers.count(obj3->GUID)) {
            continue;
        }
        auto res = content->WorldObjects->Get(obj3->GUID, false);
        if (res == nullptr) {
            dbgNoRes++; if (dbg && dbgNoRes<=8) printf("WA: unknown GUID %08X (objid %d)\n", obj3->GUID, kv.first);
            continue;
        }
        OBJD* objd = res->OBJ;
        if (res->OBJ->MasterID != 0) {
            std::vector<OBJD*> allObjs;
            for (auto x : res->Resource->List<OBJD>()) if (x->MasterID == res->OBJ->MasterID) allObjs.push_back(x);
            bool hasLead = std::any_of(allObjs.begin(), allObjs.end(), [](OBJD* x) { return x->MyLeadObject != 0; });
            if ((!hasLead || res->OBJ->MyLeadObject == 0) && (hasLead || res->OBJ->SubIndex != 0)) {
                dbgMasterSkip++;
                continue;
            }
            OBJD* master = nullptr;
            for (auto x : allObjs) if (x->SubIndex < 0) { master = x; break; }
            if (master == nullptr) {
                continue;
            }
            objd = master;
            obj3->GUID = master->GUID;
        }
        (void)objd;
        OBJM::MappedObject* src = obj3;
        while (src != nullptr && src->ParentID != 0) {
            auto pit = objm->ObjectData.find(src->ParentID);
            src = (pit == objm->ObjectData.end()) ? nullptr : &pit->second;
            if (src != nullptr && src->ParentID == 0) {
                obj3->ArryX = src->ArryX;
                obj3->ArryY = src->ArryY;
                obj3->ArryLevel = src->ArryLevel;
            }
        }
        LotTilePos pos = LotTilePos::OUT_OF_WORLD;
        Direction dir2 = (Direction)(1 << (obj3->Direction & 7));
        if (obj3->Instance) {
            // TS1(sims): the parts created for this object take their saved ids.
            auto inst = obj3->Instance;
            int32_t leadId = (inst->HasMultitileData && inst->MultitileData.MultitileParentID != 0) ? inst->MultitileData.MultitileParentID : obj3->ObjectID;
            VM->PendingIDsByGUID.clear();
            if (inst->HasMultitileData) {
                for (auto& pkv : objm->ObjectData) {
                    auto& part = pkv.second;
                    if (!part.Instance || !part.Instance->HasMultitileData || part.Instance->OBJT == nullptr) continue;
                    if (pkv.first == leadId || part.Instance->MultitileData.MultitileParentID == leadId)
                        VM->PendingIDsByGUID[part.Instance->OBJT->GUID] = (int16_t)pkv.first;
                }
            } else if (inst->OBJT != nullptr) {
                VM->PendingIDsByGUID[inst->OBJT->GUID] = (int16_t)obj3->ObjectID;
            }
            // positions: the saved object position (ARRY#3/103 agrees for all but a few entries; see STUBS_autonomy.md)
            if (inst->X != -16 || inst->Y != -16) {
                obj3->ArryX = inst->X / 16;
                obj3->ArryY = inst->Y / 16;
                obj3->ArryLevel = inst->Level;
            }
        }
        auto nobj2 = VM->Context->CreateObjectInstance(obj3->GUID, pos, dir2, true);
        VM->PendingIDsByGUID.clear();
        if (nobj2 != nullptr && nobj2->Objects.size() != 0) {
            dbgCreated++;
            if (obj3->ContainerID == 0 && obj3->ArryX != 0 && obj3->ArryY != 0) {
                nobj2->BaseObject()->SetPosition(LotTilePos::FromBigTile((int16_t)obj3->ArryX, (int16_t)obj3->ArryY, (int8_t)obj3->ArryLevel), dir2, VM->Context);
                if (nobj2->BaseObject()->Position() == LotTilePos::OUT_OF_WORLD) { dbgPlaceFail++; if (dbg && dbgPlaceFail<=10) printf("WA: place failed %s at (%d,%d,L%d) dir %d\n", nobj2->BaseObject()->ToString().c_str(), obj3->ArryX, obj3->ArryY, obj3->ArryLevel, obj3->Direction); }
            } else { dbgNoArry++; if (dbg && dbgNoArry<=8) printf("WA: no arry pos %s (container %d, x %d y %d)\n", nobj2->BaseObject()->ToString().c_str(), obj3->ContainerID, obj3->ArryX, obj3->ArryY); }
            ents.push_back(std::make_pair(nobj2->BaseObject(), obj3));
        }
    }
    if (dbg) printf("WA: mapped %d, unknownGUID %d, masterSkip %d, created %d, placeFail %d, noArryPos %d\n", dbgTotal, dbgNoRes, dbgMasterSkip, dbgCreated, dbgPlaceFail, dbgNoArry);
    for (auto& ent : ents) {
        OBJM::MappedObject* obj2 = ent.second;
        if (ent.first->Position() == LotTilePos::OUT_OF_WORLD && obj2->ContainerID != 0 && obj2->ArryX != 0 && obj2->ArryY != 0) {
            Direction dir = (Direction)(1 << obj2->Direction);
            ent.first->SetPosition(LotTilePos::FromBigTile((int16_t)obj2->ArryX, (int16_t)obj2->ArryY, (int8_t)obj2->ArryLevel), dir, VM->Context);
        }
    }
    // FIX(sims): the Vacation NPC controller's init ('CT - Lot Reset') spawns the Vacation Director and places her at the
    // lot's "Vacation Ped Marker - Family Spawn". Created before the lot objects it found no marker and left her out of
    // world for good; her "Director comes to you" then pushed 'Be Asked About' (priority 100) on the family forever.
    for (uint32_t controller : content->WorldObjects->ControllerObjects)
        if (IsLateController(controller)) VM->Context->CreateObjectInstance(controller, LotTilePos::OUT_OF_WORLD, Direction::NORTH, false);
    std::vector<VMEntity*> entClone = VM->Entities;
    for (auto nobj : entClone) {
        nobj->ExecuteEntryPoint(11, VM->Context, true);
    }
    VM->ReservedObjectIDs.clear(); // TS1(sims)
    if (objm->Decoded && !getenv("WA_NO_RESTORE")) RestoreSavedState(objm);
    // FIX(sims) QA2: the global controllers are created fresh on every lot load (as FreeSO's VMTS1Activator), so the
    // state the original keeps in their saved records was lost on each load: the Makin' Magic NPC controller
    // ('is salesman blocked?' = attribute 1) sent the Mystery Man with another MagiCo starter kit (+35 MagiCoins) after
    // every save/reload or trip home. Their saved attributes (house file OBJM, same GUID, in object id order) are put
    // back before their main tree runs. Not for the late Vacation controller (its init must place the director).
    if (objm->Decoded && !getenv("WA_NO_RESTORE")) {
        std::unordered_map<uint32_t, std::vector<decltype(objm->ObjectData.begin()->second.Instance.get())>> saved;
        for (auto& kv : objm->ObjectData) {
            auto& mo = kv.second;
            if (!controllers.count(mo.GUID) || IsLateController(mo.GUID) || !mo.Instance) continue;
            saved[mo.GUID].push_back(mo.Instance.get());
        }
        int restoredCtl = 0;
        for (auto e : VM->Entities) {
            if (e->Object == nullptr || e->Object->OBJ == nullptr) continue;
            auto it = saved.find(e->Object->OBJ->GUID);
            if (it == saved.end() || it->second.empty()) continue;
            auto inst = it->second.front(); it->second.erase(it->second.begin());
            for (size_t i = 0; i < inst->Attributes.size(); i++) e->SetAttribute((int32_t)i, inst->Attributes[i]);
            restoredCtl++;
        }
        if (getenv("WA_DEBUG")) printf("WA: controller attributes restored on %d controllers\n", restoredCtl);
    }
    arch->SignalRedraw();
    if (VM->Context->World != nullptr) VM->Context->World->InitBlueprint(BlueprintPtr);
    arch->Tick();
    return BlueprintPtr;
}

// TS1(sims): restore the state saved in the house file. FreeSO (VMTS1ActivatorNew) loads the whole saved VM
// (entities, threads, stacks) from these records; this port keeps FreeSims' "create and place every object" flow and
// overwrites each created object's state with its saved record afterwards. Threads are not restored: every object
// and Sim restarts its main tree, so values that only make sense mid-tree are left as init set them.
void VMWorldActivator::RestoreSavedState(OBJM* objm) {
    LastRestore = RestoreStats();
    auto ctx = VM->Context;
    // object data the engine owns or that belongs to a running tree / the placement just done
    static const int skipData[] = {
        (int)VMStackObjectVariable::Direction, (int)VMStackObjectVariable::ContainerId, (int)VMStackObjectVariable::SlotNumber,
        (int)VMStackObjectVariable::Flags, (int)VMStackObjectVariable::ObjectId, (int)VMStackObjectVariable::TargetId,
        (int)VMStackObjectVariable::ParentId, (int)VMStackObjectVariable::Room, (int)VMStackObjectVariable::FlagField2,
        (int)VMStackObjectVariable::PlacementFlags, (int)VMStackObjectVariable::MovementFlags, (int)VMStackObjectVariable::HideInteraction,
        (int)VMStackObjectVariable::PrimitiveResult, (int)VMStackObjectVariable::PrimitiveResultID, (int)VMStackObjectVariable::UseCount,
        (int)VMStackObjectVariable::WallAdjacencyFlags, (int)VMStackObjectVariable::SlotCount, 52, 54 };
    // person data that belongs to the running tree / routing / posture (Behavior.iff STR#200)
    static const int skipPerson[] = { 0 /*sitting*/, 8 /*current outfit*/, 22, 23 /*hand gestures*/, 25, 27, 28 /*tree use, tick counter*/,
        33 /*priority*/, 37, 38, 39, 40, 41, 42, 43, 44, 45 /*routing, head seek*/, 59 /*object social object id*/, 62 /*route result*/,
        64 /*is swimming*/, 71 /*non-interruptible*/, 74 /*render flags*/, 77, 78 /*autofollow, freeze*/ };
    auto skip = [](const int* list, size_t n, int v) { for (size_t i = 0; i < n; i++) if (list[i] == v) return true; return false; };

    for (auto& kv : objm->ObjectData) {
        auto& mo = kv.second;
        if (!mo.Instance || mo.Instance->OBJT == nullptr) continue;
        auto& inst = *mo.Instance;
        auto ent = VM->GetObjectById((int16_t)kv.first);
        if (ent == nullptr || ent->Dead || ent->Object->OBJ->GUID != inst.OBJT->GUID) { LastRestore.Missing++; continue; }
        auto ava = dynamic_cast<VMAvatar*>(ent);
        auto gobj = dynamic_cast<VMGameObject*>(ent);
        LastRestore.Objects++;
        // Objects that only exist for a running interaction (a conversation's "*Social Interactions" object, the
        // "*comeandsee" call markers, "Destination" walk targets and other OBJD type 11 'Internal' objects) keep their
        // fresh init state: their saved attributes name the Sims and the interaction step they were in, which no
        // longer runs because threads restart. Restored, a saved "Destination" kept Mortimer (house 5) failing the
        // route through a door every tick. Fresh, they clean themselves up.
        if (inst.OBJT->OBJDType == OBJDType::Internal || mo.Name == "*Social Interactions" || mo.Name == "*comeandsee") { LastRestore.Skipped++; continue; }

        for (size_t i = 0; i < inst.Attributes.size(); i++) ent->SetAttribute((int32_t)i, inst.Attributes[i]);
        for (int i = 0; i < (int)inst.ObjectData.size() && i < (int)ent->ObjectData.size(); i++) {
            if (skip(skipData, sizeof(skipData) / sizeof(int), i)) continue;
            if (ava != nullptr && (i == (int)VMStackObjectVariable::Hidden)) continue;
            ent->ObjectData[i] = inst.ObjectData[i];
        }
        if (gobj != nullptr) {
            // Flags (object data 8) are NOT restored: the saved word replaced flags the engine/init trees set for this
            // port's renderer and placement (with it, doors and windows lost their cut-away and drew full height
            // through cut walls - reported by the 3DS app). Init recomputes them; only transient state is lost.
            for (size_t i = 0; i < inst.DynamicSpriteFlags.size() && i < 128; i++) ent->SetDynamicSpriteFlag((uint16_t)i, inst.DynamicSpriteFlags[i] != 0);
            gobj->RefreshGraphic();
        }
        for (auto& rel : inst.Relationships) {
            std::vector<int16_t> values;
            for (auto v : rel.Values) values.push_back((int16_t)v);
            ent->MeToObject[(uint16_t)rel.TargetID] = values;
        }

        if (ava != nullptr && inst.HasPersonData) {
            LastRestore.Persons++;
            auto& person = inst.PersonData;
            for (int i = 0; i < (int)person.PersonData.size() && i <= 100; i++) {
                if (skip(skipPerson, sizeof(skipPerson) / sizeof(int), i)) continue;
                ava->SetPersonData((VMPersonDataVariable)i, person.PersonData[i]);
            }
            for (int i = 0; i < 16; i++) {
                if (i == (int)VMMotive::SleepState) continue; // not asleep: the Sim restarts its main tree
                ava->SetMotiveData((VMMotive)i, (int16_t)std::lround(person.MotiveData[i]));
            }
            if (inst.X != -16 || inst.Y != -16) {
                auto dir = (Direction)(1 << (inst.ObjectData[(int)VMStackObjectVariable::Direction] & 7));
                ava->SetPosition(LotTilePos((int16_t)inst.X, (int16_t)inst.Y, (int8_t)inst.Level), dir, ctx);
                if (ava->Position() == LotTilePos::OUT_OF_WORLD) // saved inside furniture (bed, chair): nearest free tile
                    ava->SetPosition(LotTilePos::FromBigTile((int16_t)(inst.X / 16), (int16_t)(inst.Y / 16), (int8_t)inst.Level), dir, ctx);
                if (ava->Position() == LotTilePos::OUT_OF_WORLD) {
                    // FIX(sims): a Sim saved sitting/lying (contained by a chair, bed ...) stands up next to that
                    // object (VMFindLocationFor via VMTS1FamilyLoader::PlaceNear) instead of at the mailbox.
                    auto near = VM->GetObjectById(inst.ObjectData[(int)VMStackObjectVariable::ContainerId]);
                    if (near != nullptr && near->Position() != LotTilePos::OUT_OF_WORLD) VMTS1FamilyLoader(VM).PlaceNear(ava, near);
                }
                if (ava->Position() != LotTilePos::OUT_OF_WORLD) LastRestore.PersonsPlaced++;
            }
        }
    }
    // containment: put contained objects back into their container's slot
    for (auto& kv : objm->ObjectData) {
        auto& mo = kv.second;
        if (!mo.Instance || mo.ContainerID == 0) continue;
        auto ent = VM->GetObjectById((int16_t)kv.first);
        auto container = VM->GetObjectById((int16_t)mo.ContainerID);
        if (ent == nullptr || container == nullptr || ent->Container == container) continue;
        if (dynamic_cast<VMAvatar*>(ent) != nullptr) continue; // Sims restart standing
        if (container->PlaceInSlot(ent, mo.ContainerSlot, true, ctx)) LastRestore.Contained++;
        else if (getenv("WA_DEBUG")) printf("WA: could not put %s into slot %d of %s\n", ent->ToString().c_str(), mo.ContainerSlot, container->ToString().c_str());
    }
    if (getenv("WA_DEBUG")) printf("WA: restored %d objects (%d persons, %d placed at their saved position, %d interaction objects left fresh), %d contained, %d records without object\n",
        LastRestore.Objects, LastRestore.Persons, LastRestore.PersonsPlaced, LastRestore.Skipped, LastRestore.Contained, LastRestore.Missing);
}

std::vector<bool> VMWorldActivator::ResizeFlags(const std::vector<uint8_t>& flags, int32_t size) {
    if (size >= 64) {
        std::vector<bool> r(flags.size());
        for (size_t i = 0; i < flags.size(); i++) r[i] = (flags[i] & 0x20) == 0;
        return r;
    }
    std::vector<bool> result((size_t)(size * size));
    int iS = 0;
    int iD = 0;
    for (int y = 0; y < 64; y++) {
        if (y >= size) {
            return result;
        }
        for (int x2 = 0; x2 < 64; x2++) {
            if (x2 < size) {
                result[iD++] = (flags[iS] & 0x20) == 0;
            }
            iS++;
        }
    }
    return result;
}

std::vector<FloorTile> VMWorldActivator::ResizeFloors(const std::vector<FloorTile>& floors, int32_t size) {
    if (size >= 64) {
        return floors;
    }
    std::vector<FloorTile> result((size_t)(size * size));
    size_t iS = 0;
    int iD = 0;
    for (int y = 0; y < 64; y++) {
        if (y >= size) {
            return result;
        }
        for (int x = 0; x < 64; x++) {
            if (x < size && iS < floors.size()) {
                result[iD++] = floors[iS];
            }
            iS++;
        }
    }
    return result;
}

std::vector<WallTile> VMWorldActivator::ResizeWalls(const std::vector<WallTile>& walls, int32_t size) {
    if (size >= 64) {
        return walls;
    }
    std::vector<WallTile> result((size_t)(size * size));
    size_t iS = 0;
    int iD = 0;
    for (int y = 0; y < 64; y++) {
        if (y >= size) {
            return result;
        }
        for (int x = 0; x < 64; x++) {
            if (x < size && iS < walls.size()) {
                result[iD++] = walls[iS];
            }
            iS++;
        }
    }
    return result;
}

std::vector<FloorTile> VMWorldActivator::RemapFloors(std::vector<FloorTile> floors, const std::unordered_map<uint8_t, uint16_t>& dict, const std::vector<uint8_t>& flags) {
    for (size_t i = 0; i < floors.size(); i++) {
        FloorTile floor = floors[i];
        if (FlipRoad) {
            if (floor.Pattern == 10) {
                floors[i].Pattern = 11;
            } else if (floor.Pattern == 11) {
                floors[i].Pattern = 10;
            }
        }
        uint8_t flag = (i < flags.size()) ? flags[i] : 0;
        if ((floor.Pattern != 0 && (flag & 0x20) == 0) || floor.Pattern > 30) {
            auto it = dict.find((uint8_t)floor.Pattern);
            if (it != dict.end()) floors[i].Pattern = it->second;
        }
    }
    return floors;
}

std::vector<WallTile> VMWorldActivator::RemapWalls(std::vector<WallTile> walls, const std::unordered_map<uint8_t, uint16_t>& dict, const std::unordered_map<uint8_t, uint16_t>& floorDict) {
    auto tryGet = [](const std::unordered_map<uint8_t, uint16_t>& d, uint16_t key, uint16_t& out) {
        auto it = d.find((uint8_t)key);
        if (it == d.end()) return false;
        out = it->second;
        return true;
    };
    for (size_t i = 0; i < walls.size(); i++) {
        WallTile wall = walls[i];
        uint16_t newID;
        if (wall.BottomLeftPattern != 0 && tryGet(dict, wall.BottomLeftPattern, newID)) {
            walls[i].BottomLeftPattern = newID;
        }
        if (wall.BottomRightPattern != 0 && tryGet(dict, wall.BottomRightPattern, newID)) {
            walls[i].BottomRightPattern = newID;
        }
        if ((wall.Segments & WallSegments::AnyDiag) > 0) {
            if (wall.TopLeftPattern != 0 && tryGet(floorDict, wall.TopLeftPattern, newID)) {
                walls[i].TopLeftPattern = newID;
            }
            if (wall.TopLeftStyle != 0 && tryGet(floorDict, wall.TopLeftStyle, newID)) {
                walls[i].TopLeftStyle = newID;
            }
        } else {
            if (wall.TopLeftPattern != 0 && tryGet(dict, wall.TopLeftPattern, newID)) {
                walls[i].TopLeftPattern = newID;
            }
            if (wall.TopRightPattern != 0 && tryGet(dict, wall.TopRightPattern, newID)) {
                walls[i].TopRightPattern = newID;
            }
        }
    }
    return walls;
}

std::vector<FloorTile> VMWorldActivator::DecodeFloors(const std::vector<uint8_t>& data) {
    std::vector<FloorTile> result(data.size());
    for (size_t i = 0; i < data.size(); i++) result[i].Pattern = data[i];
    return result;
}

std::vector<FloorTile> VMWorldActivator::DecodeAdvFloors(const std::vector<uint8_t>& data) {
    int j = 0;
    std::vector<FloorTile> result(data.size() / 2);
    for (size_t i = 0; i + 1 < data.size(); i += 2) {
        result[j++].Pattern = (uint16_t)(data[i] | (data[i + 1] << 8));
    }
    return result;
}

bool VMWorldActivator::DecodeObjID(const std::vector<uint8_t>* data, std::vector<uint16_t>& result) {
    if (data == nullptr) {
        return false;
    }
    int j = 0;
    result.assign(data->size() / 2, 0);
    for (size_t i = 0; i + 1 < data->size(); i += 2) {
        result[j++] = (uint16_t)((*data)[i] | ((*data)[i + 1] << 8));
    }
    return true;
}

std::vector<WallTile> VMWorldActivator::DecodeWalls(const std::vector<uint8_t>& data) {
    int j = 0;
    std::vector<WallTile> result(data.size() / 8);
    for (size_t i = 0; i + 7 < data.size(); i += 8) {
        WallTile wallTile;
        wallTile.Segments = (WallSegments)data[i];
        wallTile.TopLeftStyle = data[i + 2];
        wallTile.TopRightStyle = data[i + 3];
        wallTile.TopLeftPattern = data[i + 4];
        wallTile.TopRightPattern = data[i + 5];
        wallTile.BottomLeftPattern = data[i + 6];
        wallTile.BottomRightPattern = data[i + 7];
        WallTile tile = wallTile;
        if ((tile.Segments & WallSegments::AnyDiag) == 0) {
            if (!ValidWallStyles.count(tile.TopLeftStyle)) {
                tile.TopLeftStyle = 1;
            }
            if (!ValidWallStyles.count(tile.TopRightStyle)) {
                tile.TopRightStyle = 1;
            }
            if ((tile.Segments & WallSegments::TopLeft) == 0) {
                tile.TopLeftStyle = 0;
            }
            if ((tile.Segments & WallSegments::TopRight) == 0) {
                tile.TopRightStyle = 0;
            }
        }
        result[j++] = tile;
    }
    return result;
}

std::vector<WallTile> VMWorldActivator::DecodeAdvWalls(const std::vector<uint8_t>& data) {
    int j = 0;
    std::vector<WallTile> result(data.size() / 14);
    for (size_t i = 0; i + 13 < data.size(); i += 14) {
        WallTile wallTile;
        wallTile.Segments = (WallSegments)data[i];
        wallTile.TopLeftStyle = (uint16_t)(data[i + 2] | (data[i + 3] << 8));
        wallTile.TopRightStyle = (uint16_t)(data[i + 4] | (data[i + 5] << 8));
        wallTile.TopLeftPattern = (uint16_t)(data[i + 6] | (data[i + 7] << 8));
        wallTile.TopRightPattern = (uint16_t)(data[i + 8] | (data[i + 9] << 8));
        wallTile.BottomLeftPattern = (uint16_t)(data[i + 10] | (data[i + 11] << 8));
        wallTile.BottomRightPattern = (uint16_t)(data[i + 12] | (data[i + 13] << 8));
        WallTile tile = wallTile;
        if ((tile.Segments & WallSegments::AnyDiag) == 0) {
            if (!ValidWallStyles.count(tile.TopLeftStyle)) {
                tile.TopLeftStyle = 1;
            }
            if (!ValidWallStyles.count(tile.TopRightStyle)) {
                tile.TopRightStyle = 1;
            }
            if ((tile.Segments & WallSegments::TopLeft) == 0) {
                tile.TopLeftStyle = 0;
            }
            if ((tile.Segments & WallSegments::TopRight) == 0) {
                tile.TopRightStyle = 0;
            }
        }
        result[j++] = tile;
    }
    return result;
}

static std::string NameBeforeDot(const std::string& name) {
    size_t dot = name.find('.');
    return FSO::Content::ToLowerInvariant((dot == std::string::npos) ? name : name.substr(0, dot));
}

std::unordered_map<uint8_t, uint16_t> VMWorldActivator::BuildFloorDict(const std::vector<WALmEntry>& entries) {
    auto& c = FSO::Content::Content::Get()->WorldFloors->DynamicFloorFromID;
    std::unordered_map<uint8_t, uint16_t> result;
    for (auto& entry : entries) {
        auto it = c.find(NameBeforeDot(entry.Name));
        if (it != c.end()) {
            result[entry.ID] = it->second;
        }
    }
    return result;
}

std::unordered_map<uint8_t, uint16_t> VMWorldActivator::BuildWallDict(const std::vector<WALmEntry>& entries) {
    auto& c = FSO::Content::Content::Get()->WorldWalls->DynamicWallFromID;
    std::unordered_map<uint8_t, uint16_t> result;
    for (auto& entry : entries) {
        auto it = c.find(NameBeforeDot(entry.Name));
        if (it != c.end()) {
            result[entry.ID] = it->second;
        }
    }
    return result;
}

std::unique_ptr<TerrainComponent> VMWorldActivator::CreateTerrain(int16_t Size) {
    std::unique_ptr<TerrainComponent> terrain(new TerrainComponent(st::Rectangle(1, 1, Size - 2, Size - 2)));
    InitWorldComponent(terrain.get());
    return terrain;
}

VMAvatar* VMWorldActivator::CreateAvatar() {
    auto group = VM->Context->CreateObjectInstance(VMAvatar::TEMPLATE_PERSON, LotTilePos::OUT_OF_WORLD, Direction::NORTH, false);
    return (group == nullptr || group->Objects.empty()) ? nullptr : (VMAvatar*)group->Objects[0];
}

VMAvatar* VMWorldActivator::CreateAvatar(uint32_t guid, XmlCharacter* xml, bool visitor, int16_t id) {
    auto group = VM->Context->CreateObjectInstance(guid, LotTilePos::OUT_OF_WORLD, Direction::NORTH, false);
    if (group == nullptr || group->Objects.empty()) return nullptr;
    VMAvatar* avatar = (VMAvatar*)group->Objects[0];
    avatar->Visitor = visitor;
    VMEntity* mailbox = nullptr;
    for (auto x : VM->Entities) {
        auto g = x->Object->OBJ->GUID;
        if (g == 4010940788u || g == 496355760 || g == 1961361321 || g == 2254071826u) { mailbox = x; break; }
    }
    if (mailbox == nullptr) throw std::runtime_error("Sequence contains no matching element"); // C# First()
    avatar->SetAvatarData(xml);
    LotTilePos pos = mailbox->Position();
    pos.x = (int16_t)(mailbox->Position().x + 1);
    pos.y = (int16_t)(mailbox->Position().y + id);
    avatar->SetPosition(pos, Direction::WEST, VM->Context);
    return avatar;
}

void VMWorldActivator::CreateObject(const XmlHouseDataObject& obj) {
    LotTilePos pos = ((obj.Level == 0) ? LotTilePos::OUT_OF_WORLD : LotTilePos::FromBigTile((int16_t)obj.X, (int16_t)obj.Y, (int8_t)obj.Level));
    auto mojb = VM->Context->CreateObjectInstance(obj.GUIDInt(), pos, obj.Direction(), false);
    if (mojb == nullptr || mojb->Objects.empty()) {
        return;
    }
    VMEntity* nobj = mojb->Objects[0];
    if (obj.Group != 0) {
        for (auto sub : nobj->MultitileGroup->Objects) {
            sub->SetValue(VMStackObjectVariable::GroupID, (int16_t)obj.Group);
        }
    }
    for (size_t i = 0; i < nobj->MultitileGroup->Objects.size(); i++) {
        nobj->MultitileGroup->Objects[i]->ExecuteEntryPoint(11, VM->Context, true);
    }
}

void VMWorldActivator::InitWorldComponent(FSO::LotView::Components::WorldComponent* component) {
    // STUB(3ds): component.Initialize(World.State.Device, World.State)
    component->Initialize(nullptr, nullptr);
}
