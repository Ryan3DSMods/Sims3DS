/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims app — one loaded lot: the world renderer, the SimAntics VM, the family, and the player's
// commands (select Sim, pie menu, queue/cancel interactions), mirroring FreeSims' UILotControl /
// UIPieMenu / UIInteractionQueue and the VMNet*Cmd commands they send.
#pragma once
#include <cstdint>
#include <memory>
#include "core/math.h"
#include <string>
#include <vector>
#include <functional>
#include "world/Framebuffer.h"

namespace FSO {
namespace SimAntics { namespace Model { struct VMArchitectureCommand; } }
namespace LotView { class World; namespace Model { class Blueprint; } }
namespace SimAntics { class VM; class VMContext; class VMEntity; class VMAvatar; namespace Model { class VMDialogInfo; class VMHeadlineRendererProvider; } }
namespace Files { namespace Formats { namespace IFF { class IffFile; } } }
}

namespace simapp {

/// One pie menu choice (C# UIPieMenuItem / VMPieMenuInteraction).
struct PieChoice {
    std::string Name;      // full name ("Category/Sub/Item" splits into levels)
    uint8_t ID = 0;        // TTAB interaction index
    int16_t Param0 = 0;
};

/// A dialog raised by the VM (VMDialogInfo), shown by the UI.
struct PendingDialog {
    std::string Title, Message, Yes, No, Cancel;
    bool HasYes = false, HasNo = false, HasCancel = false;
    bool Block = false;
    int16_t CallerID = 0;
    uint8_t Type = 0;      // VMDialogOperand.Type (TS1 5/7/8/11/13 = lot selection: answer with the house number)
    int16_t IconID = 0;    // VMDialogInfo.Icon (the object whose picture the TS1 dialog shows)
    // TS1(sims): the Clothing dialog (type 6): whose outfits (the Sim) and which outfit type (caller's temp 0)
    int16_t AvatarID = 0;
    int16_t OutfitType = 0;
};

class LotSession {
public:
    LotSession();
    ~LotSession();

    /// Loads UserData/Houses/HouseNN.iff and its family. View size = the top screen.
    /// travel = arriving from another lot (expansion packs): VMTS1Travel::Arrive instead of VMTS1FamilyLoader.
    bool Load(int houseNumber, int viewW, int viewH, int scrollBuffer, std::string& error, bool travel = false);

    int HouseNumber = 0;
    std::string FamilyName;
    FSO::LotView::World* World() { return _World.get(); }
    FSO::SimAntics::VM* VM() { return _VM.get(); }
    FSO::SimAntics::VMContext* Context() { return _Ctx.get(); }
    FSO::LotView::Model::Blueprint* Blueprint = nullptr;

    // ---- family / selection ----
    std::vector<FSO::SimAntics::VMAvatar*> Family;
    int Selected = -1;
    FSO::SimAntics::VMAvatar* SelectedSim();
    void Select(int index);
    int IndexOfFamily(FSO::SimAntics::VMEntity* e) const;

    // ---- simulation ----
    /// Runs n VM updates (VM::Update alternates a tick and a fractional-animation step).
    void Update(int n);
    /// TS1(sims): the daily object depreciation (run when the game day changes)
    void DepreciateObjects();
    /// TS1(sims): build / buy spending of this session (the saver adds it to SIMI Architecture/ObjectsValue)
    int ArchitectureSpent = 0, ObjectsSpent = 0;
    int64_t Updates = 0;
    std::vector<PendingDialog> Dialogs;
    void RespondDialog(int code, const std::string& text = ""); // answers Dialogs.front() (0 yes/ok, 1 no, 2 cancel; text = ResponseText)
    int Resets = 0;               // SimAntics exceptions so far

    // ---- money (TS1(sims): the family budget is the VM global pair GLOBAL_BUDGET_DIV * 10000 + MOD)
    int Budget();
    void SetBudget(int value);
    /// ADDED(sims): VMNetArchitectureCmd executed locally (+ the TS1 cost from the family budget). Returns false
    /// (and changes nothing) when the family cannot afford it; `cost` = the cost (negative = refund).
    bool RunArchitecture(std::vector<FSO::SimAntics::Model::VMArchitectureCommand>& commands, int& cost);

    // ---- view ----
    /// World::Draw into LastFrame() (sized like the view).
    void Draw();
    FSO::LotView::Framebuffer& LastFrame() { return Frame; }
    void Rotate(int dir);       // +1 clockwise
    void Zoom(int dir);         // +1 = closer
    void ChangeLevel(int dir);
    void ScrollScreen(float dxPx, float dyPx); // moves the view by screen pixels
    void CenterOn(FSO::SimAntics::VMEntity* e);
    int WallsMode = 0;          // 0 cutaway (TS1 default "walls cutaway"), 1 walls down, 2 walls up
    void SetWallsMode(int mode);
    void UpdateCutaway();
    /// FreeSims UILotControl.Update "set cutaway around mouse": in walls-cutaway mode the walls in a 5x5 tile
    /// rectangle around the cursor (shifted towards the camera by CutCheckDir*2) are cut too. Call once per frame
    /// with the cursor's top-screen pixel; the wall layer is only re-rendered when the cut changes notably.
    void UpdateMouseCut(bool shown, int screenX, int screenY);
    bool CursorCutaway = true; // false: only the room cut (TS1 may not cut walls around the cursor, see world/STUBS.md)

    // ---- interaction (FreeSims UILotControl.OnMouse / UIPieMenu) ----
    /// Picks what is under top-screen pixel (x,y). Returns the interaction target (group leader) or the goto
    /// object for an empty walkable tile, and fills its pie menu for the selected Sim (may be empty).
    FSO::SimAntics::VMEntity* PieTargetAt(int x, int y, std::vector<PieChoice>& pie, bool& isGoto, int16_t& pickedId);
    /// VMNetInteractionCmd / VMNetGotoCmd: queue the choice on the selected Sim.
    bool PushInteraction(FSO::SimAntics::VMEntity* target, bool isGoto, const PieChoice& choice);
    /// VMNetInteractionCancelCmd (UIInteractionQueue click).
    void CancelAction(uint16_t uid);
    FSO::SimAntics::VMEntity* GotoObject = nullptr;

    /// Screen position (top view) of an entity's centre, for markers and for the host script ("tap object X").
    bool EntityScreenPos(FSO::SimAntics::VMEntity* e, float& sx, float& sy, float heightTiles = 0.0f);

private:
    int _LastDepreciationDay = -1;
    std::unique_ptr<FSO::LotView::World> _World;
    std::unique_ptr<FSO::SimAntics::VMContext> _Ctx;
    std::unique_ptr<FSO::SimAntics::Model::VMHeadlineRendererProvider> _Headlines; // app/live/UIHeadlineRenderer
    std::unique_ptr<FSO::SimAntics::VM> _VM;
    std::unique_ptr<FSO::Files::Formats::IFF::IffFile> _House;
    FSO::LotView::Framebuffer Frame;
    std::vector<bool> _RoomCuts;           // C# LastCuts: the room cut the rectangle is applied to
    st::Rectangle _MouseCutRect;           // C# MouseCutRect
    bool _LastRectCutNotable = false;      // C# LastRectCutNotable
    void ApplyMouseCut(bool force);
};

}
