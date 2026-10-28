/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims app — see UITerrainTools.h (FreeSO UITerrainRaiser / UITerrainFlatten / UIGrassPaint)
#include "app/build/UITerrainTools.h"
#include "app/build/UIWallPlacer.h"
#include "app/LotSession.h"
#include "app/UISounds.h"
#include "app/ui/UIUCP.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"
#include "simantics/VMArchitecture.h"
#include "world/World.h"
#include <algorithm>
#include <cmath>

namespace simapp {

using namespace FSO::SimAntics;
using FSO::SimAntics::Model::VMArchitectureCommand;
using FSO::SimAntics::Model::VMArchitectureCommandType;

UITerrainTool::UITerrainTool(LotSession* session, Mode mode) : ToolMode(mode), Session(session) {}

void UITerrainTool::Cursor(const LotControlState& state, int& x, int& y, float& fx, float& fy) {
    auto tilePos = Session->World()->State.WorldSpace.GetTileAtPosWithScroll(st::Vector2((float)state.MouseX, (float)state.MouseY));
    fx = tilePos.X; fy = tilePos.Y;
    // FreeSO: the nearest vertex (Math.Round)
    x = (int)std::lround(tilePos.X);
    y = (int)std::lround(tilePos.Y);
}

void UITerrainTool::MouseDown(LotControlState& state) {
    if (Drawing) return;
    int x, y; float fx, fy;
    Cursor(state, x, y, fx, fy);
    auto arch = Session->Context()->Architecture();
    auto& terrain = *arch->Terrain;
    if (ToolMode != Mode::Grass && (y >= terrain.Height || x >= terrain.Width || x < 0 || y < 0)) return;
    Drawing = true;
    StartX = x; StartY = y;
    HeldFrames = 0;
    _SoundSteps = 0;
    // FIX(sims) SND: FreeSO UIGrassPaint: BuildDragToolDown; TS1's level tool has its own mouse-down event (raise /
    // lower sound each step in Update: ui_terrain_raise / ui_terrain_lower)
    if (ToolMode == Mode::Grass) PlayUISound(UISounds::BuildDragToolDown);
    else if (ToolMode == Mode::Level) PlayUISound(UISounds::TerrainLevelMouseDown);
    if (ToolMode != Mode::Grass) StartTerrainHeight = terrain.Heights[(size_t)(y * terrain.Width + x)];
    GrassCommands.clear();
    LastCurX = LastCurY = -9999;
}

void UITerrainTool::MouseUp(LotControlState& state) {
    (void)state;
    auto arch = Session->Context()->Architecture();
    arch->Commands.clear();
    std::vector<VMArchitectureCommand> cmds = Commands;
    if (ToolMode == Mode::Grass) { cmds.clear(); for (auto& kv : GrassCommands) cmds.push_back(kv.second); }
    int cost = 0;
    arch->LastFailReason = 0;
    LastPlaced = Drawing && !cmds.empty() && Session->RunArchitecture(cmds, cost); // VMNetArchitectureCmd
    bool send = LastPlaced;
    // FIX(sims) SND: FreeSO UIGrassPaint / UITerrainFlatten: Place when sent, else Up; TS1 level: its mouse-up event
    if (Drawing) {
        if (ToolMode == Mode::Level) PlayUISound(UISounds::TerrainLevelMouseUp);
        else if (ToolMode == Mode::Grass) PlayUISound(send ? UISounds::BuildDragToolPlace : UISounds::BuildDragToolUp);
    }
    LastFailReason = arch->LastFailReason;
    LastCost = cost;
    Commands.clear();
    GrassCommands.clear();
    arch->SignalTerrainRedraw();
    arch->SignalRedraw();
    Drawing = false;
}

void UITerrainTool::Release() {
    auto arch = Session->Context()->Architecture();
    arch->Commands.clear();
    arch->SignalTerrainRedraw();
    arch->SignalRedraw();
}

void UITerrainTool::Update(LotControlState& state, bool scrolled) {
    (void)scrolled;
    auto arch = Session->Context()->Architecture();
    auto& terrain = *arch->Terrain;
    int x, y; float fx, fy;
    Cursor(state, x, y, fx, fy);
    bool changed = false;

    if (ToolMode == Mode::Grass) {
        // FreeSO UIGrassPaint: each tile the stylus crosses gets greener (Shift: browner)
        int tx = (int)std::floor(fx), ty = (int)std::floor(fy);
        MarkerX0 = tx; MarkerY0 = ty; MarkerX1 = tx + 1; MarkerY1 = ty + 1;
        if (Drawing && (tx != LastCurX || ty != LastCurY) && tx >= 0 && ty >= 0 && tx < terrain.Width && ty < terrain.Height) {
            auto key = std::make_pair(tx, ty);
            VMArchitectureCommand cmd;
            auto it = GrassCommands.find(key);
            if (it != GrassCommands.end()) cmd = it->second;
            else { cmd.Type = VMArchitectureCommandType::GRASS_DOT; cmd.x = tx; cmd.y = ty; cmd.pattern = 0; cmd.level = 1; }
            int mod = state.ShiftDown ? 128 : -128;
            cmd.pattern = (uint16_t)(int16_t)std::min(255, std::max(-255, (int)(int16_t)cmd.pattern + mod));
            GrassCommands[key] = cmd;
            LastCurX = tx; LastCurY = ty;
            changed = true;
        }
        auto& cmds = arch->Commands;
        cmds.clear();
        for (auto& kv : GrassCommands) cmds.push_back(kv.second);
    } else {
        int sx = Drawing ? StartX : x, sy = Drawing ? StartY : y;
        int smallX = std::min(sx, x), smallY = std::min(sy, y), bigX = std::max(sx, x), bigY = std::max(sy, y);
        if (ToolMode == Mode::Level && !Drawing) { smallX = bigX = x; smallY = bigY = y; }
        MarkerX0 = smallX; MarkerY0 = smallY; MarkerX1 = bigX; MarkerY1 = bigY;
        std::vector<VMArchitectureCommand> next;
        if (Drawing) {
            HeldFrames++;
            VMArchitectureCommand cmd;
            cmd.level = 1;
            if (ToolMode == Mode::Level) {
                // FreeSO UITerrainFlatten: the dragged rectangle takes the height of the vertex it started at
                if (smallX != bigX || smallY != bigY) {
                    cmd.Type = VMArchitectureCommandType::TERRAIN_FLATTEN;
                    cmd.x = smallX; cmd.y = smallY; cmd.x2 = bigX - smallX; cmd.y2 = bigY - smallY;
                    cmd.style = (uint16_t)StartTerrainHeight;
                    cmd.pattern = state.CtrlDown ? 1 : 0;
                    next.push_back(cmd);
                }
            } else {
                // TS1: one step per click, one more for every HoldFrames frames the stylus stays down
                int steps = std::min(20, 1 + HeldFrames / HoldFrames);
                bool lower = (ToolMode == Mode::Lower) != state.ShiftDown;
                // FIX(sims) SND: TS1 ui_terrain_raise / ui_terrain_lower (bank track ui_terrain_rand) for every step
                if (steps > _SoundSteps) { _SoundSteps = steps; PlayUISound(lower ? UISounds::TerrainLower : UISounds::TerrainRaise); }
                int16_t ref = lower ? INT16_MAX : INT16_MIN;
                for (int vy = smallY; vy <= bigY; vy++) for (int vx = smallX; vx <= bigX; vx++) {
                    if (vx < 0 || vy < 0 || vx >= terrain.Width || vy >= terrain.Height) continue;
                    auto h = terrain.Heights[(size_t)(vy * terrain.Width + vx)];
                    ref = lower ? std::min(ref, h) : std::max(ref, h);
                }
                if (ref != INT16_MAX && ref != INT16_MIN) {
                    int nh = ref + (lower ? -1 : 1) * steps * StepUnits * 10;
                    nh = std::max(0, std::min(255 * 10, nh)); // a TS1 altitude is one byte
                    cmd.Type = (smallX == bigX && smallY == bigY) ? VMArchitectureCommandType::TERRAIN_RAISE : VMArchitectureCommandType::TERRAIN_FLATTEN;
                    cmd.x = smallX; cmd.y = smallY; cmd.x2 = bigX - smallX; cmd.y2 = bigY - smallY;
                    cmd.style = (uint16_t)nh;
                    cmd.pattern = 0;
                    next.push_back(cmd);
                }
            }
        }
        if (next.size() != Commands.size() || (!next.empty() && !(next[0].Type == Commands[0].Type && next[0].x == Commands[0].x &&
            next[0].y == Commands[0].y && next[0].x2 == Commands[0].x2 && next[0].y2 == Commands[0].y2 && next[0].style == Commands[0].style))) {
            Commands = next;
            changed = true;
        }
        auto& cmds = arch->Commands;
        cmds.clear();
        for (auto& c : Commands) cmds.push_back(c);
    }
    if (changed) arch->SignalRedraw();

    state.Tooltip.clear();
    state.TooltipError = false;
    MarkerError = false;
    if (!arch->Commands.empty()) {
        auto cost = arch->LastTestCost;
        if (cost != 0) {
            state.TooltipError = cost > Session->Budget();
            state.Tooltip = (cost < 0) ? ("-" + UIUCP::Money(-cost)) : UIUCP::Money(cost);
        } else if (arch->LastFailReason != 0) MarkerError = true;
        if (state.TooltipError) MarkerError = true;
    }
    // FIX(sims) SND: FreeSO plays Error while the change is unaffordable / blocked (every frame); once when it becomes so
    bool disallowed = state.TooltipError || MarkerError;
    if (Drawing && disallowed && !_WasDisallowed) PlayUISound(UISounds::Error);
    _WasDisallowed = disallowed;
}

}
