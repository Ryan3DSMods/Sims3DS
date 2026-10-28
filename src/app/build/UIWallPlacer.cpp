/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims app — see UIWallPlacer.h (FreeSims UI/Panels/LotControls/UIWallPlacer.cs)
#include "app/build/UIWallPlacer.h"
#include "app/LotSession.h"
#include "app/UISounds.h"
#include "app/ui/UIUCP.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"
#include "simantics/VMArchitecture.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMGameObject.h"
#include "simantics/entities/VMMultitileGroup.h"
#include "simantics/model/VMStackObjectVariable.h"
#include "world/World.h"
#include "world/components/ObjectComponent.h"
#include <cmath>

namespace simapp {

using namespace FSO::SimAntics;
using FSO::SimAntics::Model::VMArchitectureCommand;
using FSO::SimAntics::Model::VMArchitectureCommandType;
using FSO::LotView::Model::LotTilePos;
using FSO::LotView::Model::Direction;

bool SameArchitectureCommand(const VMArchitectureCommand& a, const VMArchitectureCommand& b) {
    return a.Type == b.Type && a.x == b.x && a.y == b.y && a.level == b.level && a.x2 == b.x2 && a.y2 == b.y2
        && a.pattern == b.pattern && a.style == b.style;
}

const st::Point UIWallPlacer::DirUnits[8] = {
    st::Point(1, 0),
    st::Point(1, 1),
    st::Point(0, 1),
    st::Point(-1, 1),
    st::Point(-1, 0),
    st::Point(-1, -1),
    st::Point(0, -1),
    st::Point(1, -1),
};

static double PosMod(double x, double m) { double r = std::fmod(x, m); return r < 0 ? r + m : r; }

UIWallPlacer::UIWallPlacer(LotSession* session, const std::vector<int>& parameters) : Session(session) {
    Pattern = (uint16_t)parameters[0];
    Style = (uint16_t)parameters[1];
    if (Style == 1) {
        DrawPattern = 255;
        // FIX(sims): C# draws the preview with style 255; its RunCommands looks the style up with TryGetValue, which
        // leaves index 0 for an unknown style, so the preview ran. The port's lookup (-1 = "MUST be purchasable style")
        // skipped it: no wall preview and no cost tooltip while dragging. The preview pattern (255) stays; style 1 is
        // the plain wall (WallTile thick for 1 and 255 alike), so the tooltip shows the real price.
        DrawStyle = Style;
    } else {
        DrawPattern = Pattern;
        DrawStyle = Style;
    }
    auto vm = Session->VM();
    WallCursor = vm->Context->CreateObjectInstance(0x00000439, LotTilePos::OUT_OF_WORLD, Direction::NORTH, true, true);
    if (WallCursor != nullptr && !WallCursor->Objects.empty()) {
        if (auto oc = dynamic_cast<FSO::LotView::Components::ObjectComponent*>(WallCursor->Objects[0]->WorldUI.get())) oc->ForceDynamic = true;
    }
}

//0: wall
//1: bulldoze
//2: paint
//3: rect
void UIWallPlacer::SetCursorGraphic(int16_t id) {
    if (WallCursor == nullptr || WallCursor->Objects.empty()) return;
    auto obj = WallCursor->Objects[0];
    if (obj->GetValue(Model::VMStackObjectVariable::Graphic) == id) return; // ADDED(sims): no redundant refresh
    obj->SetValue(Model::VMStackObjectVariable::Graphic, id);
    if (auto go = dynamic_cast<VMGameObject*>(obj)) go->RefreshGraphic();
}

void UIWallPlacer::MouseDown(LotControlState& state) {
    if (!Drawing) {
        PlayUISound(UISounds::BuildDragToolDown); // FIX(sims) SND
        Drawing = true;
        auto tilePos = Session->World()->State.WorldSpace.GetTileAtPosWithScroll(st::Vector2((float)state.MouseX, (float)state.MouseY));
        StartPosition = st::Point((int)std::round(tilePos.X), (int)std::round(tilePos.Y));
    }
}

void UIWallPlacer::MouseUp(LotControlState& state) {
    LastPlaced = false;
    if (Drawing) {
        auto World = Session->World();
        std::vector<VMArchitectureCommand> cmds;
        if (state.ShiftDown) {
            if (StartPosition != EndPosition) {
                int smallX = std::min(StartPosition.X, EndPosition.X);
                int smallY = std::min(StartPosition.Y, EndPosition.Y);
                int bigX = std::max(StartPosition.X, EndPosition.X);
                int bigY = std::max(StartPosition.Y, EndPosition.Y);
                VMArchitectureCommand c;
                c.Type = VMArchitectureCommandType::WALL_RECT;
                c.level = World->State.Level(); c.pattern = Pattern; c.style = Style;
                c.x = smallX; c.y = smallY; c.x2 = bigX - smallX; c.y2 = bigY - smallY;
                cmds.push_back(c);
            }
        } else {
            if (DrawLength > 0) {
                VMArchitectureCommand c;
                c.Type = state.CtrlDown ? VMArchitectureCommandType::WALL_DELETE : VMArchitectureCommandType::WALL_LINE;
                c.level = World->State.Level(); c.pattern = Pattern; c.style = Style;
                c.x = StartPosition.X; c.y = StartPosition.Y; c.x2 = DrawLength; c.y2 = DrawDir;
                cmds.push_back(c);
            }
        }
        auto arch = Session->Context()->Architecture();
        arch->Commands.clear();
        if (!cmds.empty() && arch->LastTestCost <= Session->Budget()) {
            int cost = 0;
            LastPlaced = Session->RunArchitecture(cmds, cost); // vm.SendCommand(new VMNetArchitectureCmd { Commands })
            LastCost = cost;
            PlayUISound(UISounds::BuildDragToolPlace); // FIX(sims) SND
        } else PlayUISound(UISounds::BuildDragToolUp); // FIX(sims) SND
        arch->SignalRedraw();
    }
    Drawing = false;
}

void UIWallPlacer::Update(LotControlState& state, bool scrolled) {
    auto World = Session->World();
    auto arch = Session->Context()->Architecture();
    auto tilePos = World->State.WorldSpace.GetTileAtPosWithScroll(st::Vector2((float)state.MouseX, (float)state.MouseY));
    st::Point cursor((int)std::round(tilePos.X), (int)std::round(tilePos.Y));

    auto& cmds = arch->Commands;
    cmds.clear();
    if (Drawing) {
        auto diff = cursor - StartPosition;
        DrawLength = (int)std::round(std::sqrt((double)(diff.X * diff.X + diff.Y * diff.Y)));
        DrawDir = (int)PosMod(std::round(std::atan2((double)diff.Y, (double)diff.X) / (M_PI / 4)), 8);

        VMArchitectureCommand c;
        c.level = World->State.Level(); c.pattern = DrawPattern; c.style = DrawStyle;
        if (state.ShiftDown) {
            EndPosition = cursor;
            int smallX = std::min(StartPosition.X, EndPosition.X);
            int smallY = std::min(StartPosition.Y, EndPosition.Y);
            int bigX = std::max(StartPosition.X, EndPosition.X);
            int bigY = std::max(StartPosition.Y, EndPosition.Y);
            c.Type = VMArchitectureCommandType::WALL_RECT;
            c.x = smallX; c.y = smallY; c.x2 = bigX - smallX; c.y2 = bigY - smallY;
        } else {
            cursor = StartPosition + st::Point(DirUnits[DrawDir].X * DrawLength, DirUnits[DrawDir].Y * DrawLength);
            c.Type = state.CtrlDown ? VMArchitectureCommandType::WALL_DELETE : VMArchitectureCommandType::WALL_LINE;
            c.x = StartPosition.X; c.y = StartPosition.Y; c.x2 = DrawLength; c.y2 = DrawDir;
        }
        cmds.push_back(c);
    }

    state.Tooltip.clear();
    bool wasDisallowed = _WasDisallowed; // FIX(sims) SND
    _WasDisallowed = false;
    if (!cmds.empty()) {
        if (!WasDown || !SameArchitectureCommand(cmds[0], LastCmd)) {
            arch->SignalRedraw();
            WasDown = true;
        }
        auto cost = arch->LastTestCost;
        if (cost != 0) {
            bool disallowed = cost > Session->Budget();
            state.TooltipError = disallowed;
            // FIX(sims) SND: C# plays Error when the command changes onto an unaffordable one (+ when the cost
            // turns unaffordable: the test cost lags the command by one frame here)
            if (disallowed && (!SameArchitectureCommand(cmds[0], LastCmd) || !wasDisallowed)) PlayUISound(UISounds::Error);
            _WasDisallowed = disallowed;
            state.Tooltip = (cost < 0) ? ("-" + UIUCP::Money(-cost)) : UIUCP::Money(cost);
        }
        LastCmd = cmds[0];
    } else {
        if (WasDown) {
            arch->SignalRedraw();
            WasDown = false;
        }
    }

    if (WallCursor != nullptr)
        WallCursor->SetVisualPosition(st::Vector3((float)cursor.X, (float)cursor.Y, (World->State.Level() - 1) * 2.95f), Direction::NORTH, Session->VM()->Context);

    if (state.ShiftDown) SetCursorGraphic(3);
    else if (state.CtrlDown) SetCursorGraphic(1);
    else SetCursorGraphic(0);
}

void UIWallPlacer::Release() {
    auto arch = Session->Context()->Architecture();
    if (WallCursor != nullptr) WallCursor->Delete(Session->VM()->Context);
    WallCursor = nullptr;
    arch->Commands.clear();
    arch->SignalRedraw();
}

}
