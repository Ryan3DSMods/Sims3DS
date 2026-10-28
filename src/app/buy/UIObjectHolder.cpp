/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims app — see UIObjectHolder.h (FreeSims UI/Panels/UIObjectHolder.cs + NetPlay VMNet{Buy,Move,Delete}ObjectCmd)
#include "app/buy/UIObjectHolder.h"
#include "app/LotSession.h"
#include "app/WorldPicking.h"
#include "app/buy/UICatalog.h"
#include "app/UISounds.h"
#include "app/ui/CheatState.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/entities/VMGameObject.h"
#include "simantics/entities/VMMultitileGroup.h"
#include "simantics/model/VMStackObjectVariable.h"
#include "files/formats/iff/chunks/OBJD.h"
#include "content/ContentManager.h"
#include "world/World.h"
#include "world/components/ObjectComponent.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace simapp {

using namespace FSO::SimAntics;
using FSO::SimAntics::Model::VMPlacementError;
using FSO::SimAntics::Model::VMStackObjectVariable;
using FSO::LotView::Model::LotTilePos;
using FSO::LotView::Model::Direction;

static const uint32_t TILE_CURSOR_GUID = 0x00000437;

UIObjectHolder::UIObjectHolder(LotSession* session) : Session(session) {}

UIObjectHolder::~UIObjectHolder() { ClearSelected(); }

void UIObjectHolder::SetSelected(Entities::VMMultitileGroup* Group, int price, uint32_t guid) {
    if (Holding != nullptr) ClearSelected();
    auto vm = Session->VM();
    Holding.reset(new UIObjectSelection());
    Holding->Group = Group;
    Holding->PreviousTile = Holding->Group->BaseObject()->Position();
    Holding->Dir = Group->Objects[0]->Direction();
    Holding->GUID = guid;
    for (size_t i = 0; i < Group->Objects.size(); i++) {
        auto target = Group->Objects[i];
        target->SetRoom(65534);
        if (auto go = dynamic_cast<VMGameObject*>(target)) {
            if (auto oc = dynamic_cast<FSO::LotView::Components::ObjectComponent*>(go->WorldUI.get())) oc->ForceDynamic = true;
        }
        auto cursorGroup = vm->Context->CreateObjectInstance(TILE_CURSOR_GUID, LotTilePos(target->Position()), Direction::NORTH, true, true);
        if (cursorGroup == nullptr || cursorGroup->Objects.empty()) continue;
        auto ct = cursorGroup->Objects[0];
        ct->SetPosition(LotTilePos(0, 0, 1), Direction::NORTH, vm->Context);
        if (auto oc = dynamic_cast<FSO::LotView::Components::ObjectComponent*>(ct->WorldUI.get())) oc->ForceDynamic = true;
        Holding->CursorTiles.push_back(ct);
    }
    Holding->TilePosOffset = st::Vector2(0, 0);
    // C#: WorldCatalog.GetItemByGUID(guid).Price; TS1: the OBJD catalogue price
    Holding->Price = price;
}

