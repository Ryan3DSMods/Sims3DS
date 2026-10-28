/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims front end — see IntroScreen.h
#include "app/front/IntroScreen.h"
#include "app/front/FrontStrings.h"
#include "app/front/FrontUI.h"
#include "app/front/TS1Font.h"
#include "app/front/ResGraphics.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace simapp { namespace front {

// ======================================================================= intro video (TS1(sims))

IntroVideo::~IntroVideo() { if (_F) fclose((FILE*)_F); }

bool IntroVideo::Open(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    uint8_t h[12];
    if (fread(h, 1, 12, f) != 12 || h[0] != 'S' || h[1] != 'T' || h[2] != 'V' || h[3] != '1') { fclose(f); return false; }
    _W = h[4] | (h[5] << 8); _H = h[6] | (h[7] << 8);
    _Fps = (h[8] | (h[9] << 8)) / 100.0; if (_Fps <= 0) _Fps = 30;
    _Frames = h[10] | (h[11] << 8);
    if (_W <= 0 || _H <= 0 || _W > 400 || _H > 240) { fclose(f); return false; }
    _F = f;
    _Grey.assign((size_t)_W * _H, 0);
    _Index = -1; _T = 0;
    return true;
}

bool IntroVideo::ReadFrame() {
    FILE* f = (FILE*)_F;
    uint8_t sz[4];
    if (fread(sz, 1, 4, f) != 4) return false;
    uint32_t n = sz[0] | (sz[1] << 8) | (sz[2] << 16) | ((uint32_t)sz[3] << 24);
    std::vector<uint8_t> rle(n);
    if (n && fread(rle.data(), 1, n, f) != n) return false;
    size_t o = 0;
    for (size_t i = 0; i + 1 < rle.size() && o < _Grey.size(); i += 2)
        for (int k = 0; k < rle[i] && o < _Grey.size(); k++) _Grey[o++] = rle[i + 1];
    return true;
}

bool IntroVideo::Update(double dtMs) {
    if (!_F) return false;
    _T += dtMs;
    int want = (int)(_T * _Fps / 1000.0);
    while (_Index < want) {
        if (_Index + 1 >= _Frames || !ReadFrame()) return false;
        _Index++;
    }
    return true;
}

void IntroVideo::Draw(Canvas& top) const {
    top.Clear(RGBA(0, 0, 0));
    int ox = (top.Width - _W) / 2, oy = (top.Height - _H) / 2;
    for (int y = 0; y < _H; y++) for (int x = 0; x < _W; x++) {
        uint8_t g = _Grey[(size_t)y * _W + x];
        int dx = ox + x, dy = oy + y;
        if (dx >= 0 && dy >= 0 && dx < top.Width && dy < top.Height) top.Px[(size_t)dy * top.Width + dx] = RGBA(g, g, g);
    }
}

// ======================================================================= LoadingScreen (FreeSims LoadingScreen.cs)

void LoadingScreen::Init() {
    auto& ui = ResGraphics::Get();
    auto& S = FrontStrings::Get();
    /** Background image **/
    // C#: setupTex = GetTexture(UIFileIDs.setup); bgScale = 600f / setupTex.Height (the screen is scaled to 600 high)
    ui.LoadScaled("kSimsLogo", 0.4f, _Background);
    if (_Background.Width == 0) ui.LoadScaled("Other/setup.bmp", 0.4f, _Background);
    _Copyright = S.Get(154, 5, "\xA9 2000-2005 Electronic Arts Inc. All Rights Reserved.");
    _Brand = S.Get(154, 4, "Maxis is an Electronic Arts brand.");
    // C#: PreloadLabels = 155/6..9. TS1(sims): the game's full set of the funny ones, in the game's order.
    _PreloadLabels.clear();
    for (int i : { 4, 5, 6, 7, 8, 9, 10, 11, 14 }) _PreloadLabels.push_back(S.Get(155, i, ""));
    _PreloadLabels.erase(std::remove(_PreloadLabels.begin(), _PreloadLabels.end(), std::string()), _PreloadLabels.end());
    if (_PreloadLabels.empty()) _PreloadLabels = { "Reticulating Splines", "Interpreting Family Values", "Fabricating Imaginary Infrastructure" };
    _CurrentPreloadLabel = 0;
    _Done = false;
    AnimateLabel("", _PreloadLabels[0]);
}

