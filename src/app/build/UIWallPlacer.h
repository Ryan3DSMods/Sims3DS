/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims app — FreeSims UI/Panels/LotControls/UIWallPlacer.cs: the wall & fence tool. Press, drag along one
// of the 8 directions, release: a WALL_LINE (or WALL_DELETE with Ctrl, WALL_RECT with Shift) architecture
// command. While dragging, the command is simulated (VMArchitecture.Commands) so the walls preview on the lot and
// the cost shows as a tooltip. The cursor is the game's "Wall Cursor" object (GUID 0x439, graphic 0 wall,
// 1 bulldoze, 2 paint, 3 rect).
#pragma once
#include <cstdint>
#include <vector>
#include "core/math.h"
#include "app/build/UICustomLotControl.h"
#include "simantics/model/VMArchitectureCommand.h"

namespace FSO { namespace SimAntics { namespace Entities { class VMMultitileGroup; } } }

namespace simapp {

class LotSession;

class UIWallPlacer : public UICustomLotControl {
public:
    UIWallPlacer(LotSession* session, const std::vector<int>& parameters);
    void SetCursorGraphic(int16_t id);
    void MouseDown(LotControlState& state) override;
    void MouseUp(LotControlState& state) override;
    void Update(LotControlState& state, bool scrolled) override;
    void Release() override;
    int LastCost = 0;          // ADDED(sims): cost of the last placed command (money pop-up)
    bool LastPlaced = false;

private:
    FSO::SimAntics::Entities::VMMultitileGroup* WallCursor = nullptr;
    LotSession* Session;
    bool Drawing = false;
    st::Point StartPosition;
    int DrawDir = 0;
    int DrawLength = 0;
    st::Point EndPosition;
    uint16_t DrawPattern, DrawStyle, Pattern, Style;
    FSO::SimAntics::Model::VMArchitectureCommand LastCmd;
    bool WasDown = false;
    bool _WasDisallowed = false; // FIX(sims) SND
    static const st::Point DirUnits[8];
};

/// C# VMArchitectureCommand.Equals (struct value equality)
bool SameArchitectureCommand(const FSO::SimAntics::Model::VMArchitectureCommand& a, const FSO::SimAntics::Model::VMArchitectureCommand& b);

}