void UIObjectHolder::MoveSelected(st::Vector2 pos, int8_t level) {
    auto vm = Session->VM();
    auto World = Session->World();
    Holding->TilePos = pos;
    Holding->Level = level;

    //first, eject the object from any slots
    for (auto obj : Holding->Group->Objects) {
        if (obj->Container != nullptr) obj->Container->ClearSlot(obj->ContainerSlot);
    }

    //rotate through to try all configurations
    auto dir = Holding->Dir;
    VMPlacementError status = VMPlacementError::Success;
    for (int i = 0; i < 4; i++) {
        auto res = Holding->Group->ChangePosition(LotTilePos::FromBigTile((int16_t)std::floor(pos.X), (int16_t)std::floor(pos.Y), World->State.Level()), dir, vm->Context);
        status = res.Status;
        if (getenv("ST_DEBUG_PLACE")) printf("[holder] place at (%.1f,%.1f) -> %d obstacle %s\n", pos.X, pos.Y, (int)status, res.Object ? res.Object->ToString().c_str() : "-");
        if (getenv("ST_DEBUG_PLACE") && res.Object && res.Object->Footprint) { auto f = res.Object->Footprint.get(); auto p2 = res.Object->Position(); printf("   obstacle fp %d,%d-%d,%d pos %d,%d L%d\n", f->x1, f->y1, f->x2, f->y2, p2.x, p2.y, (int)p2.Level); auto g = Holding->Group->Objects[0]->GetObstacle(LotTilePos::FromBigTile((int16_t)std::floor(pos.X), (int16_t)std::floor(pos.Y), World->State.Level()), dir); if (g) printf("   mine %d,%d-%d,%d\n", g->x1, g->y1, g->x2, g->y2); }
        if (status != VMPlacementError::MustBeAgainstWall) break;
        dir = (Direction)(((((int)dir) << 6) & 255) | (((int)dir) >> 2));
    }
    if (Holding->Dir != dir) Holding->Dir = dir;
    // TS1(sims): FreeSO VMPlaceRequestFlags.UserBuildableLimit — nothing bought or moved onto the street /
    // outside the lot's buildable area (house ARRY 8 flag 32)
    if (status == VMPlacementError::Success) {
        for (auto obj : Holding->Group->Objects) {
            if (vm->Context->IsUserOutOfBounds(obj->Position())) { status = VMPlacementError::LocationOutOfBounds; break; }
        }
    }

    if (status != VMPlacementError::Success) {
        Holding->Group->ChangePosition(LotTilePos::OUT_OF_WORLD, Holding->Dir, vm->Context);
        Holding->Group->SetVisualPosition(st::Vector3(std::floor(pos.X), std::floor(pos.Y),
            (((Holding->Group->Objects[0]->GetValue(VMStackObjectVariable::AllowedHeightFlags) & 1) == 1) ? 0 : 4.0f / 5.0f) + (World->State.Level() - 1) * 2.95f),
            //^ if we can't be placed on the floor, default to table height.
            Holding->Dir, vm->Context);
    }

    for (size_t i = 0; i < Holding->Group->Objects.size() && i < Holding->CursorTiles.size(); i++) {
        auto target = Holding->Group->Objects[i];
        auto tpos = target->VisualPosition();
        tpos.Z = (World->State.Level() - 1) * 2.95f;
        if (Holding->CursorTiles[i]->MultitileGroup != nullptr)
            Holding->CursorTiles[i]->MultitileGroup->SetVisualPosition(tpos, Holding->Dir, vm->Context);
    }
    Holding->CanPlace = status;
}

void UIObjectHolder::ClearSelected() {
    //TODO: selected items are only spooky ghosts of the items themselves.
    if (Holding != nullptr && Session != nullptr && Session->VM() != nullptr) {
        auto ctx = Session->VM()->Context;
        Holding->Group->Delete(ctx);
        for (auto ct : Holding->CursorTiles) {
            ct->Delete(true, ctx);
            if (auto oc = dynamic_cast<FSO::LotView::Components::ObjectComponent*>(ct->WorldUI.get())) oc->ForceDynamic = false;
        }
    }
    Holding.reset();
}

void UIObjectHolder::Rotate(int dir) {
    if (Holding == nullptr) return;
    int d = (int)Holding->Dir;
    // Direction bits N=1 E=4 S=16 W=64: a quarter turn is a 2 bit rotation of the byte
    if (dir > 0) d = ((d << 2) & 255) | (d >> 6);
    else d = ((d << 6) & 255) | (d >> 2);
    Holding->Dir = (Direction)d;
    DirChanged = true;
    PlayUISound(UISounds::ObjectRotate); // FIX(sims) SND: C# Update's drag rotation
    MoveSelected(Holding->TilePos, Holding->Level);
}