void LoadingScreen::InitLot(const std::string& art, const std::string& title) {
    auto& ui = ResGraphics::Get();
    auto& S = FrontStrings::Get();
    _Background = Canvas();
    if (!art.empty()) ui.LoadScaled(art, 0.4f, _Background);
    if (_Background.Width == 0) ui.LoadScaled("kSimsLogo", 0.4f, _Background);
    if (_Background.Width == 0) ui.LoadScaled("Other/setup.bmp", 0.4f, _Background);
    _Copyright = S.Get(154, 5, "\xA9 2000-2005 Electronic Arts Inc. All Rights Reserved.");
    _Brand = S.Get(154, 4, "Maxis is an Electronic Arts brand.");
    Title = title;
    // the lot loading steps (house file, objects, family, view) under the game's own lot loading labels:
    // 4 Reticulating Splines, 14 Compiling Reticulated Splines, 15 Loading Characters, 16 Simulating Program Execution
    _PreloadLabels.clear();
    static const char* fb[4] = { "Reticulating Splines", "Compiling Reticulated Splines", "Loading Characters", "Simulating Program Execution" };
    int ids[4] = { 4, 14, 15, 16 };
    for (int i = 0; i < 4; i++) _PreloadLabels.push_back(S.Get(155, ids[i], fb[i]));
    _CurrentPreloadLabel = 0;
    _Progress = 0;
    _Done = false;
    CurrentStep.clear();
    AnimateLabel("", _PreloadLabels[0]);
    _InTween = false; // shown at once: the first frame is the only one before the blocking load
}

void LoadingScreen::SetLotStep(const std::string& step) {
    CurrentStep = step;
    float p = _Progress;
    if (step.find("objects") != std::string::npos) p = 0.25f;
    else if (step.find("family") != std::string::npos || step.find("travellers") != std::string::npos) p = 0.5f;
    else if (step.find("view") != std::string::npos || step.find("first frame") != std::string::npos) p = 0.75f;
    if (p > _Progress) _Progress = p;
}

void LoadingScreen::AnimateLabel(const std::string& previousLabel, const std::string& newLabel) {
    _InTween = true;
    _TweenT = 0;
    _Label1 = previousLabel; // ProgressLabel1: X 0 -> -800 over 1 s
    _Label2 = newLabel;      // ProgressLabel2: X 800 -> 0 over 1 s
}

void LoadingScreen::CheckPreloadLabel() {
    /** Have we preloaded the correct percent? **/
    float percentDone = _Progress;
    float percentUntilNextLabel = (float)(_CurrentPreloadLabel + 1) / (float)_PreloadLabels.size();
    if (percentDone >= percentUntilNextLabel) {
        if (!_InTween) {
            if (_CurrentPreloadLabel + 1 < (int)_PreloadLabels.size()) {
                _CurrentPreloadLabel++;
                AnimateLabel(_PreloadLabels[(size_t)_CurrentPreloadLabel - 1], _PreloadLabels[(size_t)_CurrentPreloadLabel]);
            } else {
                /** No more labels to show! Preload must be complete :) **/
                _Done = true;
            }
        }
    }
}

bool LoadingScreen::Update(double dtMs) {
    if (_InTween) {
        _TweenT += dtMs / 1000.0;
        if (_TweenT >= 1.0) { _TweenT = 1.0; _InTween = false; } // tween_OnComplete
    }
    CheckPreloadLabel();
    return _Done;
}

