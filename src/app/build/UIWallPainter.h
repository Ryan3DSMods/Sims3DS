/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims app — FreeSims UI/Panels/LotControls/UIWallPainter.cs: the wallpaper tool. Hovering a wall side
// previews the pattern on it (PATTERN_DOT; the side is picked from where in the tile the cursor is, per view
// rotation); dragging paints along; Shift fills the whole room (PATTERN_FILL); Ctrl paints pattern 0 (removes).
// The cursor is the Wall Cursor object with graphic 2 (paint).
#pragma once
#include <cstdint>
#include <vector>
#include "app/build/UICustomLotControl.h"
#include "simantics/model/VMArchitectureCommand.h"

namespace FSO { namespace SimAntics { namespace Entities { class VMMultitileGroup; } } }

namespace simapp {

class LotSession;

class UIWallPainter : public UICustomLotControl {
public:
    UIWallPainter(LotSession* session, const std::vector<int>& parameters);
    void SetCursorGraphic(int16_t id);
    void MouseDown(LotControlState& state) override;
    void MouseUp(LotControlState& state) override;
    void Update(LotControlState& state, bool scrolled) override;
    void Release() override;
    int LastCost = 0;
    bool LastPlaced = false;

private:
    FSO::SimAntics::Entities::VMMultitileGroup* WallCursor = nullptr;
    LotSession* Session;
    bool Drawing = false;
    bool _WasDisallowed = false; // FIX(sims) SND
    std::vector<FSO::SimAntics::Model::VMArchitectureCommand> Commands;
    int CursorDir = 0;
    uint16_t Pattern;
};

}