void UIObjectHolder::MouseDown(int x, int y) {
    MouseIsDown = true;
    MouseDownX = x;
    MouseDownY = y;
    if (Holding != nullptr) {
        Rotation = Holding->Dir;
        DirChanged = false;
    }
}

void UIObjectHolder::MouseUp(int x, int y) {
    MouseIsDown = false;
    if (Holding != nullptr && Holding->Clicked) PutDown(false); // C#: no sound when it can't go there (Update played it)
    ShowTooltip = false;
}

bool UIObjectHolder::PutDown(bool errorSound) {
    if (Holding == nullptr) return false;
    if (Holding->CanPlace != VMPlacementError::Success) {
        LastError = Holding->CanPlace;
        if (errorSound) PlayUISound(UISounds::Error); // FIX(sims) SND: the touch "place" (C# ShowErrorAtMouse)
        return false;
    }
    auto putDown = *Holding; // copy for the callback
    // FIX(sims) SND: C# MouseUp: HITVM.PlaySoundEvent(IsBought ? ObjectMovePlace : ObjectPlace) — a purchase is the
    // cash register (ui_object_place.xa, the same sample as ui_buy_place), a move the put-down thud
    bool ok;
    if (Holding->IsBought()) {
        auto pos = Holding->Group->BaseObject()->Position();
        ok = MoveObject(Holding->MoveTarget, pos.x, pos.y, pos.Level, Holding->Dir);
    } else {
        ok = BuyHolding();
    }
    PlayUISound(!ok ? UISounds::Error : (putDown.IsBought() ? UISounds::ObjectMovePlace : UISounds::ObjectPlace)); // ADDED(sims): Error when the command failed
    putDown.Group = nullptr;
    putDown.CursorTiles.clear();
    ClearSelected();
    if (OnPutDown) OnPutDown(putDown); //call this after so that buy mode etc can produce more.
    return true;
}

bool UIObjectHolder::BuyHolding() {
    auto pos = Holding->Group->BaseObject()->Position();
    auto base = Holding->Group->BaseObject();
    uint32_t GUID = Holding->GUID;
    if (GUID == 0) GUID = (Holding->Group->MultiTile && base->MasterDefinition) ? base->MasterDefinition->GUID : base->Object->OBJ->GUID;
    return BuyObject(GUID, pos.x, pos.y, pos.Level, Holding->Dir, Holding->Price);
}

void UIObjectHolder::SellBack() {
    if (Holding == nullptr) return;
    if (Holding->IsBought()) {
        if (!Holding->CanDelete) { LastError = VMPlacementError::CannotDeleteObject; ShowTooltip = true; PlayUISound(UISounds::Error); return; } // FIX(sims) SND
        DeleteObject(Holding->MoveTarget, true);
        PlayUISound(UISounds::MoneyBack); // FIX(sims) SND: C# SellBack
    }
    auto sel = *Holding;
    sel.Group = nullptr; sel.CursorTiles.clear();
    ClearSelected();
    if (OnDelete) OnDelete(sel);
}

