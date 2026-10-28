/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.VMContext
#include "simantics/VMContext.h"
#include "simantics/VM.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/entities/VMGameObject.h"
#include "simantics/entities/VMMultitileGroup.h"
#include "simantics/engine/VMRoutingFrame.h"
#include "simantics/engine/VMThread.h"
#include "simantics/engine/VMAmbientSound.h"
#include "simantics/engine/VMChangeActionString.h"
#include "simantics/model/VMBuildableAreaInfo.h"
#include "simantics/model/TSOPlatform/VMTSOLotState.h"
#include <random>
#include <algorithm>
#include <cmath>

namespace FSO { namespace SimAntics {
using namespace FSO::LotView::Model;
using namespace FSO::SimAntics::Model;
using namespace FSO::Files::Formats::IFF::Chunks;
using FSO::LotView::Components::ObjectComponent;
using FSO::LotView::Components::AvatarComponent;
using Engine::VMPrimitiveRegistration;

bool VMContext::UseWorld = true;

// Weak default so the core links without the primitives. primitives/VMPrimitiveTable.cpp overrides it.
__attribute__((weak)) void RegisterDefaultPrimitives(VMContext* /*context*/) {}
__attribute__((weak)) void RegisterTS1ExpansionPrimitives(VMContext* /*context*/) {}

void VMContext::Architecture(std::unique_ptr<FSO::SimAntics::VMArchitecture> value) {
    if (_Arch != nullptr) _Arch->WallsChanged = nullptr;
    value->WallsChanged = [this](FSO::SimAntics::VMArchitecture* caller) { WallsChanged(caller); };
    _Arch = std::move(value);
}

VMContext::VMContext(FSO::LotView::World* world) : VMContext(world, nullptr) {}

VMContext::VMContext(FSO::LotView::World* world, VMContext* oldContext) {
    //oldContext is passed in case we need to inherit certain things, like the ambient sound player
    this->World = world;
    this->Clock.reset(new FSO::SimAntics::VMClock());
    this->SetToNextCache.reset(new VMSetToNextCache(this));
    this->ObjectQueries.reset(new VMObjectQueries(this));

    if (oldContext == nullptr) {
        this->Ambience = std::make_shared<Engine::VMAmbientSound>();
    } else {
        this->Ambience = oldContext->Ambience;
    }

    std::random_device rd;
    RandomSeed = ((uint64_t)rd() << 32) ^ (uint64_t)rd(); //when resuming state, this should be set.
    if (RandomSeed == 0) RandomSeed = 0x2545F4914F6CDD1DULL;
    if (DebugSeed != 0) RandomSeed = DebugSeed++ * 2654435761ULL + 1; // FIX(sims) QA: repeatable host runs
    Clock->TicksPerMinute = 30; //1 minute per irl second

    // Primitive table (C# registers each primitive inline here; see README.md for the full opcode list).
    RegisterDefaultPrimitives(this);
    RegisterTS1ExpansionPrimitives(this); // TS1(sims): primitives/VMTS1Primitives.cpp (25 Alter Budget, 51 Manage Inventory, ...)
    if (Primitives[50] == nullptr) {
        auto reg = new VMPrimitiveRegistration(new Primitives::VMChangeActionString());
        reg->Opcode = 50;
        reg->Name = "change_action_string";
        reg->OperandModel = VMPrimitiveRegistration::Model<Primitives::VMChangeActionStringOperand>();
        AddPrimitive(reg);
    }
}

VMContext::~VMContext() {}

/// Returns a random number between 0 and less than the specified maximum.
uint64_t VMContext::DebugSeed = 0; // FIX(sims) QA

uint64_t VMContext::NextRandom(uint64_t max) {
    if (max == 0) return 0;
    RandomSeed ^= RandomSeed >> 12;
    RandomSeed ^= RandomSeed << 25;
    RandomSeed ^= RandomSeed >> 27;
    return (RandomSeed * (uint64_t)(2685821657736338717ULL)) % max;
}

void VMContext::WallsChanged(FSO::SimAntics::VMArchitecture* /*caller*/) {
    RegeneratePortalInfo();

    //TODO: this could get very slow! find a way to make this quicker.
    for (auto obj : VM->Entities) {
        if (dynamic_cast<VMAvatar*>(obj) != nullptr && obj->Thread != nullptr) {
            for (auto& frame : obj->Thread->Stack) {
                if (frame->IsRoutingFrame()) {
                    ((Engine::VMRoutingFrame*)frame.get())->InvalidateRoomRoute();
                }
            }
        }
    }
}

ObjectComponent* VMContext::MakeObjectComponent(FSO::Content::GameObject* obj) {
    // if (UseWorld) return World.MakeObjectComponent(obj);
    return new ObjectComponent(obj);
}

bool VMContext::IsUserOutOfBounds(const LotTilePos& pos) {
    // TS1(sims): map_edit cheat: objects may go anywhere on the lot (street, sidewalk)
    if (_Arch->MapEdit) return pos.TileX() < 0 || pos.TileY() < 0 || pos.TileX() >= _Arch->Width || pos.TileY() >= _Arch->Height || pos.Level < 1 || pos.Level > _Arch->BuildableFloors;
    auto& fine = Architecture()->FineBuildableArea;
    if (!fine.empty()) {
        if (pos.TileX() < 0 || pos.TileX() >= _Arch->Width) return false;
        else if (pos.TileY() < 0 || pos.TileY() >= _Arch->Height) return false;
        else if (pos.Level < 1 || pos.Level > _Arch->BuildableFloors) return false;
        else {
            return !fine[pos.TileX() + pos.TileY() * _Arch->Width];
        }
    } else {
        auto area = Architecture()->BuildableArea;
        return (pos.TileX() < area.X || pos.TileY() < area.Y || pos.Level < 1 || pos.TileX() >= area.Right() || pos.TileY() >= area.Bottom() || pos.Level > _Arch->BuildableFloors);
    }
}

void VMContext::UpdateTSOBuildableArea() {
    //if (VM.TS1) return;
    VMBuildableAreaInfo::UpdateOverbudgetObjects(VM);
    auto lotSInfo = VM->TSOState()->Size;
    auto area = GetTSOBuildableArea(lotSInfo);
    Architecture()->UpdateBuildableArea(area, ((lotSInfo >> 8) & 255) + 2);
}

st::Rectangle VMContext::GetTSOBuildableArea(int32_t lotSInfo) {
    if (Architecture() == nullptr) return st::Rectangle();
    //note: sync on this DOES matter as the OOB check performs on some primitives, and objects are double checked before placement.

    auto lotSize = lotSInfo & 255;
    auto lotDir = (lotSInfo >> 16);

    auto dim = VMBuildableAreaInfo::BuildableSizes[std::min<int32_t>(10, lotSize)];

    //need to rotate the lot dir towards the road. bit weird cos we're rotating a rectangle

    float w = (float)Architecture()->Width;
    float h = (float)Architecture()->Height;
    st::Vector2 corners[4] = {
        st::Vector2(6, 6), // top, default orientation
        st::Vector2(w - 7, 6), // right
        st::Vector2(w - 7, h - 7), // bottom
        st::Vector2(6, h - 7) // left
    };
    st::Vector2 perpIncrease[4] = {
        st::Vector2(0, -1), //bottom left road side
        st::Vector2(-1, 0),
        st::Vector2(0, -1),
        st::Vector2(-1, 0)
    };

    //rotation 0: move perp from closer point to top bottom -> left (90 degree ccw of perp)
    //rotation 1: choose closer pt to top left->top (90 degree ccw of perp)
    //rotation 2: choose closer pt to top top->right (90 degree cw of perp)

    auto pt1 = corners[(lotDir + 2) % 4];
    auto pt2 = corners[(lotDir + 3) % 4];

    auto ctr = (pt1 + pt2) / 2;
    auto lotBase = ctr + perpIncrease[((3 - lotDir) % 4 + 4) % 4] * (float)dim / 2;
    if (lotDir == 0 || lotDir == 3) lotBase += perpIncrease[lotDir] * (float)(dim);

    return st::Rectangle((int)lotBase.X, (int)lotBase.Y, dim, dim);
}

void VMContext::RegeneratePortalInfo() {
    RoomInfo.assign(Architecture()->RoomData.size(), VMRoomInfo());
    for (size_t i = 0; i < RoomInfo.size(); i++) {
        RoomInfo[i].Entities.clear();
        RoomInfo[i].Portals.clear();
        RoomInfo[i].Room = Architecture()->RoomData[i];
        // TS1(sims): FreeSO keys a room's lighting and room score by its "light base room" (the first of a group
        // of rooms joined by open divisions). This port does not group rooms, so every room is its own base. FreeSims
        // left it 0, so GetRoomScore always returned room 0's score (0): the Room motive was always 0.
        RoomInfo[i].Room.LightBaseRoom = (uint16_t)i;
        RoomInfo[i].Light = RoomLighting();
    }

    for (auto obj : VM->Entities) {
        auto room = GetObjectRoom(obj);
        VM::AddToObjList(RoomInfo[room].Entities, obj);
        if (obj->EntryPoints[15].ActionFunction != 0) { //portal object
            AddRoomPortal(obj, room);
        }
        obj->SetRoom(room);
    }

    for (uint16_t i = 0; i < RoomInfo.size(); i++) {
        RefreshLighting(i, i == (RoomInfo.size() - 1));
    }
    if (VM::UseWorld() && World != nullptr) World->InvalidateZoom();
}

void VMContext::RefreshLighting(uint16_t room, bool commit) {
    if (RoomInfo.empty() || room >= RoomInfo.size()) return;
    auto& info = RoomInfo[room];
    info.Light.AmbientLight = 0;
    info.Light.OutsideLight = 0;
    if (info.Room.IsOutside) {
        info.Light.OutsideLight = 100;
    } else {
        float areaScale = std::max(1.0f, info.Room.Area / 100.0f);
        for (auto ent : info.Entities) {
            auto flags2 = (VMEntityFlags2)ent->GetValue(VMStackObjectVariable::FlagField2);
            auto cont = ent->GetValue(VMStackObjectVariable::LightingContribution);
            if (cont > 0) {
                if ((flags2 & (VMEntityFlags2::ArchitectualWindow | VMEntityFlags2::ArchitectualDoor)) > 0)
                    info.Light.OutsideLight += (uint16_t)std::min(100.0f, cont / areaScale);
                else
                    info.Light.AmbientLight += (uint16_t)std::min(100.0f, cont / areaScale);
            }
        }
        if (info.Light.OutsideLight > 100) info.Light.OutsideLight = 100;
    }

    RefreshRoomScore(room); // TS1(sims): FreeSO computes the room score with the lighting

    if (commit && UseWorld && Blueprint != nullptr) {
        Blueprint->Light.resize(RoomInfo.size());
        for (size_t i = 0; i < RoomInfo.size(); i++) {
            Blueprint->Light[i] = RoomInfo[i].Light;
        }
        Blueprint->Damage.push_back(BlueprintDamage(BlueprintDamageType::LIGHTING_CHANGED));
    }
}

// TS1(sims): FreeSO VMContext.RefreshRoomScore — the Room motive. Sum of the objects' Room Impact (object data 7:
// decorations positive, dirty dishes/trash/puddles negative), scaled by area (1 per 12 tiles, outside 1/30), minus 10
// inside or 15 outside, clamped to -100..100. Rooms are not merged across open divisions here (see LightBaseRoom).
void VMContext::RefreshRoomScore(uint16_t room) {
    if (RoomInfo.empty() || room == 0 || room >= RoomInfo.size()) return;
    room = RoomInfo[room].Room.LightBaseRoom;
    if (room >= RoomInfo.size()) return;
    auto& info = RoomInfo[room];
    int32_t roomScore = 0;
    for (auto ent : info.Entities) {
        auto roomImpact = ent->GetValue(VMStackObjectVariable::RoomImpact);
        if (roomImpact != 0) roomScore += roomImpact;
    }
    float areaRScale = std::max(1.0f, info.Room.Area / 12.0f);
    if (info.Room.IsOutside) areaRScale = 30;
    roomScore = (int16_t)(roomScore / areaRScale);
    roomScore -= (info.Room.IsOutside) ? 15 : 10;
    info.Light.RoomScore = (int16_t)std::min<int32_t>(100, std::max<int32_t>(-100, roomScore));
}

void VMContext::AddRoomPortal(VMEntity* obj, uint16_t room) {
    //find other portal part, must be in other room to count...
    for (auto obj2 : obj->MultitileGroup->Objects) {
        auto room2 = GetObjectRoom(obj2);
        if (obj != obj2 && room2 != room && obj2->EntryPoints[15].ActionFunction != 0) {
            RoomInfo[room].Portals.push_back(VMRoomPortal(obj->ObjectID, room2));
            break;
        }
    }
}

void VMContext::RemoveRoomPortal(VMEntity* obj, uint16_t room) {
    auto& portals = RoomInfo[room].Portals;
    for (size_t i = 0; i < portals.size(); i++) {
        if (portals[i].ObjectID == obj->ObjectID) {
            portals.erase(portals.begin() + i);
            break;
        }
    }
}

void VMContext::RegisterObjectPos(VMEntity* obj) {
    auto pos = obj->Position();
    if (pos.Level < 1) return;

    //add object to room

    auto room = GetObjectRoom(obj);
    if (room < RoomInfo.size()) {
        VM::AddToObjList(RoomInfo[room].Entities, obj);
        if (obj->EntryPoints[15].ActionFunction != 0) { //portal
            AddRoomPortal(obj, room);
        }
    }
    obj->SetRoom(room);
    if (obj->GetValue(VMStackObjectVariable::LightingContribution) > 0)
        RefreshLighting(room, true);
    else if (obj->GetValue(VMStackObjectVariable::RoomImpact) != 0)
        RefreshRoomScore(room); // TS1(sims): FreeSO

    SetToNextCache->RegisterObjectPos(obj);
}

void VMContext::UnregisterObjectPos(VMEntity* obj) {
    //remove object from room

    auto room = GetObjectRoom(obj);
    if (room < RoomInfo.size()) {
        auto& ents = RoomInfo[room].Entities;
        auto it = std::find(ents.begin(), ents.end(), obj);
        if (it != ents.end()) ents.erase(it);
        if (obj->EntryPoints[15].ActionFunction != 0) { //portal
            RemoveRoomPortal(obj, room);
        }
    }
    if (obj->GetValue(VMStackObjectVariable::LightingContribution) > 0)
        RefreshLighting(room, true);
    else if (obj->GetValue(VMStackObjectVariable::RoomImpact) != 0)
        RefreshRoomScore(room); // TS1(sims): FreeSO

    SetToNextCache->UnregisterObjectPos(obj);
}

// TS1(sims): FreeSO VMContext.SlopeVertexCheck / CheckSlopeValid — a terrain vertex may only move when no wall,
// object (that needs level ground) or floor tile (TS1: floors are always level) touches it.
bool VMContext::SlopeVertexCheck(int32_t x, int32_t y) {
    for (int8_t i = 1; i <= 5; i++) {
        auto pos = LotTilePos::FromBigTile((int16_t)x, (int16_t)y, i);
        if (!CheckSlopeValid(pos)) return false;
        pos.x -= 16;
        if (!CheckSlopeValid(pos)) return false;
        pos.y -= 16;
        if (!CheckSlopeValid(pos)) return false;
        pos.x += 16;
        if (!CheckSlopeValid(pos)) return false;
    }
    return true;
}

bool VMContext::CheckSlopeValid(const LotTilePos& pos) {
    if (pos.x < 0 || pos.y < 0) return true;
    if (pos.TileX() >= _Arch->Width || pos.TileY() >= _Arch->Height || pos.Level > _Arch->Stories) return true;
    auto objs = SetToNextCache->GetObjectsAt(pos);
    if (objs != nullptr) {
        for (auto obj : *objs) {
            if (obj->SlopeValid() != VMPlacementError::Success) return false;
        }
    }
    if (_Arch->GetWall(pos.TileX(), pos.TileY(), pos.Level).Segments != 0) return false;
    if (_Arch->GetFloor(pos.TileX(), pos.TileY(), pos.Level).Pattern != 0) return false; // TS1(sims)
    return true;
}

bool VMContext::CheckWallValid(const LotTilePos& pos, const WallTile& wall) {
    // TS1(sims): FreeSO — no walls on sloped terrain
    if ((int)wall.Segments > 0 && pos.Level == 1 && _Arch->GetTerrainSloped(pos.TileX(), pos.TileY())) return false;
    auto objs = SetToNextCache->GetObjectsAt(pos);
    if (objs == nullptr) return true;
    for (auto obj : *objs) {
        if (obj->WallChangeValid(wall, obj->Direction(), false) != VMPlacementError::Success) return false;
    }
    return true;
}

bool VMContext::CheckFloorValid(const LotTilePos& pos, const FloorTile& floor) {
    // TS1(sims): floors and pools need level terrain
    if (floor.Pattern != 0 && pos.Level == 1 && _Arch->GetTerrainSloped(pos.TileX(), pos.TileY())) return false;
    auto objs = SetToNextCache->GetObjectsAt(pos);
    if (objs == nullptr) return true;
    for (auto obj : *objs) {
        if (obj->FloorChangeValid(floor, pos.Level) != VMPlacementError::Success) return false;
    }
    return true;
}

VMSolidResult VMContext::SolidToAvatars(const LotTilePos& pos) {
    if (IsOutOfBounds(pos) || (pos.Level < 1) ||
        (pos.Level != 1 && Architecture()->GetFloor(pos.TileX(), pos.TileY(), pos.Level).Pattern == 0)) { VMSolidResult r; r.Solid = true; return r; }
    auto objs = SetToNextCache->GetObjectsAt(pos);
    if (objs == nullptr) return VMSolidResult();
    for (auto obj : *objs) {
        if (obj == nullptr) continue;
        auto flags = (VMEntityFlags)obj->GetValue(VMStackObjectVariable::Flags);
        if (((flags & VMEntityFlags::DisallowPersonIntersection) > 0) || (flags & (VMEntityFlags::AllowPersonIntersection | VMEntityFlags::HasZeroExtent)) == 0) {
            VMSolidResult r;
            r.Solid = true;
            r.Chair = (obj->EntryPoints[26].ActionFunction != 0) ? obj : nullptr;
            return r; //solid to people
        }
    }
    return VMSolidResult();
}

bool VMContext::IsOutOfBounds(const LotTilePos& pos) {
    return (pos.x < 0 || pos.y < 0 || pos.Level < 1 || pos.TileX() >= _Arch->Width || pos.TileY() >= _Arch->Height || pos.Level > _Arch->Stories);
}

VMPlacementResult VMContext::GetAvatarPlace(VMEntity* target, const LotTilePos& pos, Direction dir) {
    //avatars cannot be placed in slots under any circumstances, so we skip a few steps.

    auto footprint = target->GetObstacle(pos, dir);
    uint16_t room = GetRoomAt(pos);

    VMPlacementError status = VMPlacementError::Success;
    VMEntity* statusObj = nullptr;

    if (footprint == nullptr || pos.Level < 1 || room >= RoomInfo.size()) {
        return VMPlacementResult(status);
    }

    auto objs = RoomInfo[room].Entities; //copy: entry points may change room membership
    for (auto obj : objs) {
        if (obj->MultitileGroup == target->MultitileGroup) continue;
        auto oFoot = obj->Footprint.get();

        std::vector<int16_t> a1 = { obj->ObjectID, 0, 0, 0 };
        std::vector<int16_t> a2 = { target->ObjectID, 0, 0, 0 };
        if (oFoot != nullptr && oFoot->Intersects(*footprint)
            && (!(target->ExecuteEntryPoint(5, this, true, obj, &a1)
                || obj->ExecuteEntryPoint(5, this, true, target, &a2)))) {
            auto flags = (VMEntityFlags)obj->GetValue(VMStackObjectVariable::Flags);
            bool allowAvatars = ((flags & VMEntityFlags::DisallowPersonIntersection) == 0) && ((flags & VMEntityFlags::AllowPersonIntersection) > 0);
            if (!allowAvatars) {
                status = VMPlacementError::CantIntersectOtherObjects;
                statusObj = obj;
                if (obj->EntryPoints[26].ActionFunction != 0) break; //select chairs immediately.
            }
        }
    }
    return VMPlacementResult(status, statusObj);
}

VMPlacementResult VMContext::GetObjPlace(VMEntity* target, const LotTilePos& pos, Direction dir) {
    //ok, this might be confusing...
    int16_t allowedHeights = target->GetValue(VMStackObjectVariable::AllowedHeightFlags);
    int16_t weight = target->GetValue(VMStackObjectVariable::Weight);
    bool noFloor = (allowedHeights & 1) == 0;

    auto flags = (VMEntityFlags)target->GetValue(VMStackObjectVariable::Flags);
    bool allowAvatars = ((flags & VMEntityFlags::DisallowPersonIntersection) == 0) && ((flags & VMEntityFlags::AllowPersonIntersection) > 0);

    auto footprint = target->GetObstacle(pos, dir);
    uint16_t room = GetRoomAt(pos);

    VMPlacementError status = (noFloor) ? VMPlacementError::HeightNotAllowed : VMPlacementError::Success;
    VMEntity* statusObj = nullptr;

    if (footprint == nullptr || pos.Level < 1 || room >= RoomInfo.size()) {
        return VMPlacementResult(status);
    }

    auto objs = RoomInfo[room].Entities; //copy: entry points may change room membership
    for (auto obj : objs) {
        if (obj->MultitileGroup == target->MultitileGroup || (dynamic_cast<VMAvatar*>(obj) != nullptr && allowAvatars)
            || (target->GhostImage && target->GhostOriginal != nullptr &&
                std::find(target->GhostOriginal->Objects.begin(), target->GhostOriginal->Objects.end(), obj) != target->GhostOriginal->Objects.end())) continue;
        auto oFoot = obj->Footprint.get();

        std::vector<int16_t> a1 = { obj->ObjectID, 0, 0, 0 };
        std::vector<int16_t> a2 = { target->ObjectID, 0, 0, 0 };
        if (oFoot != nullptr && oFoot->Intersects(*footprint)
            && (!(target->ExecuteEntryPoint(5, this, true, obj, &a1)
                || obj->ExecuteEntryPoint(5, this, true, target, &a2)))) {
            statusObj = obj;
            status = VMPlacementError::CantIntersectOtherObjects;

            //this object is technically solid. Check if we can place on top of it
            if (allowedHeights > 1 && obj->TotalSlots() > 0 && (obj->GetSlot(0) == nullptr || obj->GetSlot(0) == target)) {
                //first check if we have a slot 0, which is what we place onto. then check if it's empty,
                //then check if the object can support this one's weight.
                //we also need to make sure that the height of this specific slot is allowed.

                if (((1 << (obj->GetSlotHeight(0) - 1)) & allowedHeights) > 0) {
                    if (weight < obj->GetValue(VMStackObjectVariable::SupportStrength)) {
                        return VMPlacementResult(VMPlacementError::Success, obj);
                    } else {
                        status = VMPlacementError::CantSupportWeight;
                    }
                } else {
                    if (noFloor) {
                        if ((allowedHeights & (1 << 3)) > 0) status = VMPlacementError::CounterHeight;
                        else status = (obj->GetSlotHeight(0) == 8) ? VMPlacementError::CannotPlaceComputerOnEndTable : VMPlacementError::HeightNotAllowed;
                    }
                }
            }
        }
    }
    return VMPlacementResult(status, statusObj);
}

uint16_t VMContext::GetObjectRoom(VMEntity* obj) {
    if (obj->Position() == LotTilePos::OUT_OF_WORLD) return 0;
    if (obj->Position().Level < 1 || obj->Position().Level > _Arch->Stories) return 0;
    auto& map = Architecture()->Rooms[obj->Position().Level - 1].Map;
    size_t off = (size_t)(obj->Position().TileX() + obj->Position().TileY() * _Arch->Width);
    if (off >= map.size()) return 0;
    return (uint16_t)map[off];
}

uint16_t VMContext::GetRoomAt(const LotTilePos& pos) {
    if (pos.TileX() < 0 || pos.TileX() >= _Arch->Width) return 0;
    else if (pos.TileY() < 0 || pos.TileY() >= _Arch->Height) return 0;
    else if (pos.Level < 1 || pos.Level > _Arch->Stories) return 0;
    else {
        auto& map = Architecture()->Rooms[pos.Level - 1].Map;
        size_t off = (size_t)(pos.TileX() + pos.TileY() * _Arch->Width);
        if (off >= map.size()) return 0;
        return (uint16_t)map[off];
    }
}

int16_t VMContext::GetRoomScore(uint16_t room) {
    if (room >= RoomInfo.size()) return 0;
    auto base = RoomInfo[room].Room.LightBaseRoom;
    if (base >= RoomInfo.size()) return 0;
    return RoomInfo[base].Light.RoomScore;
}

Entities::VMMultitileGroup* VMContext::GhostCopyGroup(Entities::VMMultitileGroup* group) {
    auto newGroup = CreateObjectInstance(((group->MultiTile) ? group->BaseObject()->MasterDefinition->GUID : group->BaseObject()->Object->OBJ->GUID), LotTilePos::OUT_OF_WORLD, group->BaseObject()->Direction(), true, false);

    if (newGroup != nullptr) {
        newGroup->Price = group->Price;
        for (size_t i = 0; i < std::min(newGroup->Objects.size(), group->Objects.size()); i++) {
            newGroup->Objects[i]->GhostOriginal = group;
            newGroup->Objects[i]->SetValue(VMStackObjectVariable::Graphic, group->Objects[i]->GetValue(VMStackObjectVariable::Graphic));
            newGroup->Objects[i]->DynamicSpriteFlags = group->Objects[i]->DynamicSpriteFlags;
            newGroup->Objects[i]->DynamicSpriteFlags2 = group->Objects[i]->DynamicSpriteFlags2;
            newGroup->Objects[i]->SetDynamicSpriteFlag(0, group->Objects[i]->IsDynamicSpriteFlagSet(0));
            newGroup->Objects[i]->PlatformState = group->Objects[i]->PlatformState;
            auto go = dynamic_cast<VMGameObject*>(newGroup->Objects[i]);
            if (go != nullptr) go->RefreshGraphic();
        }
    }

    return newGroup;
}

Entities::VMMultitileGroup* VMContext::CreateObjectInstance(uint32_t GUID, const LotTilePos& pos, Direction direction, bool ghostImage, bool ts1) {
    return CreateObjectInstance(GUID, pos, direction, 0, 0, ghostImage, ts1);
}

Entities::VMMultitileGroup* VMContext::CreateObjectInstance(uint32_t GUID, const LotTilePos& pos, Direction direction, bool ts1) {
    return CreateObjectInstance(GUID, pos, direction, 0, 0, false, ts1);
}

Entities::VMMultitileGroup* VMContext::CreateObjectInstance(uint32_t GUID, const LotTilePos& pos, Direction direction, int16_t MainStackOBJ, int16_t MainParam, bool ghostImage, bool ts1) {
    auto objDefinition = FSO::Content::Content::Get()->WorldObjects->Get(GUID, ts1);
    if (objDefinition == nullptr) {
        return nullptr;
    }
    auto group = VM->OwnGroup(new Entities::VMMultitileGroup());

    auto master = objDefinition->OBJ->MasterID;
    if (master != 0 && objDefinition->OBJ->SubIndex == -1) {
        group->MultiTile = true;
        auto objd = objDefinition->Resource->List<OBJD>();

        for (size_t i = 0; i < objd.size(); i++) {
            if (objd[i]->MasterID == master && objd[i]->SubIndex != -1) //if sub-part of this object, make it!
            {
                auto subObjDefinition = FSO::Content::Content::Get()->WorldObjects->Get(objd[i]->GUID, ts1);
                if (subObjDefinition != nullptr) {
                    auto worldObject = new ObjectComponent(subObjDefinition);
                    auto vmObject = (VMGameObject*)VM->OwnEntity(new VMGameObject(subObjDefinition, worldObject));
                    vmObject->GhostImage = ghostImage;
                    if (UseWorld && Blueprint != nullptr) Blueprint->AddObject(worldObject);

                    vmObject->MasterDefinition = objDefinition->OBJ;
                    vmObject->UseTreeTableOf(objDefinition, ts1);

                    vmObject->MainParam = MainParam;
                    vmObject->MainStackOBJ = MainStackOBJ;
                    group->AddObject(vmObject);

                    vmObject->MultitileGroup = group;
                    if (!ghostImage) VM->AddEntity(vmObject);
                }
            }
        }

        group->Init(this);
        VMPlacementError couldPlace = group->ChangePosition(pos, direction, this).Status;
        (void)couldPlace;
        return group;
    } else {
        if (objDefinition->OBJ->ObjectType == OBJDType::Person) //person
        {
            auto vmObject = (VMAvatar*)VM->OwnEntity(new VMAvatar(objDefinition));
            vmObject->MultitileGroup = group;
            group->AddObject(vmObject);

            vmObject->GhostImage = ghostImage;
            if (!ghostImage) VM->AddEntity(vmObject);

            if (UseWorld && Blueprint != nullptr && vmObject->WorldUI) Blueprint->AddAvatar((AvatarComponent*)vmObject->WorldUI.get());

            vmObject->MainParam = MainParam;
            vmObject->MainStackOBJ = MainStackOBJ;

            group->Init(this);
            vmObject->SetPosition(pos, direction, this);

            return group;
        } else {
            auto worldObject = new ObjectComponent(objDefinition);
            auto vmObject = (VMGameObject*)VM->OwnEntity(new VMGameObject(objDefinition, worldObject));

            vmObject->MultitileGroup = group;

            group->AddObject(vmObject);

            vmObject->GhostImage = ghostImage;
            if (!ghostImage) VM->AddEntity(vmObject);
            if (UseWorld && Blueprint != nullptr) Blueprint->AddObject(worldObject);

            vmObject->MainParam = MainParam;
            vmObject->MainStackOBJ = MainStackOBJ;

            group->Init(this);
            vmObject->SetPosition(pos, direction, this);

            return group;
        }
    }
}

void VMContext::RemoveObjectInstance(VMEntity* target) {
    target->PrePositionChange(this);
    if (!target->GhostImage) {
        VM->RemoveEntity(target);
    }
    if (UseWorld && Blueprint != nullptr && target->WorldUI) {
        if (dynamic_cast<VMGameObject*>(target) != nullptr) Blueprint->RemoveObject((ObjectComponent*)target->WorldUI.get());
        else Blueprint->RemoveAvatar((AvatarComponent*)target->WorldUI.get());
    }
    // STUB(3ds): VM.EODHost.ForceDisconnectObj(target) — no EOD host.
}

void VMContext::AddPrimitive(std::unique_ptr<VMPrimitiveRegistration> primitive) {
    auto op = primitive->Opcode;
    if (op > 255) return;
    Primitives[op] = std::move(primitive);
}

}}
