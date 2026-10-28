/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims app — FreeSims UI/Controls/UIMotiveDisplay.cs: the eight need bars (TS1 order: Hunger, Comfort,
// Hygiene, Bladder | Energy, Fun, Social, Room), coloured from red (-100) to green (+100) with the bar colour
// and background formulas of the C# control, and the little arrows that show a need rising (green, right) or
// falling (red, left) from the change over the last 240 updates.
// ADDED(sims): Draw() takes a rect (the 3DS panel is 272x51 compact or 272x206 expanded); the labels are
// Live.iff STR# 130 ("Motives") instead of the hard-coded C# English names.
#pragma once
#include <cstdint>
#include <deque>
#include <string>
#include "app/Canvas.h"

namespace simapp {

class UIMotiveDisplay {
public:
    UIMotiveDisplay();
    int16_t MotiveValues[8] = { 0 };
    std::string MotiveNames[8];
    /// Once per frame after the VM ran (C# Update).
    void Update();
    void Draw(Canvas& c, const Rect& r);
    void Reset() { FirstFrame = true; for (auto& q : ChangeBuffer) q.clear(); for (int i = 0; i < 8; i++) ArrowStates[i] = TargetArrowStates[i] = 0; }

private:
    int OldMotives[8] = { 0 };
    std::deque<int> ChangeBuffer[8];
    int ArrowStates[8] = { 0 };
    int TargetArrowStates[8] = { 0 };
    bool FirstFrame = true;
    void DrawMotive(Canvas& c, int x, int y, int w, int h, int motive, bool labelAbove);
};

}