void UIObjectHolder::Update(int mouseX, int mouseY, bool mouseIsDown) {
    auto World = Session->World();
    MouseIsDown = mouseIsDown;
    MouseClicked = (MouseIsDown && (!MouseWasDown));
    if (Holding != nullptr) {
        if (MouseClicked) Holding->Clicked = true;
        if (MouseIsDown && Holding->Clicked) {
            bool updatePos = MouseClicked;
            int xDiff = mouseX - MouseDownX;
            int yDiff = mouseY - MouseDownY;
            // ADDED(sims): 24 px instead of 64 (400x240 screen)
            if (std::sqrt((double)(xDiff * xDiff + yDiff * yDiff)) > 24) {
                int dir;
                if (xDiff > 0) dir = (yDiff > 0) ? 1 : 0;
                else dir = (yDiff > 0) ? 2 : 3;
                auto newDir = (Direction)(1 << (((dir + 4 - (int)World->State.Rotation()) % 4) * 2));
                if (newDir != Holding->Dir || MouseClicked) {
                    updatePos = true;
                    PlayUISound(UISounds::ObjectRotate); // FIX(sims) SND
                    Holding->Dir = newDir;
                    DirChanged = true;
                }
            }
            if (updatePos) {
                MoveSelected(Holding->TilePos, Holding->Level);
                if (!Holding->IsBought() && Holding->CanPlace == VMPlacementError::Success && Session->Budget() < Holding->Price)
                    Holding->CanPlace = VMPlacementError::InsufficientFunds;
                ShowTooltip = Holding->CanPlace != VMPlacementError::Success;
                LastError = Holding->CanPlace;
                if (ShowTooltip) PlayUISound(UISounds::Error); // FIX(sims) SND: C# Update
            }
        } else {
            auto tilePos = World->State.WorldSpace.GetTileAtPosWithScroll(st::Vector2((float)mouseX, (float)mouseY));
            tilePos = st::Vector2(tilePos.X + Holding->TilePosOffset.X, tilePos.Y + Holding->TilePosOffset.Y);
            MoveSelected(tilePos, 1);
            if (!Holding->IsBought() && Holding->CanPlace == VMPlacementError::Success && Session->Budget() < Holding->Price)
                Holding->CanPlace = VMPlacementError::InsufficientFunds;
            LastError = Holding->CanPlace;
        }
    }
    MouseWasDown = MouseIsDown;
}

bool UIObjectHolder::PickUpAt(int x, int y) {
    //not holding an object, but one can be selected
    auto vm = Session->VM();
    auto World = Session->World();
    auto hit = PickObjectAt(*World, &Session->LastFrame(), x, y, 3);
    int16_t newHover = hit.ObjectID;
    auto ent = newHover != 0 ? vm->GetObjectById(newHover) : nullptr;
    bool cheat = CheatState::Get().MoveObjects; // TS1(sims): move_objects on: Sims and every object
    auto av = dynamic_cast<VMAvatar*>(ent);
    if (av != nullptr && cheat) return PickUpSim(av, x, y);
    if (ent == nullptr || dynamic_cast<VMGameObject*>(ent) == nullptr || ent->MultitileGroup == nullptr) return false;
    auto objGroup = ent->MultitileGroup;
    auto objBasePos = objGroup->BaseObject()->Position();
    if (objBasePos.Level != World->State.Level()) {
        LastError = VMPlacementError::CantEffectFirstLevelFromSecondLevel;
        ShowTooltip = true;
        PlayUISound(UISounds::Error); // FIX(sims) SND: C# Update
        return false;
    }
    // TS1(sims): C# UIObjectHolder: success = BaseObject.IsUserMovable(context, false) (the mailbox, the outside
    // trash can, objects in use stay put); the move_objects cheat skips the check (GameFacade.EnableMod in C#)
    if (!cheat) {
        auto movable = objGroup->BaseObject()->IsUserMovable(vm->Context, false);
        if (movable != VMPlacementError::Success) { LastError = movable; ShowTooltip = true; PlayUISound(UISounds::Error); return false; } // FIX(sims) SND
    }
    auto ghostGroup = vm->Context->GhostCopyGroup(objGroup);
    if (ghostGroup == nullptr) return false;
    auto base = objGroup->BaseObject();
    uint32_t guid = (objGroup->MultiTile && base->MasterDefinition) ? base->MasterDefinition->GUID : base->Object->OBJ->GUID;
    SetSelected(ghostGroup, 0, guid);
    // C#: Validator.GetDeleteMode != Disallowed && IsUserMovable(context, true) == Success
    auto canDelete = objGroup->BaseObject()->IsUserMovable(vm->Context, true) == VMPlacementError::Success;
    Holding->CanDelete = canDelete || cheat; // TS1(sims): move_objects deletes anything
    Holding->Forced = cheat;
    Holding->MoveTarget = newHover;
    auto tile = World->State.WorldSpace.GetTileAtPosWithScroll(st::Vector2((float)x, (float)y));
    Holding->TilePosOffset = st::Vector2(objBasePos.x / 16.0f - tile.X, objBasePos.y / 16.0f - tile.Y);
    Holding->TilePos = st::Vector2(objBasePos.x / 16.0f, objBasePos.y / 16.0f);
    if (OnPickup) OnPickup(*Holding);
    return true;
}

