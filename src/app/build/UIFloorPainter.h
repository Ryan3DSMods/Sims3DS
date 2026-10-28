/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims app — FreeSims UI/Panels/LotControls/UIFloorPainter.cs: the floor tool. Press and drag a rectangle
// of tiles (FLOOR_RECT, the half-tile triangle chosen from the cursor's position in the tile), Shift fills the
// room (FLOOR_FILL), Ctrl paints pattern 0 (removes the floor). Previewed through VMArchitecture.Commands.
#pragma once
#include <cstdint>
#include <vector>
#include "app/build/UICustomLotControl.h"
#include "simantics/model/VMArchitectureCommand.h"

namespace simapp {

class LotSession;

class UIFloorPainter : public UICustomLotControl {
public:
    UIFloorPainter(LotSession* session, const std::vector<int>& parameters);
    void MouseDown(LotControlState& state) override;
    void MouseUp(LotControlState& state) override;
    void Update(LotControlState& state, bool scrolled) override;
    void Release() override;
    int LastCost = 0;
    bool LastPlaced = false;

private:
    LotSession* Session;
    bool Drawing = false;
    bool _WasDisallowed = false; // FIX(sims) SND
    std::vector<FSO::SimAntics::Model::VMArchitectureCommand> Commands;
    int CursorDir = 0;
    int StartX = -1;
    int StartY = -1;
    uint16_t Pattern;
    bool PoolMode = false; // TS1(sims): pool / water tool (pattern 65535 / 65534): no room fill, erase only removes pool tiles
};

}
