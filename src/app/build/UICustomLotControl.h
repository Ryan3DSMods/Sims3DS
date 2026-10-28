/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims app — FreeSims UI/Panels/LotControls/UICustomLotControl.cs: a build tool that takes over the mouse
// on the lot (UIWallPlacer, UIWallPainter, UIFloorPainter).
// ADDED(sims): C# passed UpdateState (mouse + keyboard); here the pointer is in top-screen pixels and the
// keyboard modifiers TS1 uses (Shift = rectangle / fill, Ctrl = erase) are toggles on the touch toolbar.
#pragma once
#include <cstdint>
#include <string>

namespace simapp {

struct LotControlState {
    int MouseX = 0, MouseY = 0;
    bool ShiftDown = false;   // C# Keys.LeftShift
    bool CtrlDown = false;    // C# Keys.LeftControl
    // output for the UI (C# set UIState.Tooltip)
    std::string Tooltip;
    bool TooltipError = false;
};

class UICustomLotControl {
public:
    virtual ~UICustomLotControl() {}
    virtual void MouseDown(LotControlState& state) = 0;
    virtual void MouseUp(LotControlState& state) = 0;
    virtual void Update(LotControlState& state, bool scrolled) = 0;
    virtual void Release() = 0;
};

}