// TS1(sims): move_objects on: a Sim is picked up like an object. The hand holds its tile cursor; putting it down
// moves the Sim there and resets it (its queue is dropped, as in the original); "Sell" deletes the Sim from the lot
// (the family keeps it: it comes back when the lot is loaded again, the original's well-known trick).
bool UIObjectHolder::PickUpSim(VMAvatar* av, int x, int y) {
    auto vm = Session->VM();
    auto World = Session->World();
    auto pos = av->Position();
    if (pos == LotTilePos::OUT_OF_WORLD) return false;
    if (pos.Level != World->State.Level()) {
        LastError = VMPlacementError::CantEffectFirstLevelFromSecondLevel;
        ShowTooltip = true;
        PlayUISound(UISounds::Error); // FIX(sims) SND
        return false;
    }
    auto ghost = vm->Context->CreateObjectInstance(TILE_CURSOR_GUID, LotTilePos::OUT_OF_WORLD, Direction::NORTH, true, true);
    if (ghost == nullptr || ghost->Objects.empty()) return false;
    SetSelected(ghost, 0, 0);
    Holding->Dir = av->Direction();
    Holding->CanDelete = true;
    Holding->Forced = true;
    Holding->IsSim = true;
    Holding->MoveTarget = av->ObjectID;
    auto tile = World->State.WorldSpace.GetTileAtPosWithScroll(st::Vector2((float)x, (float)y));
    Holding->TilePosOffset = st::Vector2(pos.TileX() - tile.X + 0.5f, pos.TileY() - tile.Y + 0.5f);
    Holding->TilePos = st::Vector2(pos.TileX() + 0.5f, pos.TileY() + 0.5f);
    if (OnPickup) OnPickup(*Holding);
    return true;
}

int UIObjectHolder::SaleValue(VMEntity* obj) {
    if (dynamic_cast<VMAvatar*>(obj) != nullptr) return 0; // TS1(sims): move_objects deleting a Sim
    if (obj == nullptr || obj->MultitileGroup == nullptr) return 0;
    auto group = obj->MultitileGroup;
    auto base = group->BaseObject();
    auto def = base->MasterDefinition ? base->MasterDefinition : (base->Object ? base->Object->OBJ : nullptr);
    int price = (def != nullptr) ? def->Price : 0;
    // TS1(sims): full refund for what was bought in this session
    if (BoughtThisSession.count(base->ObjectID)) return price;
    // TS1(sims): the object's current value (object data 41: bought price after depreciation, saved with the lot)
    int current = base->GetValue(VMStackObjectVariable::CurrentValue);
    if (current > 0) return current;
    if (group->Price > 0) return group->Price;
    // objects that came with the lot: catalogue price after the initial depreciation (VMNetBuyObjectCmd rule)
    if (def != nullptr && price > def->DepreciationLimit) {
        price -= def->InitialDepreciation;
        if (price < def->DepreciationLimit) price = def->DepreciationLimit;
    }
    return std::max(0, price);
}

// ---------------------------------------------------------------- VMNet*Cmd.Execute (local)

