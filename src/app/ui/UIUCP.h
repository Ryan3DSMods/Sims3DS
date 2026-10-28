/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims app — FreeSims UI/Panels/UIUCP.cs ("user control panel"): the always-present part of the control
// panel — the mode buttons (Live / Buy / Build / Options), the funds and the clock — plus TS1's mood meter.
// ADDED(sims): laid out as a 48 px column on the left of the bottom screen, with the original art
// (UIGraphics cpanel\Buttons\people.bmp, objects.bmp, arch.bmp, options.bmp; cpanel\Greenbars.BMP, Redbars.bmp).
#pragma once
#include "app/Canvas.h"

namespace simapp {

class LotScreen;

class UIUCP {
public:
    static void Draw(LotScreen& L, Canvas& c, const Rect& r);
    /// "§1,234" (Latin-1 section sign = Simoleons)
    static std::string Money(int v);
    /// "7:05 AM" from the VM clock (UIText STR# 138 AM/PM)
    static std::string ClockText(LotScreen& L);
};

}
