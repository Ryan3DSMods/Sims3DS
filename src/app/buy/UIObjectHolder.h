/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims app — FreeSims UI/Panels/UIObjectHolder.cs: holding an object in buy/build mode (a ghost copy that
// follows the cursor with its "Tile Cursor" (GUID 0x437) footprint, rotation by dragging away from the click
// point, placement errors from UIText.iff STR# 137), and the commands it sends when the object is put down:
// VMNetBuyObjectCmd, VMNetMoveObjectCmd, VMNetDeleteObjectCmd (sell back).
//
// ADDED(sims): there is no net driver, so the commands' Execute bodies run directly (BuyObject, MoveObject,
// DeleteObject below). TS1(sims): The Sims has no TSO budget objects: the family's money is the VM global
// pair GLOBAL_BUDGET_DIV/MOD (LotSession::Budget), the purchase price is the catalogue (OBJD) price, the resale value
// starts at price - InitialDepreciation (clamped to DepreciationLimit, as in VMNetBuyObjectCmd) and anything bought
// in the current buy/build session sells back at full price (the original's "changed my mind" refund).
#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <set>
#include <vector>
#include "core/math.h"
#include "world/model/LotTilePos.h"
#include "world/model/Direction.h"
#include "simantics/model/VMPlacementError.h"

namespace FSO { namespace SimAntics { class VM; class VMEntity; class VMAvatar; namespace Entities { class VMMultitileGroup; } } }
namespace FSO { namespace LotView { class World; } }

namespace simapp {

class LotSession;

class UIObjectSelection {
public:
    int16_t MoveTarget = 0;
    FSO::SimAntics::Entities::VMMultitileGroup* Group = nullptr;
    std::vector<FSO::SimAntics::VMEntity*> CursorTiles;
    FSO::LotView::Model::LotTilePos PreviousTile;
    FSO::LotView::Model::Direction Dir = FSO::LotView::Model::Direction::NORTH;
    st::Vector2 TilePos;
    st::Vector2 TilePosOffset;
    bool Clicked = false, CanDelete = false;
    FSO::SimAntics::Model::VMPlacementError CanPlace = FSO::SimAntics::Model::VMPlacementError::Success;
    int8_t Level = 1;
    int Price = 0;
    uint32_t GUID = 0;               // ADDED(sims): catalogue GUID (C# read it back from the group)
    bool Forced = false;             // TS1(sims): picked up through the move_objects cheat
    bool IsSim = false;              // TS1(sims): move_objects: MoveTarget is a Sim (the group is its tile cursor)
    bool IsBought() const { return MoveTarget != 0; }
};

class UIObjectHolder {
public:
    UIObjectHolder(LotSession* session);
    ~UIObjectHolder();

    LotSession* Session;
    FSO::SimAntics::Model::VMPlacementError LastError = FSO::SimAntics::Model::VMPlacementError::Success;
    bool ShowTooltip = false;
    bool DirChanged = false;
    std::unique_ptr<UIObjectSelection> Holding;

    std::function<void(UIObjectSelection&)> OnPickup, OnDelete, OnPutDown;
    /// ADDED(sims): money moved (negative = spent) at an entity (money pop-up, sounds).
    std::function<void(int delta, FSO::SimAntics::VMEntity* where)> OnFunds;
    /// ADDED(sims): objects bought this buy/build session (ObjectIDs) sell back at full price.
    std::set<int16_t> BoughtThisSession;

    void SetSelected(FSO::SimAntics::Entities::VMMultitileGroup* group, int price, uint32_t guid);
    void MoveSelected(st::Vector2 pos, int8_t level);
    void ClearSelected();
    /// Rotates the held object by quarter turns (+1 clockwise on screen). ADDED(sims): buttons for C#'s drag.
    void Rotate(int dir);

    // mouse in top-screen pixels (C# UpdateState.MouseState); the platform maps the cursor / stylus onto it
    void MouseDown(int x, int y);
    void MouseUp(int x, int y);
    void Update(int mouseX, int mouseY, bool mouseIsDown);
    /// Hand tool: picks up the object under (x, y) (C# Update with MouseClicked and nothing held).
    bool PickUpAt(int x, int y);
    /// TS1(sims): move_objects on: picks up a Sim (put down = move + reset, sell = remove from the lot).
    bool PickUpSim(FSO::SimAntics::VMAvatar* av, int x, int y);

    void SellBack();
    /// Puts the held object down if it can be placed (C# MouseUp with Holding.Clicked). Returns true if placed.
    /// errorSound: ui_error when it cannot go there (the mouse path already played it in Update). // FIX(sims) SND
    bool PutDown(bool errorSound = true);
    /// Resale value of a placed object (VMNetDeleteObjectCmd refund).
    int SaleValue(FSO::SimAntics::VMEntity* obj);

private:
    int MouseDownX = 0, MouseDownY = 0;
    bool MouseIsDown = false, MouseWasDown = false, MouseClicked = false;
    FSO::LotView::Model::Direction Rotation = FSO::LotView::Model::Direction::NORTH;
    bool BuyHolding();
    // ADDED(sims): the VMNet*Cmd Execute bodies
    bool BuyObject(uint32_t guid, int16_t x, int16_t y, int8_t level, FSO::LotView::Model::Direction dir, int price);
    bool MoveObject(int16_t objectID, int16_t x, int16_t y, int8_t level, FSO::LotView::Model::Direction dir);
    bool DeleteObject(int16_t objectID, bool cleanupAll);
};

}