bool UIObjectHolder::BuyObject(uint32_t GUID, int16_t x, int16_t y, int8_t level, Direction dir, int price) {
    auto vm = Session->VM();
    if (Session->Budget() < price) return false;
    // TryPlace
    auto group = vm->Context->CreateObjectInstance(GUID, LotTilePos(x, y, level), dir, false, true);
    if (group == nullptr) return false;
    group->ExecuteEntryPoint(11, vm->Context); //User Placement
    if (group->Objects.empty()) return false;
    if (group->BaseObject()->Position() == LotTilePos::OUT_OF_WORLD) {
        group->Delete(vm->Context);
        return false;
    }
    int salePrice = price;
    auto def = group->BaseObject()->MasterDefinition;
    if (def == nullptr) def = group->BaseObject()->Object->OBJ;
    auto limit = def->DepreciationLimit;
    if (salePrice > limit) { //only try to deprecate if we're above the limit. Prevents objects with a limit above their price being money fountains.
        salePrice -= def->InitialDepreciation;
        if (salePrice < limit) salePrice = limit;
    }
    group->Price = salePrice;
    // TS1(sims): the value that depreciates daily and is paid back on sale (object data 41 CurrentValue)
    group->BaseObject()->SetValue(VMStackObjectVariable::CurrentValue, (int16_t)std::min(32767, salePrice));
    // TS1(sims): pay from the family budget
    Session->SetBudget(Session->Budget() - price);
    Session->ObjectsSpent += salePrice; // TS1(sims): SIMI ObjectsValue
    BoughtThisSession.insert(group->BaseObject()->ObjectID);
    if (OnFunds) OnFunds(-price, group->BaseObject());
    return true;
}

bool UIObjectHolder::MoveObject(int16_t ObjectID, int16_t x, int16_t y, int8_t level, Direction dir) {
    auto vm = Session->VM();
    VMEntity* obj = vm->GetObjectById(ObjectID);
    if (obj == nullptr) return false;
    if (auto av = dynamic_cast<VMAvatar*>(obj)) { // TS1(sims): move_objects: the Sim goes there and resets
        if (av->Container != nullptr) av->Container->ClearSlot(av->ContainerSlot);
        auto r = av->SetPosition(LotTilePos(x, y, level), dir, vm->Context);
        if (r.Status != VMPlacementError::Success) return false;
        av->Reset(vm->Context);
        return true;
    }
    auto result = obj->SetPosition(LotTilePos(x, y, level), dir, vm->Context);
    if (result.Status == VMPlacementError::Success) {
        obj->MultitileGroup->ExecuteEntryPoint(11, vm->Context); //User Placement
        return true;
    }
    return false;
}

bool UIObjectHolder::DeleteObject(int16_t ObjectID, bool CleanupAll) {
    auto vm = Session->VM();
    VMEntity* obj = vm->GetObjectById(ObjectID);
    if (obj == nullptr) return false;
    if (auto av = dynamic_cast<VMAvatar*>(obj)) { // TS1(sims): move_objects: the Sim leaves the lot (not the family)
        int idx = Session->IndexOfFamily(av);
        if (idx >= 0) {
            Session->Family.erase(Session->Family.begin() + idx);
            if (Session->Selected >= (int)Session->Family.size()) Session->Selected = (int)Session->Family.size() - 1;
            else if (Session->Selected > idx) Session->Selected--;
            if (Session->Selected >= 0) Session->Select(Session->Selected);
        }
        av->Delete(true, vm->Context);
        return true;
    }
    int refund = SaleValue(obj);
    auto base = obj->MultitileGroup ? obj->MultitileGroup->BaseObject() : obj;
    auto pos = base->Position();
    BoughtThisSession.erase(base->ObjectID);
    obj->Delete(CleanupAll, vm->Context);
    // TS1(sims): GlobalLink.PerformTransaction(Maxis -> caller, group.Price) = back into the family budget
    Session->SetBudget(Session->Budget() + refund);
    Session->ObjectsSpent -= refund; // TS1(sims): SIMI ObjectsValue
    if (OnFunds) OnFunds(refund, nullptr);
    (void)pos;
    return true;
}

}
