/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.VMGameObject (entities/VMGameObject.cs)
#include "simantics/entities/VMGameObject.h"
#include "simantics/entities/VMMultitileGroup.h"
#include "simantics/model/TSOPlatform/VMTSOObjectState.h"
#include "simantics/model/TSOPlatform/VMTSOLotState.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"
#include "simantics/VMArchitecture.h"
#include "common/utils/DirectionUtils.h"
#include <cmath>
#include <cstdio>
namespace FSO { namespace SimAntics {
using namespace FSO::Files::Formats::IFF::Chunks;
using namespace FSO::SimAntics::Model;
using namespace FSO::LotView::Model;
using FSO::LotView::Components::ObjectComponent;
using FSO::Common::Utils::DirectionUtils;

VMGameObject::VMGameObject(FSO::Content::GameObject* def, ObjectComponent* worldUI) : VMEntity(def) {
    this->WorldUI.reset(worldUI);
    PlatformState = std::make_shared<TSOPlatform::VMTSOObjectState>(); //todo: ts1 switch
}

void VMGameObject::SetDynamicSpriteFlag(uint16_t index, bool set) {
    VMEntity::SetDynamicSpriteFlag(index, set);
    if (this->WorldUI != nullptr) {
        ObjUI()->DynamicSpriteFlags = this->DynamicSpriteFlags;
        ObjUI()->DynamicSpriteFlags2 = this->DynamicSpriteFlags2;
    }
}

bool VMGameObject::SetValue(VMStackObjectVariable var, int16_t value) {
    switch (var) {
        case VMStackObjectVariable::Flags: {
            auto flags = (VMEntityFlags)value;
            if (UseWorld && WorldUI)
                ObjUI()->HideForCutaway = (flags & VMEntityFlags::HideForCutaway) > 0;
            //            || ((VMEntityFlags2)GetValue(VMStackObjectVariable.FlagField2) & VMEntityFlags2.ArchitectualDoor) > 0;
            break;
        }
        case VMStackObjectVariable::FlagField2:
            /*
            var flags2 = (VMEntityFlags2)value;
            if (UseWorld)
                ((ObjectComponent)WorldUI).HideForCutaway = (flags2 & VMEntityFlags2.ArchitectualDoor) > 0
                    || ((VMEntityFlags)GetValue(VMStackObjectVariable.Flags) & VMEntityFlags.HideForCutaway) > 0; */
            break;
        default: break;
    }
    return VMEntity::SetValue(var, value);
}

int16_t VMGameObject::GetValue(VMStackObjectVariable var) {
    return VMEntity::GetValue(var);
}

bool VMGameObject::RefreshGraphic() {
    if (!UseWorld) return true;
    auto newGraphic = Object->OBJ->BaseGraphicID + ObjectData[(int)VMStackObjectVariable::Graphic];
    auto dgrp = Object->Resource->Get<DGRP>((uint16_t)newGraphic);
    if (dgrp != nullptr) {
        ObjUI()->DGRP = dgrp;
        return true;
    }
    return false;
}

void VMGameObject::SetRoom(uint16_t room) {
    VMEntity::SetRoom(room);
    // NOTE: if something computationally intensive happens here, take into account
    // that the previous value of room may equal the new value if the client resyncs
    // so checking for a difference and only acting when there is is likely a bad idea.
    RefreshLight();
}

void VMGameObject::RefreshLight() {
    if (UseWorld && WorldUI) {
        if (Disabled >= VMGameObjectDisableFlags::LotCategoryWrong) WorldUI->Room = 65533; //grayscale
        else {
            auto flags = (VMEntityFlags2)GetValue(VMStackObjectVariable::FlagField2);
            WorldUI->Room = ((flags & VMEntityFlags2::GeneratesLight) > 0 &&
                GetValue(VMStackObjectVariable::LightingContribution) > 0 &&
                (flags & (VMEntityFlags2::ArchitectualWindow | VMEntityFlags2::ArchitectualDoor)) == 0)
                ? (uint16_t)65535 : (uint16_t)GetValue(VMStackObjectVariable::Room);
        }
    }
}

void VMGameObject::Init(VMContext* context) {
    if (UseWorld && WorldUI) WorldUI->ObjectID = ObjectID;
    PersistID = (uint32_t)ObjectID; //temporary til theres a system to manage these
    if (Slots != nullptr && Slots->Slots.find(0) != Slots->Slots.end()) {
        auto& s0 = Slots->Slots[0];
        Contained.assign(s0.size(), nullptr);
        if (UseWorld && WorldUI) {
            ObjUI()->ContainerSlots.clear();
            for (auto& item : s0) ObjUI()->ContainerSlots.push_back(&item);
        }
    }
    VMEntity::Init(context);
}

float VMGameObject::RadianDirection() const {
    double dir = std::log2((double)(int)Direction()) * (M_PI / 4);
    if (dir > M_PI) dir -= 2 * M_PI;
    return (float)dir;
}

void VMGameObject::RadianDirection(float value) {
    // NOTE: faithful to the C# (which divides by 8 rather than PI/4).
    Direction((DirectionT)(1 << (int)((int)std::nearbyint(DirectionUtils::PosMod(value, (float)M_PI * 2) / 8) % 8)));
}

void VMGameObject::Direction(DirectionT value) {
    _Direction = value;
    if (UseWorld && WorldUI) WorldUI->Direction = value;
}

bool VMGameObject::PartOfPortal() {
    for (auto obj : MultitileGroup->Objects) {
        if (obj->Portal) return true;
    }
    return false;
}

st::Vector3 VMGameObject::VisualPosition() const {
    return (UseWorld && WorldUI) ? (WorldUI->Position + st::Vector3(0.5f, 0.5f, 0.0f)) : st::Vector3();
}
void VMGameObject::VisualPosition(const st::Vector3& value) {
    if (UseWorld && WorldUI) WorldUI->Position = value - st::Vector3(0.5f, 0.5f, 0.0f);
}

std::string VMGameObject::ToString() {
    if (MultitileGroup != nullptr && MultitileGroup->Name != "") return MultitileGroup->Name;
    auto strings = Object->Resource->Get<CTSS>(Object->OBJ->CatalogStringsID);
    if (strings != nullptr) {
        return strings->GetString(0);
    }
    auto label = Object->OBJ->ChunkLabel;
    if (label.size() > 0) {
        return label;
    }
    char buf[16];
    snprintf(buf, sizeof(buf), "%X", (unsigned)Object->OBJ->GUID);
    return buf;
}

void VMGameObject::DisableIfTSOCategoryWrong(VMContext* context) {
    //if (context.VM.TS1) return;
    OBJD* obj = Object->OBJ;
    if (MasterDefinition != nullptr) obj = MasterDefinition;
    auto category = context->VM->TSOState()->PropertyCategory;
    auto flag = (1 << category);
    if (category == 7) flag |= 2; //money objects are allowed on welcome lots too. (fso change, disabling this is todo)
    if (category != 255 && obj->LotCategories > 0 && (obj->LotCategories & flag) == 0)
        Disabled |= VMGameObjectDisableFlags::LotCategoryWrong;
    else
        Disabled &= ~VMGameObjectDisableFlags::LotCategoryWrong;
}

// Begin Container SLOTs interface

int32_t VMGameObject::TotalSlots() {
    return (int32_t)Contained.size();
}

bool VMGameObject::PlaceInSlot(VMEntity* obj, int32_t slot, bool cleanOld, VMContext* context) {
    if (GetSlot(slot) == obj) return true; //already in slot
    if (GetSlot(slot) != nullptr) return false;
    if (cleanOld) obj->PrePositionChange(context);

    if (slot > -1 && slot < (int32_t)Contained.size()) {
        if (!obj->GhostImage) {
            Contained[slot] = obj;
            obj->Container = this;
            obj->ContainerSlot = (int16_t)slot;
        }
        if (UseWorld && obj->WorldUI) {
            obj->WorldUI->Container = this->WorldUI.get();
            obj->WorldUI->ContainerSlot = slot;
        }
        obj->Position(Position()); //TODO: is physical position the same as the slot offset position?
        if (cleanOld) obj->PositionChange(context, false);
        return true;
    }
    return false;
}

VMEntity* VMGameObject::GetSlot(int32_t slot) {
    if (slot > -1 && slot < (int32_t)Contained.size()) return Contained[slot];
    else return nullptr;
}

int32_t VMGameObject::GetSlotHeight(int32_t slot) {
    if (slot < TotalSlots()) return Slots->Slots[0][0].Height;
    else return -1;
}

void VMGameObject::ClearSlot(int32_t slot) {
    if (slot > -1 && slot < (int32_t)Contained.size()) {
        if (Contained[slot] == nullptr) return; //what..
        Contained[slot]->Container = nullptr;
        Contained[slot]->ContainerSlot = -1;
        if (UseWorld && Contained[slot]->WorldUI) {
            Contained[slot]->WorldUI->Container = nullptr;
            Contained[slot]->WorldUI->ContainerSlot = -1;
        }
        Contained[slot] = nullptr;
    }
}

// End Container SLOTs interface

void* VMGameObject::GetIcon(void* /*gd*/, int32_t /*store*/) {
    // STUB(3ds): returned the catalog BMP (CatalogStringsID + store*2000) as a Texture2D.
    return nullptr;
}

void VMGameObject::PrePositionChange(VMContext* context) {
    Footprint.reset();
    if (GhostImage && UseWorld) {
        if (WorldUI && WorldUI->Container != nullptr) {
            WorldUI->Container = nullptr;
            WorldUI->ContainerSlot = 0;
        }
        return;
    }

    context->UnregisterObjectPos(this);
    if (Container != nullptr) {
        Container->ClearSlot(ContainerSlot);
        return;
    }
    if (Position() == LotTilePos::OUT_OF_WORLD) return;

    auto arch = context->Architecture();
    if (((VMEntityFlags2)ObjectData[(int)VMStackObjectVariable::FlagField2] & (VMEntityFlags2::ArchitectualWindow | VMEntityFlags2::ArchitectualDoor)) > 0) { //if wall or door, attempt to place style on wall
        auto placeFlags = (WallPlacementFlags)ObjectData[(int)VMStackObjectVariable::WallPlacementFlags];
        auto dir = DirectionToWallOff(Direction());
        if ((placeFlags & WallPlacementFlags::WallRequiredInFront) > 0) SetWallStyle((dir) % 4, arch, 0);
        if ((placeFlags & WallPlacementFlags::WallRequiredOnRight) > 0) SetWallStyle((dir + 1) % 4, arch, 0);
        if ((placeFlags & WallPlacementFlags::WallRequiredBehind) > 0) SetWallStyle((dir + 2) % 4, arch, 0);
        if ((placeFlags & WallPlacementFlags::WallRequiredOnLeft) > 0) SetWallStyle((dir + 3) % 4, arch, 0);
    }
    SetWallUse(arch, false);
    if (GetValue(VMStackObjectVariable::Category) == 8) context->Architecture()->SetObjectSupported(Position().TileX(), Position().TileY(), Position().Level, false);

    VMEntity::PrePositionChange(context);
}

std::unique_ptr<Routing::VMObstacle> VMGameObject::GetObstacle(const LotTilePos& pos, DirectionT dir) {
    if (GetFlag(VMEntityFlags::HasZeroExtent)) return nullptr;

    auto idir = (DirectionToWallOff(dir) * 4);
    uint32_t rotatedFPM = (uint32_t)(Object->OBJ->FootprintMask << idir);
    rotatedFPM = (rotatedFPM >> 16) | (rotatedFPM & 0xFFFF);

    int tileWidth = Object->OBJ->TileWidth / 2;
    if (tileWidth == 0) tileWidth = 8;

    return std::unique_ptr<Routing::VMObstacle>(new Routing::VMObstacle(
        (pos.x + tileWidth) - ((int)(rotatedFPM >> 4) & 0xF),
        (pos.y + tileWidth) - ((int)(rotatedFPM >> 8) & 0xF),
        (pos.x - tileWidth) + ((int)(rotatedFPM >> 12) & 0xF),
        (pos.y - tileWidth) + ((int)rotatedFPM & 0xF)));
}

void VMGameObject::PositionChange(VMContext* context, bool noEntryPoint) {
    if (GhostImage) return;

    auto room = context->GetObjectRoom(this);
    SetRoom(room);

    for (size_t i = 0; i < Contained.size(); i++) {
        if (Contained[i] != nullptr) {
            context->UnregisterObjectPos(Contained[i]);
            Contained[i]->Position(Position());
            Contained[i]->PositionChange(context, noEntryPoint); //recursive
        }
    }

    context->RegisterObjectPos(this);
    if (Container != nullptr) return;
    if (Position() == LotTilePos::OUT_OF_WORLD) return;

    auto arch = context->Architecture();

    if (((VMEntityFlags2)ObjectData[(int)VMStackObjectVariable::FlagField2] & (VMEntityFlags2::ArchitectualWindow | VMEntityFlags2::ArchitectualDoor)) > 0) { //if wall or door, attempt to place style on wall
        if (Object->OBJ->WallStyle > 21 && Object->OBJ->WallStyle < 256) {
            //first thing's first, is the style between 22-255 inclusive? If it is, then the style is stored in the object. Need to load its sprites and change the id for the objd.
            auto id = Object->OBJ->WallStyleSpriteID;
            FSO::Content::WallStyle style;
            style.WallsUpFar = Object->Resource->Get<SPR>(id);
            style.WallsUpMedium = Object->Resource->Get<SPR>((uint16_t)(id + 1));
            style.WallsUpNear = Object->Resource->Get<SPR>((uint16_t)(id + 2));
            style.WallsDownFar = Object->Resource->Get<SPR>((uint16_t)(id + 3));
            style.WallsDownMedium = Object->Resource->Get<SPR>((uint16_t)(id + 4));
            style.WallsDownNear = Object->Resource->Get<SPR>((uint16_t)(id + 5));
            Object->OBJ->WallStyle = FSO::Content::Content::Get()->WorldWalls->AddDynamicWallStyle(style);
        }

        auto placeFlags = (WallPlacementFlags)ObjectData[(int)VMStackObjectVariable::WallPlacementFlags];
        auto dir = DirectionToWallOff(Direction());
        if ((placeFlags & WallPlacementFlags::WallRequiredInFront) > 0) SetWallStyle((dir) % 4, arch, Object->OBJ->WallStyle);
        if ((placeFlags & WallPlacementFlags::WallRequiredOnRight) > 0) SetWallStyle((dir + 1) % 4, arch, Object->OBJ->WallStyle);
        if ((placeFlags & WallPlacementFlags::WallRequiredBehind) > 0) SetWallStyle((dir + 2) % 4, arch, Object->OBJ->WallStyle);
        if ((placeFlags & WallPlacementFlags::WallRequiredOnLeft) > 0) SetWallStyle((dir + 3) % 4, arch, Object->OBJ->WallStyle);
    }
    SetWallUse(arch, true);
    if (GetValue(VMStackObjectVariable::Category) == 8) context->Architecture()->SetObjectSupported(Position().TileX(), Position().TileY(), Position().Level, true);

    if (EntryPoints[8].ActionFunction != 0) UpdateDynamicMultitile(context);

    VMEntity::PositionChange(context, noEntryPoint);
}
}}