void LoadingScreen::Draw(Canvas& top, Canvas& bottom) {
    // C#: Draw fills the viewport with (0x09,0x18,0x2F), then the container (scaled to the screen height)
    top.Clear(RGBA(0x09, 0x18, 0x2F));
    int bx = (top.Width - _Background.Width) / 2;
    if (_Background.Width > 0) {
        top.Blit(_Background.Px.data(), _Background.Width, _Background.Height, bx, 0, false);
        // soft edges where the picture meets the background (C# splashSeg 9-slice edge)
        for (int x = 0; x < 12; x++) {
            int a = 255 - x * 21;
            for (int y = 0; y < top.Height; y++) {
                top.BlendA(bx + x, y, RGBA(0x09, 0x18, 0x2F), a);
                top.BlendA(bx + _Background.Width - 1 - x, y, RGBA(0x09, 0x18, 0x2F), a);
            }
        }
    }
    const float s = 0.4f;
    // lbl.Caption = 154/5, Y = 508, width 800, centred
    TextCentered(top, TS1Size::S9, Rect(0, (int)(508 * s) - 2, top.Width, 14), _Copyright, RGBA(255, 255, 255), RGBA(0, 0, 0, 200));
    // ProgressLabel1/2 at Y = 550, sliding (UITween linear)
    float t = (float)_TweenT;
    int y = (int)(550 * s) - 2;
    int w = top.Width;
    // FIX(sims) QA2: "Fabricating Imaginary Infrastructure" is wider than the top screen at 14 pt (cut at both edges)
    auto labelSize = [&](const std::string& l) { return TextW(TS1Size::S14, l) <= w - 8 ? TS1Size::S14 : TS1Size::S12; };
    if (!_Label1.empty() && _InTween) TextCentered(top, labelSize(_Label1), Rect((int)(-w * t), y, w, 18), _Label1, RGBA(255, 255, 255), RGBA(0, 0, 0, 200));
    TextCentered(top, labelSize(_Label2), Rect((int)(w * (_InTween ? (1.0f - t) : 0.0f)), y, w, 18), _Label2, RGBA(255, 255, 255), RGBA(0, 0, 0, 200));

    // ---- touch screen (TS1(sims))
    bottom.GradientV(0, 0, bottom.Width, bottom.Height, RGBA(0x12, 0x2A, 0x5A), RGBA(0x09, 0x18, 0x2F));
    TextCentered(bottom, TS1Size::S12, Rect(0, 18, bottom.Width, 18), Fit(TS1Size::S12, Title.empty() ? FrontStrings::Get().Get(153, 0, "The Sims Complete Collection") : Title, bottom.Width - 16), RGBA(255, 216, 96), RGBA(0, 0, 0, 180));
    // the labels shown so far
    int ly = 52;
    for (int i = 0; i <= _CurrentPreloadLabel && i < (int)_PreloadLabels.size(); i++) {
        bool cur = i == _CurrentPreloadLabel && !_Done;
        std::string l = _PreloadLabels[(size_t)i] + (cur ? std::string("...") : std::string(""));
        Text(bottom, TS1Size::S10, 40, ly, l, cur ? RGBA(255, 255, 255) : RGBA(130, 160, 210), RGBA(0, 0, 0, 150));
        if (!cur) Text(bottom, TS1Size::S10, 18, ly, "\x95", RGBA(120, 220, 90));
        ly += 15;
    }
    // progress bar
    Rect bar(40, 196, bottom.Width - 80, 10);
    bottom.FillRoundRect(bar, 4, RGBA(4, 10, 30));
    int pw = (int)((bar.W - 4) * std::min(1.0f, std::max(0.0f, _Progress)));
    if (pw > 0) bottom.GradientV(bar.X + 2, bar.Y + 2, pw, bar.H - 4, RGBA(120, 230, 255), RGBA(30, 110, 200));
    bottom.StrokeRoundRect(bar.X, bar.Y, bar.W, bar.H, 4, RGBA(90, 130, 210));
    if (!CurrentStep.empty()) TextCentered(bottom, TS1Size::S9, Rect(0, 178, bottom.Width, 14), Fit(TS1Size::S9, CurrentStep, bottom.Width - 20), RGBA(130, 160, 210));
    TextCentered(bottom, TS1Size::S9, Rect(0, 218, bottom.Width, 14), _Brand, RGBA(160, 180, 220));
}

}}
