/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims app — see UIFloorPainter.h (FreeSims UI/Panels/LotControls/UIFloorPainter.cs)
#include "app/build/UIFloorPainter.h"
#include "app/build/UIWallPlacer.h"
#include "app/LotSession.h"
#include "app/UISounds.h"
#include "app/ui/UIUCP.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"
#include "simantics/VMArchitecture.h"
#include "world/World.h"
#include <algorithm>
#include <cstdlib>
#include <cstdio>

namespace simapp {

using namespace FSO::SimAntics;
using FSO::SimAntics::Model::VMArchitectureCommand;
using FSO::SimAntics::Model::VMArchitectureCommandType;

UIFloorPainter::UIFloorPainter(LotSession* session, const std::vector<int>& parameters) : Session(session) {
    Pattern = (uint16_t)parameters[0];
    PoolMode = Pattern >= 65534; // TS1(sims)
}

void UIFloorPainter::MouseDown(LotControlState& state) {
    PlayUISound(UISounds::BuildDragToolDown); // FIX(sims) SND
    Drawing = true;
    // FIX(sims) QA2: the hover preview (cursor tile before the stylus touched) left a command, so the drag kept that tile
    // as its start: a 3x3 pool/floor drag made 1x1..2x2 (TEST_REPORT pool note). Start where the stylus goes down.
    Commands.clear();
}

void UIFloorPainter::MouseUp(LotControlState& state) {
    auto arch = Session->Context()->Architecture();
    arch->Commands.clear();
    int cost = 0;
    LastPlaced = !Commands.empty() && Session->RunArchitecture(Commands, cost); // VMNetArchitectureCmd
    LastCost = cost;
    PlayUISound(UISounds::BuildDragToolUp); // FIX(sims) SND: C# MouseUp
    Commands.clear();
    arch->SignalRedraw();
    Drawing = false;
}

void UIFloorPainter::Release() {
    auto arch = Session->Context()->Architecture();
    arch->Commands.clear();
    arch->SignalRedraw();
}

void UIFloorPainter::Update(LotControlState& state, bool scrolled) {
    auto World = Session->World();
    auto arch = Session->Context()->Architecture();
    uint16_t pattern = state.CtrlDown ? (uint16_t)0 : Pattern;

    auto tilePos = World->State.WorldSpace.GetTileAtPosWithScroll(st::Vector2((float)state.MouseX, (float)state.MouseY));
    st::Point cursor((int)tilePos.X, (int)tilePos.Y);

    if (!Drawing && !Commands.empty()) {
        arch->SignalRedraw();
        Commands.clear();
    }
    if (state.ShiftDown && pattern < 65534 && !PoolMode) {
        if (Commands.empty() || Commands[0].Type != VMArchitectureCommandType::FLOOR_FILL) {
            Commands.clear();
            arch->SignalRedraw();
            VMArchitectureCommand c;
            c.Type = VMArchitectureCommandType::FLOOR_FILL;
            c.level = World->State.Level(); c.pattern = pattern; c.style = 0;
            c.x = cursor.X; c.y = cursor.Y;
            Commands.push_back(c);
        }
    } else {
        if (!Commands.empty() && Commands[0].Type == VMArchitectureCommandType::FLOOR_FILL) {
            Commands.clear();
            arch->SignalRedraw();
        }

        if (!Drawing || Commands.empty()) {
            StartX = cursor.X;
            StartY = cursor.Y;
        }

        int dir = 0;
        st::Vector2 fract(tilePos.X - cursor.X, tilePos.Y - cursor.Y);
        if (fract.X - fract.Y > 0) dir = (fract.X + fract.Y > 1) ? 2 : 1;
        else dir = (fract.X + fract.Y > 1) ? 3 : 0;

        int smallX = std::min<int>(StartX, cursor.X);
        int smallY = std::min<int>(StartY, cursor.Y);
        int bigX = std::max<int>(StartX, cursor.X);
        int bigY = std::max<int>(StartY, cursor.Y);

        VMArchitectureCommand cmd;
        cmd.Type = VMArchitectureCommandType::FLOOR_RECT;
        cmd.level = World->State.Level(); cmd.pattern = pattern; cmd.style = (uint16_t)dir;
        cmd.x = smallX; cmd.y = smallY; cmd.x2 = bigX - smallX; cmd.y2 = bigY - smallY;
        if (PoolMode && pattern == 0) {
            // TS1(sims): erasing with the pool tool only digs out pool (water) tiles, one dot command per tile
            std::vector<VMArchitectureCommand> dots;
            for (int y = smallY; y <= bigY; y++) for (int x = smallX; x <= bigX; x++) {
                if (x < 0 || y < 0 || x >= arch->Width || y >= arch->Height) continue;
                if (arch->GetFloor((int16_t)x, (int16_t)y, 1).Pattern != Pattern) continue;
                VMArchitectureCommand d = cmd;
                d.x = x; d.y = y; d.x2 = 0; d.y2 = 0; d.level = 1;
                dots.push_back(d);
            }
            bool same = dots.size() == Commands.size();
            for (size_t i = 0; same && i < dots.size(); i++) same = SameArchitectureCommand(dots[i], Commands[i]);
            if (!same) { Commands = dots; arch->SignalRedraw(); }
        } else {
        if (PoolMode) cmd.level = 1; // pools are dug into the ground floor
        bool contains = false;
        for (auto& c : Commands) if (SameArchitectureCommand(c, cmd)) { contains = true; break; }
        if (!contains) {
            Commands.clear();
            arch->SignalRedraw();
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
}

}
