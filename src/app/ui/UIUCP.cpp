/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims app — see UIUCP.h
#include "app/ui/UIUCP.h"
#include "app/LotScreen.h"
#include "app/Font.h"
#include "app/Theme.h"
#include "app/ui/UIGraphics.h"
#include "app/ui/GameText.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"
#include "simantics/VMClock.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/model/VMMotive.h"
#include <algorithm>
#include <cstdio>

namespace simapp {

std::string UIUCP::Money(int v) {
    char buf[32];
    std::string digits = std::to_string(v < 0 ? -v : v), out;
    int c = 0;
    for (int i = (int)digits.size() - 1; i >= 0; i--) {
        out.insert(out.begin(), digits[(size_t)i]);
        if (++c % 3 == 0 && i > 0) out.insert(out.begin(), ',');
    }
    snprintf(buf, sizeof buf, "%s\xA7%s", v < 0 ? "-" : "", out.c_str());
    return buf;
}

std::string UIUCP::ClockText(LotScreen& L) {
    auto clock = L.S->VM()->Context->Clock.get();
    int h = clock->Hours % 12; if (h == 0) h = 12;
    char tbuf[48];
    auto& gt = GameText::Get();
    snprintf(tbuf, sizeof tbuf, "%d:%02d %s", h, clock->Minutes, clock->Hours < 12 ? gt.UI(138, 21, "AM").c_str() : gt.UI(138, 22, "PM").c_str());
    return tbuf;
}

void UIUCP::Draw(LotScreen& L, Canvas& c, const Rect& r) {
    // panel background (cpanel\Backgrounds\PanelBack.bmp is the TS1 control panel strip: its left end)
    if (!UIGraphics::Get().DrawImage(c, "cpanel/Backgrounds/PanelBack.bmp", r, Rect(0, 0, 60, 100)))
        c.GradientV(r.X, r.Y, r.W, r.H, RGBA(30, 40, 120), RGBA(8, 12, 70));
    c.FillRect(r.Right() - 1, r.Y, 1, r.H, RGBA(120, 130, 210));
    auto& gt = GameText::Get();
    typedef LotScreen::Mode M;
    // mode buttons (TS1: big people button, then objects, house frame, options)
    L.DrawArtButton(c, Rect(r.X + 2, r.Y + 2, 44, 44), "cpanel/Buttons/people.bmp", 4, L.CurrentMode == M::Live, false, gt.UI(138, 10, "Live"), "mode_live");
    L.DrawArtButton(c, Rect(r.X + 3, r.Y + 48, 42, 42), "cpanel/Buttons/objects.bmp", 4, L.CurrentMode == M::Buy, false, gt.UI(138, 11, "Buy"), "mode_buy");
    L.DrawArtButton(c, Rect(r.X + 5, r.Y + 92, 38, 38), "cpanel/Buttons/arch.bmp", 4, L.CurrentMode == M::Build, false, gt.UI(138, 12, "Build"), "mode_build");
    L.DrawArtButton(c, Rect(r.X + 10, r.Y + 132, 28, 26), "cpanel/Buttons/options.bmp", 4, L.CurrentMode == M::Options, false, "Opt", "mode_options");

    // funds and time
    int budget = L.S->Budget();
    std::string m = Money(budget);
    FontId mf = TextWidth(FontId::Bold, m) <= r.W - 4 ? FontId::Bold : FontId::Small;
    DrawTextCentered(c, mf, Rect(r.X, r.Y + 161, r.W, 13), Ellipsize(mf, m, r.W - 2), RGBA(120, 255, 120));
    L.Hit(Rect(r.X, r.Y + 160, r.W, 14), "funds");
    DrawTextCentered(c, FontId::Small, Rect(r.X, r.Y + 175, r.W, 13), ClockText(L), RGBA(255, 255, 255));
    L.Hit(Rect(r.X, r.Y + 174, r.W, 14), "clock");

    // TS1 mood meter: green bars rise above the line for a good mood, red ones fall below it for a bad one
    Rect meter(r.X + 10, r.Y + 191, 27, 46);
    c.FillRect(meter, RGBA(0, 0, 30));
    c.FillRect(meter.X - 2, meter.Y + 22, meter.W + 4, 1, RGBA(140, 150, 230));
    auto sim = L.S->SelectedSim();
    if (sim != nullptr) {
        int mood = std::max(-100, std::min(100, (int)sim->GetMotiveData(FSO::SimAntics::Model::VMMotive::Mood)));
        if (mood > 0) {
            int h = std::max(1, mood * 22 / 100);
            if (!UIGraphics::Get().DrawImage(c, "cpanel/Greenbars.BMP", Rect(meter.X, meter.Y + 22 - h, 27, h), Rect(0, 25 - (h * 25 + 21) / 22, 27, (h * 25 + 21) / 22)))
                for (int k = 0; k < h; k += 4) c.FillRect(meter.X + 2, meter.Y + 21 - k - 2, 23, 3, RGBA(40, 220, 60));
        } else if (mood < 0) {
            int h = std::max(1, -mood * 22 / 100);
            if (!UIGraphics::Get().DrawImage(c, "cpanel/Redbars.bmp", Rect(meter.X, meter.Y + 24, 27, h), Rect(0, 0, 27, (h * 25 + 21) / 22)))
                for (int k = 0; k < h; k += 4) c.FillRect(meter.X + 2, meter.Y + 24 + k, 23, 3, RGBA(230, 40, 40));
        }
    }
}

}
