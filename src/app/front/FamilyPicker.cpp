/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims front end — see FamilyPicker.h. TS1(sims).
#include "app/front/FamilyPicker.h"
#include "app/front/FrontStrings.h"
#include "app/front/TS1Font.h"
#include <algorithm>
#include <cmath>

namespace simapp { namespace front {

static const float S = 0.4f;

void FamilyPicker::LoadArt() {
    if (_ArtLoaded) return;
    auto& ui = ResGraphics::Get();
    ui.LoadScaled("kPickFamilyBkg", S, _Bkg);
    Canvas item;
    if (ui.Load("kPickFamilyItemBkg", item)) {
        // 3 frames stacked vertically (normal, pressed, selected) -> scale each
        int fh = item.Height / 3, dw = (int)(item.Width * S + 0.5f), dh = (int)(fh * S + 0.5f);
        _Item.Resize(dw, dh * 3);
        for (int f = 0; f < 3; f++) {
            Canvas fr, sc;
            Crop(item, fr, 0, f * fh, item.Width, fh);
            Resample(fr, sc, dw, dh);
            std::copy(sc.Px.begin(), sc.Px.end(), _Item.Px.begin() + (size_t)f * dw * dh);
        }
    }
    _Add.Load(ui, "kPickFamilyAdd", S);         _Add.Id = "add";
    _Delete.Load(ui, "kPickFamilyDelete", S);   _Delete.Id = "delete";
    _MoveIn.Load(ui, "kPickFamilyMoveIn", S);   _MoveIn.Id = "movein";
    _Cancel.Load(ui, "kPickFamilyCancel", S, 6); _Cancel.Id = "cancel";
    // the button slots of PickBkg (325..668, 530..590)
    _Add.X = (int)(330 * S);    _Add.Y = (int)(531 * S);
    _Delete.X = (int)(417 * S); _Delete.Y = (int)(531 * S);
    _MoveIn.X = (int)(509 * S); _MoveIn.Y = (int)(531 * S);
    _Cancel.X = (int)(30 * S);  _Cancel.Y = (int)(538 * S);
    auto& T = FrontStrings::Get();
    _Add.Tooltip = T.Get(128, 2, "Create New Family");
    _Delete.Tooltip = T.Get(128, 4, "Delete Family");
    _MoveIn.Tooltip = T.Get(128, 3, "Move In Family");
    _Cancel.Tooltip = T.Get(128, 5, "Cancel");
    _ArtLoaded = true;
}

std::vector<const FamilyInfo*> FamilyPicker::Families() const {
    if (!ImportMode) return _M ? _M->Bin() : std::vector<const FamilyInfo*>();
    std::vector<const FamilyInfo*> r;
    for (auto& e : Imports) r.push_back(&e.Info);
    return r;
}

void FamilyPicker::ShowImport(NhoodModel* model, std::vector<ImportEntry> entries) {
    Show(model, -1);
    ImportMode = true;
    Imports = std::move(entries);
    _List = Families();
    _MoveIn.Tooltip = FrontStrings::Get().Get(151, 6, "Import Family");
}

void FamilyPicker::Show(NhoodModel* model, int targetLot) {
    _M = model;
    ImportMode = false;
    Imports.clear();
    if (_ArtLoaded) _MoveIn.Tooltip = FrontStrings::Get().Get(128, 3, "Move In Family");
    TargetLot = targetLot;
    LoadArt();
    _List = model->Bin();
    _Sel = 0; _Scroll = 0;
    Open = true;
}

int16_t FamilyPicker::SelectedFamily() const {
    if (!_M) return -1;
    if (ImportMode) return (int16_t)((_Sel >= 0 && _Sel < (int)Imports.size()) ? _Sel : -1);
    auto list = _M->Bin();
    if (_Sel < 0 || _Sel >= (int)list.size()) return -1;
    return list[(size_t)_Sel]->ChunkID;
    if (_Sel < 0 || _Sel >= (int)_List.size()) return -1;
    return _List[(size_t)_Sel]->ChunkID;
}

Rect FamilyPicker::RowRect(int i) const {
    int h = _Item.Width > 0 ? _Item.Height / 3 : 26;
    return Rect((int)(35 * S), (int)(74 * S) + (i - _Scroll) * (h + 2), _Item.Width > 0 ? _Item.Width : 276, h);
}

void FamilyPicker::Update(const AppInput& in) {
    if (!Open) return;
    _List = Families();
    int n = (int)_List.size();
    if (_Sel >= n) _Sel = n - 1;
    if (_Sel < 0 && n > 0) _Sel = 0;
    _MoveIn.Disabled = _Delete.Disabled = (n == 0);
    if (in.Down & BTN_DOWN) _Sel = std::min(n - 1, _Sel + 1);
    if (in.Down & BTN_UP) _Sel = std::max(0, _Sel - 1);
    if (_Sel < _Scroll) _Scroll = _Sel;
    if (_Sel >= _Scroll + Rows) _Scroll = _Sel - Rows + 1;
    _Scroll = std::max(0, _Scroll);
    if (in.TouchDown) {
        for (int i = _Scroll; i < std::min(n, _Scroll + Rows); i++)
            if (RowRect(i).Contains(in.TouchX, in.TouchY)) {
                if (i == _Sel && ImportMode) { if (OnImport) OnImport(Imports[(size_t)i].Path); return; }
                if (i == _Sel && OnMoveIn) { OnMoveIn(_List[(size_t)i]->ChunkID); return; } // second tap: move in
                _Sel = i;
            }
        // scroll arrows (right of the list)
        if (n > Rows && Rect(300, 28, 18, 40).Contains(in.TouchX, in.TouchY)) _Scroll = std::max(0, _Scroll - 1);
        if (n > Rows && Rect(300, 160, 18, 40).Contains(in.TouchX, in.TouchY)) _Scroll = std::min(n - Rows, _Scroll + 1);
    }
    if (ImportMode) {
        if ((_MoveIn.Update(in) || (in.Down & BTN_A)) && OnImport && _Sel >= 0 && _Sel < (int)Imports.size()) { OnImport(Imports[(size_t)_Sel].Path); return; }
        if ((_Cancel.Update(in) || (in.Down & BTN_B)) && OnCancel) { OnCancel(); return; }
        return;
    }
    if (_Add.Update(in) && OnCreate) { OnCreate(); return; }
    if (_Delete.Update(in) && OnDelete && SelectedFamily() >= 0) { OnDelete(SelectedFamily()); return; }
    if ((_MoveIn.Update(in) || (in.Down & BTN_A)) && OnMoveIn && SelectedFamily() >= 0) { OnMoveIn(SelectedFamily()); return; }
    if ((_Cancel.Update(in) || (in.Down & BTN_B)) && OnCancel) { OnCancel(); return; }
}

void FamilyPicker::DrawBottom(Canvas& c) {
    if (!Open) return;
    _List = Families(); // the families may have changed since Update (delete, CAS): never keep stale pointers
    auto& T = FrontStrings::Get();
    if (_Bkg.Width > 0) c.Blit(_Bkg.Px.data(), _Bkg.Width, _Bkg.Height, 0, 0, false);
    else c.GradientV(0, 0, c.Width, c.Height, RGBA(10, 30, 120), RGBA(5, 10, 60));
    Text(c, TS1Size::S12, 12, 7, ImportMode ? T.Get(151, 6, "Import Family") : T.Get(128, 8, "SELECT A FAMILY"), RGBA(255, 255, 255), RGBA(0, 0, 40, 200));
    int n = (int)_List.size();
    if (n == 0 && ImportMode) {
        TextCentered(c, TS1Size::S10, Rect(8, 90, 288, 14), "No family files (.FAM) found.", RGBA(200, 220, 255));
    } else if (n == 0) {
        auto lines = Wrap(TS1Size::S10, T.Get(128, 14, "You can create a new family by clicking on the Create a Family button at the bottom of the screen"), 240);
        int y = 90;
        for (auto& l : lines) { TextCentered(c, TS1Size::S10, Rect(8, y, 288, 14), l, RGBA(200, 220, 255)); y += 14; }
    }
    int fh = _Item.Width > 0 ? _Item.Height / 3 : 26;
    _Scroll = std::max(0, std::min(_Scroll, std::max(0, n - Rows)));
    for (int i = _Scroll; i < std::min(n, _Scroll + Rows); i++) {
        Rect r = RowRect(i);
        bool sel = i == _Sel;
        if (_Item.Width > 0) {
            const uint32_t* src = &_Item.Px[(size_t)(sel ? 2 : 0) * _Item.Width * fh];
            c.Blit(src, _Item.Width, fh, r.X, r.Y, true);
        } else c.FillRoundRect(r, 6, sel ? RGBA(60, 60, 140) : RGBA(0, 0, 60));
        auto f = _List[(size_t)i];
        std::string name = f->Name;
        Text(c, TS1Size::S11, r.X + 8, r.Y + (r.H - TextH(TS1Size::S11)) / 2, Fit(TS1Size::S11, name, 150), sel ? RGBA(255, 255, 255) : RGBA(170, 200, 255), RGBA(0, 0, 30, 180));
        // Net Worth under the Simoleon head (x 598), Friends under the smiley (x 675)
        std::string nw = Simoleons(f->Budget);
        Text(c, TS1Size::S10, (int)(640 * S) - TextW(TS1Size::S10, nw), r.Y + (r.H - TextH(TS1Size::S10)) / 2, nw, RGBA(255, 255, 255), RGBA(0, 0, 30, 180));
        std::string fr = std::to_string(f->Friends);
        Text(c, TS1Size::S10, (int)(681 * S) - TextW(TS1Size::S10, fr) / 2, r.Y + (r.H - TextH(TS1Size::S10)) / 2, fr, RGBA(255, 255, 255), RGBA(0, 0, 30, 180));
    }
    if (n > Rows) {
        c.FillTriangle(303, 44, 315, 44, 309, 34, RGBA(150, 200, 255));
        c.FillTriangle(303, 184, 315, 184, 309, 194, RGBA(150, 200, 255));
    }
    if (!ImportMode) { _Add.Draw(c); _Delete.Draw(c); }
    _MoveIn.Draw(c); _Cancel.Draw(c);
}

void FamilyPicker::DrawTop(Canvas& top) {
    if (!Open) return;
    _List = Families();
    _Time += 33;
    auto& T = FrontStrings::Get();
    top.Dim(0, 0, top.Width, top.Height, 110);
    Rect p(10, 8, top.Width - 20, top.Height - 16);
    auto& art = FrontArt::Get();
    if (art.DialogTiler.Width > 0) { NineSlice(top, art.DialogTiler, p, 14); NineSlice(top, art.DialogTiler, Rect(p.X + 3, p.Y + 3, p.W - 6, p.H - 6), 14); }
    else top.FillRoundRect(p, 10, RGBA(0, 10, 80, 230));
    int sel = _Sel;
    if (sel < 0 || sel >= (int)_List.size()) {
        TextCentered(top, TS1Size::S16, Rect(p.X, p.Y + 20, p.W, 24), T.Get(128, 13, "Create a Family?"), RGBA(255, 216, 96), RGBA(0, 0, 30, 200));
        auto lines = Wrap(TS1Size::S12, T.Get(128, 14, "You can create a new family by clicking on the Create a Family button at the bottom of the screen"), p.W - 60);
        int y = p.Y + 60;
        for (auto& l : lines) { TextCentered(top, TS1Size::S12, Rect(p.X, y, p.W, 16), l, RGBA(255, 255, 255)); y += 17; }
        return;
    }
    auto f = _List[(size_t)sel];
    std::string title = Format(T.Get(134, 0, "The %s Family"), { f->Name });
    title = title.substr(0, title.find('\n'));
    Text(top, TS1Size::S20, p.X + 14, p.Y + 10, title, RGBA(255, 216, 96), RGBA(0, 0, 30, 220));
    std::string money = T.Get(128, 10, "Net Worth") + ": " + Simoleons(f->Budget) + "     " + T.Get(128, 11, "Family Friends") + ": " + std::to_string(f->Friends);
    Text(top, TS1Size::S11, p.X + 16, p.Y + 38, money, RGBA(170, 210, 255), RGBA(0, 0, 30, 200));
    // FIX(sims) QA2: the lot text below the members ("If you buy this lot, the family will have $9,500 to spend to build
    // a ...") was cut after 6 lines: wrap it first, keep up to 8 lines (ellipsis beyond) and leave the members that much room.
    std::vector<std::string> lotLines;
    LotInfo* lotForText = TargetLot >= 0 ? _M->Lot(TargetLot) : nullptr;
    if (lotForText) {
        lotLines = Wrap(TS1Size::S10, _M->Rollover(*lotForText, f), p.W - 40 - (lotForText->Thumb.Width > 0 ? 90 : 0));
        if (lotLines.size() > 8) { lotLines.resize(8); lotLines[7] = Fit(TS1Size::S10, lotLines[7] + "...", p.W - 130); }
    }
    int lotTextH = lotForText ? 10 + (int)lotLines.size() * 12 + 4 : 0;
    // the members: a little figure (adult / child, blue / pink as the CAS buttons), first name, biography
    int y = p.Y + 60;
    int perRow = (int)f->Members.size() > 4 ? 2 : 1;
    int colW = (p.W - 28) / perRow;
    for (size_t i = 0; i < f->Members.size(); i++) {
        auto& m = f->Members[i];
        int col = (int)i % perRow, x = p.X + 14 + col * colW;
        int yy = y + (int)(i / perRow) * 31;
        if (yy > p.Bottom() - (lotForText ? lotTextH + 28 : 24)) break;
        int fx = x + 20;
        if (m.Face.Width > 0) {
            // the portrait (Nbhd/PersonInfoBkg.bmp frames it in the original family view)
            Canvas small;
            Resample(m.Face, small, 22, 26);
            top.FillRect(x - 1, yy - 1, 24, 28, RGBA(0, 0, 40));
            top.Blit(small.Px.data(), small.Width, small.Height, x, yy, true);
            top.StrokeRect(x - 1, yy - 1, 24, 28, m.Female ? RGBA(240, 120, 170) : RGBA(90, 160, 255));
            fx = x + 28;
        } else {
            uint32_t body = m.Female ? RGBA(240, 120, 170) : RGBA(90, 160, 255);
            int hh = m.Child ? 9 : 13;
            top.FillCircle(x + 7, yy + 16 - hh, 4, RGBA(250, 220, 180));
            top.FillRoundRect(x + 2, yy + 20 - hh, 10, hh, 3, body);
        }
        Text(top, TS1Size::S12, fx, yy, m.FirstName, RGBA(255, 255, 255), RGBA(0, 0, 30, 200));
        int nx = fx + 4 + TextW(TS1Size::S12, m.FirstName);
        Text(top, TS1Size::S9, nx, yy + 3, m.Child ? "(child)" : "(adult)", RGBA(140, 170, 220));
        if (perRow == 1 && !m.Bio.empty())
            Text(top, TS1Size::S9, fx, yy + 15, Fit(TS1Size::S9, m.Bio, colW - (fx - x) - 10), RGBA(180, 200, 235));
    }
    if (lotForText) {
        auto lot = lotForText;
        {
            auto& lines = lotLines;
            int ly = p.Bottom() - 10 - (int)lines.size() * 12;
            if (lot->Thumb.Width > 0) {
                Canvas small;
                float sc = std::min(80.0f / lot->Thumb.Width, 46.0f / lot->Thumb.Height);
                Resample(lot->Thumb, small, (int)(lot->Thumb.Width * sc), (int)(lot->Thumb.Height * sc));
                top.Blit(small.Px.data(), small.Width, small.Height, p.Right() - 14 - small.Width, p.Bottom() - 12 - small.Height, true);
            }
            for (size_t i = 0; i < lines.size(); i++) { Text(top, TS1Size::S10, p.X + 16, ly, lines[i], i == 0 ? RGBA(255, 216, 96) : RGBA(255, 255, 255), RGBA(0, 0, 30, 200)); ly += 12; }
        }
    }
}

bool FamilyPicker::ButtonPos(const std::string& id, int& x, int& y) const {
    for (auto b : { &_Add, &_Delete, &_MoveIn, &_Cancel }) {
        if (b->Id != id) continue;
        x = b->X + b->Width() / 2; y = b->Y + b->Height() / 2;
        return true;
    }
    return false;
}

bool FamilyPicker::RowPos(const std::string& name, int& x, int& y) {
    for (int i = 0; i < (int)_List.size(); i++) {
        if (_List[(size_t)i]->Name.find(name) == std::string::npos) continue;
        if (i < _Scroll) _Scroll = i;
        if (i >= _Scroll + Rows) _Scroll = i - Rows + 1;
        Rect r = RowRect(i);
        x = r.X + r.W / 2; y = r.Y + r.H / 2;
        return true;
    }
    return false;
}

}}
