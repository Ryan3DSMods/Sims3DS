/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims app — see UIMotiveDisplay.h (FreeSims UI/Controls/UIMotiveDisplay.cs)
#include "app/live/UIMotiveDisplay.h"
#include "app/Font.h"
#include <algorithm>
#include <cmath>

namespace simapp {

static const char* DefaultNames[8] = { "Hunger", "Comfort", "Hygiene", "Bladder", "Energy", "Fun", "Social", "Room" };
// ADDED(sims): the UI runs at 30 fps instead of 60, so half the C# 240-entry window covers the same time
static const size_t CHANGE_WINDOW = 120;

UIMotiveDisplay::UIMotiveDisplay() {
    for (int i = 0; i < 8; i++) MotiveNames[i] = DefaultNames[i];
}

void UIMotiveDisplay::Update() {
    for (int i = 0; i < 8; i++) {
        if (!FirstFrame) ChangeBuffer[i].push_back(MotiveValues[i] - OldMotives[i]);
        if (ChangeBuffer[i].size() > CHANGE_WINDOW) ChangeBuffer[i].pop_front();

        int sum = 0;
        for (auto c : ChangeBuffer[i]) sum += c;

        double diff = sum / 2.5;
        if (diff < 0) diff = std::floor(diff);
        else if (diff > 0) diff = std::ceil(diff);
        TargetArrowStates[i] = std::max(std::min((int)diff, 5), -5) * 60;

        OldMotives[i] = MotiveValues[i];
    }
    FirstFrame = false;

    for (int i = 0; i < 8; i++) {
        // ADDED(sims): 2 steps per frame (30 fps)
        for (int k = 0; k < 2; k++) {
            if (TargetArrowStates[i] > ArrowStates[i]) ArrowStates[i]++;
            else if (TargetArrowStates[i] < ArrowStates[i]) ArrowStates[i]--;
        }
    }
}

static inline uint32_t Col(double r, double g, double b, int a = 255) { return RGBA((int)r, (int)g, (int)b, a); }

void UIMotiveDisplay::DrawMotive(Canvas& c, int x, int y, int w, int h, int motive, bool labelAbove) {
    double p = (MotiveValues[motive] + 100) / 200.0;
    p = std::max(0.0, std::min(1.0, p));
    uint32_t barcol = Col(57 * (1 - p), 213 * p + 97 * (1 - p), 49 * p + 90 * (1 - p));
    uint32_t bgcol = Col(57 * p + 214 * (1 - p), 97 * p, 90 * p);

    int labelW = labelAbove ? 0 : std::min(58, w / 3 + 8);
    int bx = x + labelW, bw = w - labelW - 14;
    int by = labelAbove ? y + 14 : y + (h - 7) / 2, bh = labelAbove ? std::max(5, std::min(10, h - 18)) : 7;
    c.FillRect(bx, by, bw, bh, bgcol);
    c.FillRect(bx, by, (int)(bw * p), bh, barcol);
    c.FillRect(bx + (int)(bw * p), by, 1, bh, RGBA(0, 0, 0));
    c.FillRect(bx, by, bw, 1, RGBA(255, 255, 255, 50));

    // label (shadowed, like the C# style with a black copy offset by 1)
    std::string name = Ellipsize(FontId::Small, MotiveNames[motive], labelAbove ? bw : labelW - 2);
    if (labelAbove) {
        int tw = TextWidth(FontId::Small, name);
        DrawTextShadow(c, FontId::Small, bx + (bw - tw) / 2, y, name, RGBA(255, 255, 255), RGBA(0, 0, 0));
    } else {
        DrawTextShadow(c, FontId::Small, x, y + (h - LineHeight(FontId::Small)) / 2, name, RGBA(255, 255, 255), RGBA(0, 0, 0));
    }

    // change arrows: green to the right of the bar when rising, red to the left when falling
    int arrowState = ArrowStates[motive];
    int ay = by + bh / 2;
    if (arrowState > 0) {
        for (int i = 0; i < (int)std::ceil(arrowState / 60.0); i++) {
            int a = (int)(255 * std::min(1.0, arrowState / 60.0 - i));
            int ax = bx + bw + 2 + i * 4;
            if (ax + 3 > x + w) break;
            c.FillTriangle(ax, ay - 3, ax, ay + 3, ax + 3, ay, RGBA(0x00, 0xCB, 0x39, a));
        }
    } else if (arrowState < 0) {
        arrowState = -arrowState;
        for (int i = 0; i < (int)std::ceil(arrowState / 60.0); i++) {
            int a = (int)(255 * std::min(1.0, arrowState / 60.0 - i));
            int ax = bx - 4 - i * 4;
            if (labelAbove ? ax < x : ax < x + labelW - 12) break;
            c.FillTriangle(ax + 3, ay - 3, ax + 3, ay + 3, ax, ay, RGBA(0xD6, 0x00, 0x00, a));
        }
    }
}

void UIMotiveDisplay::Draw(Canvas& c, const Rect& r) {
    int colW = r.W / 2;
    int rowH = r.H / 4;
    bool labelAbove = rowH >= 28;
    for (int i = 0; i < 4; i++) {
        DrawMotive(c, r.X + 4, r.Y + rowH * i, colW - 8, rowH, i, labelAbove);          //left side
        DrawMotive(c, r.X + colW + 4, r.Y + rowH * i, colW - 8, rowH, i + 4, labelAbove); //right side
    }
}

}
