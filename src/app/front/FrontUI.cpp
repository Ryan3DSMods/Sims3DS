/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims front end — see FrontUI.h. TS1(sims).
#include "app/front/FrontUI.h"
#include <algorithm>

namespace simapp { namespace front {

void NineSlice(Canvas& dst, const Canvas& tile, const Rect& r, int b) {
    if (tile.Width <= 0 || r.W <= 0 || r.H <= 0) return;
    int tw = tile.Width, th = tile.Height;
    b = std::min(b, std::min(tw / 2, th / 2));
    b = std::min(b, std::min(r.W / 2, r.H / 2));
    for (int y = 0; y < r.H; y++) {
        int dy = r.Y + y;
        if (dy < 0 || dy >= dst.Height) continue;
        int sy;
        if (y < b) sy = y;
        else if (y >= r.H - b) sy = th - (r.H - y);
        else sy = b + (int)((int64_t)(y - b) * (th - 2 * b) / std::max(1, r.H - 2 * b));
        const uint32_t* row = &tile.Px[(size_t)sy * tw];
        uint32_t* out = &dst.Px[(size_t)dy * dst.Width];
        for (int x = 0; x < r.W; x++) {
            int dx = r.X + x;
            if (dx < 0 || dx >= dst.Width) continue;
            int sx;
            if (x < b) sx = x;
            else if (x >= r.W - b) sx = tw - (r.W - x);
            else sx = b + (int)((int64_t)(x - b) * (tw - 2 * b) / std::max(1, r.W - 2 * b));
            Canvas::BlendPx(out[dx], row[sx]);
        }
    }
}

FrontArt& FrontArt::Get() {
    static FrontArt a;
    return a;
}

void FrontArt::Load(const ResGraphics& ui) {
    if (Loaded) return;
    ui.Load("Nbhd/NScreenDialogTiler.TGA", DialogTiler, false);
    ui.Load("Nbhd/HotspotPopupTiler.TGA", PopupTiler, false);
    ui.Load("Shared/Sys/CheckBtnNew.BMP", CheckBtn);
    Canvas btn;
    if (ui.Load("Shared/Sys/WinBtn.BMP", btn)) {
        // scale each of the 4 frames to 0.6 (65x33 -> 39x20)
        int fw = btn.Width / 4, fh = btn.Height;
        int dw = (int)(fw * 0.6f + 0.5f), dh = (int)(fh * 0.6f + 0.5f);
        WinBtn.Resize(dw * 4, dh);
        for (auto& p : WinBtn.Px) p = 0;
        for (int f = 0; f < 4; f++) {
            Canvas fr, sc;
            Crop(btn, fr, f * fw, 0, fw, fh);
            Resample(fr, sc, dw, dh);
            for (int y = 0; y < dh; y++) std::copy(sc.Px.begin() + (size_t)y * dw, sc.Px.begin() + (size_t)(y + 1) * dw, WinBtn.Px.begin() + (size_t)y * WinBtn.Width + f * dw);
        }
    }
    Loaded = true;
}

// ---------------------------------------------------------------- TextButton

bool TextButton::Update(const AppInput& in) {
    if (Disabled) { _Down = false; return false; }
    bool inside = R.Contains(in.TouchX, in.TouchY);
    if (in.TouchDown && inside) { _Down = true; _Over = true; }
    if (_Down && in.Touching) _Over = inside;
    if (_Down && in.TouchUp) { _Down = false; _Over = false; return inside; }
    if (!in.Touching && !in.TouchUp) _Down = false;
    return false;
}

void TextButton::Draw(Canvas& c) const {
    auto& art = FrontArt::Get();
    int frame = Disabled ? 3 : (Selected ? 1 : ((_Down && _Over) ? 2 : 0));
    if (art.WinBtn.Width > 0) {
        // 3-slice (C# UIButton.Draw with Width set: left third, stretched centre, right third)
        int fw = art.WinBtn.Width / 4, fh = art.WinBtn.Height, third = fw / 3;
        Canvas frameImg;
        Crop(art.WinBtn, frameImg, frame * fw, 0, fw, fh);
        int y0 = R.Y + (R.H - fh) / 2;
        for (int y = 0; y < fh; y++) {
            int dy = y0 + y;
            if (dy < 0 || dy >= c.Height) continue;
            for (int x = 0; x < R.W; x++) {
                int sx;
                if (x < third) sx = x;
                else if (x >= R.W - third) sx = fw - (R.W - x);
                else sx = third + (x - third) % std::max(1, fw - 2 * third);
                int dx = R.X + x;
                if (dx < 0 || dx >= c.Width) continue;
                Canvas::BlendPx(c.Px[(size_t)dy * c.Width + dx], frameImg.Px[(size_t)y * fw + sx]);
            }
        }
    } else {
        c.FillRoundRect(R, 6, Disabled ? RGBA(40, 50, 80) : RGBA(30, 80, 150));
        c.StrokeRoundRect(R.X, R.Y, R.W, R.H, 6, RGBA(120, 170, 230));
    }
    uint32_t tc = Disabled ? RGBA(120, 130, 160) : (frame == 2 ? RGBA(0, 255, 255) : RGBA(255, 255, 255));
    TextCentered(c, TS1Size::S10, R, Fit(TS1Size::S10, Label, R.W - 10), tc, RGBA(0, 0, 40, 200));
}

// ---------------------------------------------------------------- FrontDialog

void FrontDialog::Show(const std::string& title, const std::string& message, const std::vector<std::string>& buttons, std::function<void(int)> onResult) {
    Title = title; Message = message;
    Buttons.clear();
    for (size_t i = 0; i < buttons.size(); i++) { TextButton b; b.Label = buttons[i]; b.Id = std::to_string(i); Buttons.push_back(b); }
    OnResult = onResult;
    Open = true;
}

static const int DLG_W = 280;

bool FrontDialog::Update(const AppInput& in) {
    if (!Open) return false;
    int chosen = -1;
    for (size_t i = 0; i < Buttons.size(); i++) if (Buttons[i].Update(in)) chosen = (int)i;
    if (chosen < 0 && (in.Down & BTN_A) && !Buttons.empty()) chosen = 0;
    if (chosen < 0 && (in.Down & BTN_B) && !Buttons.empty()) chosen = (int)Buttons.size() - 1;
    if (chosen >= 0) {
        Open = false;
        auto cb = OnResult;
        OnResult = nullptr;
        if (cb) cb(chosen);
    }
    return true;
}

void FrontDialog::Draw(Canvas& c) {
    if (!Open) return;
    c.Dim(0, 0, c.Width, c.Height, 120);
    auto lines = Wrap(TS1Size::S10, Message, DLG_W - 24);
    if (lines.size() > 10) lines.resize(10);
    int lh = TextH(TS1Size::S10) + 1;
    int h = 34 + (int)lines.size() * lh + 36;
    Rect p((c.Width - DLG_W) / 2, std::max(2, (c.Height - h) / 2), DLG_W, h);
    auto& art = FrontArt::Get();
    if (art.DialogTiler.Width > 0) {
        NineSlice(c, art.DialogTiler, p, 14);
        NineSlice(c, art.DialogTiler, Rect(p.X + 4, p.Y + 4, p.W - 8, p.H - 8), 14);
    } else c.FillRoundRect(p, 10, RGBA(0, 10, 80, 235));
    c.StrokeRoundRect(p.X + 2, p.Y + 2, p.W - 4, p.H - 4, 10, RGBA(90, 120, 210, 200));
    TextCentered(c, TS1Size::S12, Rect(p.X, p.Y + 7, p.W, 18), Fit(TS1Size::S12, Title, p.W - 16), RGBA(255, 216, 96), RGBA(0, 0, 30, 220));
    int y = p.Y + 30;
    for (auto& l : lines) { Text(c, TS1Size::S10, p.X + 12, y, l, RGBA(255, 255, 255), RGBA(0, 0, 30, 160)); y += lh; }
    int n = (int)Buttons.size();
    int bw = std::min(100, (p.W - 24) / std::max(1, n) - 6);
    int bx = p.X + (p.W - n * (bw + 6) + 6) / 2;
    for (auto& b : Buttons) {
        b.R = Rect(bx, p.Bottom() - 30, bw, 22);
        b.Draw(c);
        bx += bw + 6;
    }
}

bool FrontDialog::ButtonPos(int index, int& x, int& y) const {
    if (!Open || index < 0 || index >= (int)Buttons.size()) return false;
    x = Buttons[(size_t)index].R.X + Buttons[(size_t)index].R.W / 2;
    y = Buttons[(size_t)index].R.Y + Buttons[(size_t)index].R.H / 2;
    return true;
}

}}
