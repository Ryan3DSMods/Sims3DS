/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// ADDED(sims): see SaveOverlay.h.
#include "app/front/SaveOverlay.h"
#include "app/front/FrontUI.h"
#include "app/front/TS1Font.h"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace simapp { namespace front {

namespace {
const uint32_t kGold = RGBA(255, 216, 96), kWhite = RGBA(255, 255, 255), kShadow = RGBA(0, 0, 30, 220);
// the touch screen's panel and what is redrawn inside it
const Rect kPanel(20, 64, 280, 112);
const Rect kTitle(kPanel.X, kPanel.Y + 12, kPanel.W, 18);
const Rect kStep(kPanel.X, kPanel.Y + 38, kPanel.W, 16);
const Rect kBar(kPanel.X + 20, kPanel.Y + 64, kPanel.W - 40, 12);

/// The front end's dialog frame (FrontDialog::Draw): the dialog tiler twice (outer frame + inner panel) and the
/// blue rim; a plain rounded panel without UIGraphics.
void Panel(Canvas& c, const Rect& p) {
    auto& art = FrontArt::Get();
    if (art.DialogTiler.Width > 0) {
        NineSlice(c, art.DialogTiler, p, 14);
        NineSlice(c, art.DialogTiler, Rect(p.X + 4, p.Y + 4, p.W - 8, p.H - 8), 14);
    } else c.FillRoundRect(p, 10, RGBA(0, 10, 80, 235));
    c.StrokeRoundRect(p.X + 2, p.Y + 2, p.W - 4, p.H - 4, 10, RGBA(90, 120, 210, 200));
}

void CopyInto(Canvas& dst, const Canvas& src) {
    if (src.Width == dst.Width && src.Height == dst.Height && !src.Px.empty()) std::memcpy(dst.Px.data(), src.Px.data(), dst.Px.size() * sizeof(uint32_t));
    else dst.Clear(RGBA(0, 0, 0));
}
}

void SaveOverlay::Begin(const Canvas& top, const Canvas& bottom, const std::string& lotName) {
    _Top = top;
    _Bottom = bottom;
    _Top.Dim(0, 0, _Top.Width, _Top.Height, 110);
    _Bottom.Dim(0, 0, _Bottom.Width, _Bottom.Height, 110);
    // top screen: the lot (dimmed) with "Saving..." and the lot's name
    bool sub = !lotName.empty();
    Rect p((_Top.Width - 240) / 2, (_Top.Height - (sub ? 66 : 48)) / 2, 240, sub ? 66 : 48);
    Panel(_Top, p);
    TextCentered(_Top, TS1Size::S16, Rect(p.X, p.Y + 10, p.W, 24), "Saving...", kGold, kShadow);
    if (sub) TextCentered(_Top, TS1Size::S10, Rect(p.X, p.Y + 38, p.W, 16), Fit(TS1Size::S10, lotName, p.W - 24), kWhite, kShadow);
    // touch screen: the panel; the bar's frame and the note under it do not change either
    Panel(_Bottom, kPanel);
    TextCentered(_Bottom, TS1Size::S9, Rect(kPanel.X, kPanel.Y + 84, kPanel.W, 14), "Please do not turn off the power.", RGBA(200, 210, 240), RGBA(0, 0, 30, 160));
}

void SaveOverlay::Draw(Canvas& top, Canvas& bottom, const std::string& step, float fraction) const {
    fraction = std::min(1.0f, std::max(0.0f, fraction));
    int pct = (int)(fraction * 100.0f);
    if (fraction >= 1.0f) pct = 100; // 100 % only when it is done
    CopyInto(top, _Top);
    CopyInto(bottom, _Bottom);
    char title[64];
    std::snprintf(title, sizeof title, "Saving house: %d%%", pct);
    TextCentered(bottom, TS1Size::S12, kTitle, title, kGold, kShadow);
    TextCentered(bottom, TS1Size::S10, kStep, Fit(TS1Size::S10, step, kStep.W - 24), kWhite, RGBA(0, 0, 30, 160));
    // the bar of the loading screens (LoadingScreen::Draw)
    bottom.FillRoundRect(kBar, 4, RGBA(4, 10, 30));
    int pw = (int)((kBar.W - 4) * fraction);
    if (pw > 0) bottom.GradientV(kBar.X + 2, kBar.Y + 2, pw, kBar.H - 4, RGBA(120, 230, 255), RGBA(30, 110, 200));
    bottom.StrokeRoundRect(kBar.X, kBar.Y, kBar.W, kBar.H, 4, RGBA(90, 130, 210));
}

void SaveOverlay::End() {
    _Top = Canvas();
    _Bottom = Canvas();
}

}}
