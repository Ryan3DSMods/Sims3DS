/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims app — see UIWallPainter.h (FreeSims UI/Panels/LotControls/UIWallPainter.cs)
#include "app/build/UIWallPainter.h"
#include "app/build/UIWallPlacer.h"
#include "app/LotSession.h"
#include "app/UISounds.h"
#include "app/ui/UIUCP.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"
#include "simantics/VMArchitecture.h"
#include "simantics/utils/VMArchitectureTools.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMGameObject.h"
#include "simantics/entities/VMMultitileGroup.h"
#include "simantics/model/VMStackObjectVariable.h"
#include "world/World.h"
#include "world/components/ObjectComponent.h"
#include <algorithm>

namespace simapp {

using namespace FSO::SimAntics;
using FSO::SimAntics::Model::VMArchitectureCommand;
using FSO::SimAntics::Model::VMArchitectureCommandType;
using FSO::LotView::Model::LotTilePos;
using FSO::LotView::Model::Direction;
using FSO::LotView::WorldRotation;

UIWallPainter::UIWallPainter(LotSession* session, const std::vector<int>& parameters) : Session(session) {
    Pattern = (uint16_t)parameters[0];
    auto vm = Session->VM();
    WallCursor = vm->Context->CreateObjectInstance(0x00000439, LotTilePos::OUT_OF_WORLD, Direction::NORTH, true, true);
    if (WallCursor != nullptr && !WallCursor->Objects.empty()) {
        if (auto oc = dynamic_cast<FSO::LotView::Components::ObjectComponent*>(WallCursor->Objects[0]->WorldUI.get())) oc->ForceDynamic = true;
    }
    SetCursorGraphic(2);
}

void UIWallPainter::SetCursorGraphic(int16_t id) {
    if (WallCursor == nullptr || WallCursor->Objects.empty()) return;
    auto obj = WallCursor->Objects[0];
    obj->SetValue(Model::VMStackObjectVariable::Graphic, id);
    if (auto go = dynamic_cast<VMGameObject*>(obj)) go->RefreshGraphic();
}

void UIWallPainter::MouseDown(LotControlState& state) {
    PlayUISound(UISounds::BuildDragToolDown); // FIX(sims) SND
    Drawing = true;
}

void UIWallPainter::MouseUp(LotControlState& state) {
    // vm.SendCommand(new VMNetArchitectureCmd { Commands })
    auto arch = Session->Context()->Architecture();
    arch->Commands.clear();
    int cost = 0;
    LastPlaced = !Commands.empty() && Session->RunArchitecture(Commands, cost);
    LastCost = cost;
    PlayUISound(UISounds::BuildDragToolUp); // FIX(sims) SND: C# MouseUp
    Commands.clear();
    arch->SignalRedraw();
    Drawing = false;
}

void UIWallPainter::Release() {
    auto arch = Session->Context()->Architecture();
    if (WallCursor != nullptr) WallCursor->Delete(Session->VM()->Context);
    WallCursor = nullptr;
    arch->Commands.clear();
    arch->SignalRedraw();
}

void UIWallPainter::Update(LotControlState& state, bool scrolled) {
    auto World = Session->World();
    auto arch = Session->Context()->Architecture();
    uint16_t pattern = state.CtrlDown ? (uint16_t)0 : Pattern;

    auto tilePos = World->State.WorldSpace.GetTileAtPosWithScroll(st::Vector2((float)state.MouseX, (float)state.MouseY));
    st::Point cursor((int)tilePos.X, (int)tilePos.Y);

    if (!Drawing && !Commands.empty()) {
        arch->SignalRedraw();
        Commands.clear();
    }
    if (state.ShiftDown) {
        if (Commands.empty() || Commands[0].Type != VMArchitectureCommandType::PATTERN_FILL) {
            Commands.clear();
            arch->SignalRedraw();
            VMArchitectureCommand c;
            c.Type = VMArchitectureCommandType::PATTERN_FILL;
            c.level = World->State.Level(); c.pattern = pattern; c.style = 0;
            c.x = cursor.X; c.y = cursor.Y;
            Commands.push_back(c);
        }
    } else {
        if (!Commands.empty() && Commands[0].Type == VMArchitectureCommandType::PATTERN_FILL) {
            Commands.clear();
            arch->SignalRedraw();
        }
        int dir = 0;
        int altdir = 0;
        st::Vector2 fract(tilePos.X - cursor.X, tilePos.Y - cursor.Y);
        switch (World->State.Rotation()) {
            case WorldRotation::BottomRight:
                if (fract.X - fract.Y > 0) { dir = 2; altdir = 3; }
                else { dir = 3; altdir = 2; }
                break;
            case WorldRotation::TopRight:
                if (fract.X + fract.Y > 1) { dir = 3; altdir = 0; }
                else { dir = 0; altdir = 3; }
                break;
            case WorldRotation::TopLeft:
                //+x is right down. +y is left down
                if (fract.X - fract.Y > 0) { dir = 1; altdir = 0; }
                else { dir = 0; altdir = 1; }
                break;
            case WorldRotation::BottomLeft:
                if (fract.X + fract.Y > 1) { dir = 2; altdir = 1; }
                else { dir = 1; altdir = 2; }
                break;
        }

        auto finalDir = Utils::VMArchitectureTools::GetPatternDirection(arch, cursor, pattern, dir, altdir, World->State.Level());
        if (finalDir != -1) {
            CursorDir = finalDir;
            VMArchitectureCommand cmd;
            cmd.Type = VMArchitectureCommandType::PATTERN_DOT;
            cmd.level = World->State.Level(); cmd.pattern = pattern; cmd.style = 0;
            cmd.x = cursor.X; cmd.y = cursor.Y; cmd.x2 = dir; cmd.y2 = altdir;
            bool contains = false;
            for (auto& c : Commands) if (SameArchitectureCommand(c, cmd)) { contains = true; break; }
            if (!contains) {
                arch->SignalRedraw();
                // ADDED(sims): while not drawing only the hovered side is previewed (C# kept one too:
                // the list is cleared above whenever the mouse is up)
                if (!Drawing) Commands.clear();
                Commands.push_back(cmd);
            }
        }
    }

    auto& cmds = arch->Commands;
    cmds.clear();
    for (auto& cmd : Commands) cmds.push_back(cmd);

    state.Tooltip.clear();
    if (!cmds.empty()) {
        auto cost = arch->LastTestCost;
        if (cost != 0) {
            state.TooltipError = cost > Session->Budget();
            // FIX(sims) SND: C# plays Error every frame while unaffordable; once when it becomes so
            if (state.TooltipError && !_WasDisallowed) PlayUISound(UISounds::Error);
            state.Tooltip = (cost < 0) ? ("-" + UIUCP::Money(-cost)) : UIUCP::Money(cost);
        }
    }
    _WasDisallowed = !state.Tooltip.empty() && state.TooltipError; // FIX(sims) SND

    if (WallCursor != nullptr)
        WallCursor->SetVisualPosition(st::Vector3(cursor.X + 0.5f, cursor.Y + 0.5f, (World->State.Level() - 1) * 2.95f), (Direction)(1 << ((3 - CursorDir) * 2)), Session->VM()->Context);
}

}
