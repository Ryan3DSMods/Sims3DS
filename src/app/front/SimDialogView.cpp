/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims front end — see SimDialogView.h
#include "app/front/SimDialogView.h"
#include "app/front/FrontAudio.h"
#include "app/front/FrontStrings.h"
#include "app/front/ResGraphics.h"
#include "app/front/TS1Font.h"
#include "app/Font.h"
#include "app/LotScreen.h"
#include "app/LotSession.h"
#include "simantics/VM.h"
#include "simantics/entities/VMAvatar.h"
#include <algorithm>
#include "simantics/engine/VMTS1Outfits.h"
#include "simantics/engine/VMTS1Travel.h"
#include "vitaboy/content/TS1AvatarProvider.h"
#include "vitaboy/Texture.h"
#include "app/ui/UIGraphics.h"

namespace simapp { namespace front {

void SimDialogView::Layout(LotSession& s) {
    auto& d = s.Dialogs.front();
    auto& T = FrontStrings::Get();
    _Btns.clear(); _Codes.clear();
    auto add = [&](const std::string& label, int code) { TextButton b; b.Label = ToLatin1(label); _Btns.push_back(b); _Codes.push_back(code); };
    if (d.HasYes) add(d.Yes, 0);
    if (d.HasNo) add(d.No, 1);
    if (d.HasCancel) add(d.Cancel, 2);
    if (_Btns.empty()) add(T.Get(152, 0, "OK"), 0);
    int n = (int)_Btns.size();
    int bw = std::min(96, (300 - 8 * (n - 1)) / n);
    int x = (320 - (n * bw + (n - 1) * 8)) / 2;
    for (auto& b : _Btns) { b.R = Rect(x, 172, bw, 30); x += bw + 8; }
}

// ---- TS1(sims): Clothing dialog (dialog type 6; FreeSO VMTS1PurchasableOutfitHelper lists)
void SimDialogView::LoadOutfits(LotSession& s) {
    auto& d = s.Dialogs.front();
    if (_OutfitAvatar == d.AvatarID && _OutfitType == d.OutfitType) return;
    _OutfitAvatar = d.AvatarID; _OutfitType = d.OutfitType;
    _Outfits.clear(); _OutfitPage = 0; _OutfitSel = -1;
    auto av = dynamic_cast<FSO::SimAntics::VMAvatar*>(s.VM()->GetObjectById(d.AvatarID));
    if (av == nullptr) return;
    auto list = FSO::SimAntics::Engine::Utils::VMTS1Outfits::ValidOutfits(av, d.OutfitType);
    _OutfitSel = FSO::SimAntics::Engine::Utils::VMTS1Outfits::GetSuitIndex(av, d.OutfitType);
    auto content = FSO::Content::TS1::TS1AvatarContent::Get();
    for (auto& o : list) {
        OutfitThumb t;
        auto at = o.BodyString.find("BODY=");
        t.Texture = (at == std::string::npos) ? o.BodyString : o.BodyString.substr(at + 5);
        if (content != nullptr) {
            if (auto tex = content->AvatarTextures.Get(t.Texture)) {
                // the body texture's upper part (the shirt) as the thumbnail, like the original dialog
                Canvas full;
                full.Resize(tex->Width, tex->Height);
                for (int i = 0; i < tex->Width * tex->Height; i++) {
                    uint32_t p = tex->Paletted ? tex->Palette[tex->Indices[(size_t)i]] : tex->Pixels[(size_t)i];
                    full.Px[(size_t)i] = (p & 0x00FFFFFFu) | 0xFF000000u;
                }
                int side = std::min(tex->Width, tex->Height);
                UIGraphics::Resample(full, Rect(0, 0, side, side), t.Img, 56, 56);
            }
        }
        _Outfits.push_back(std::move(t));
    }
}

bool SimDialogView::UpdateClothes(LotSession& s, const AppInput& in) {
    LoadOutfits(s);
    int n = (int)_Outfits.size(), pages = std::max(1, (n + 7) / 8);
    if (in.Down & BTN_RIGHT) _OutfitPage = std::min(pages - 1, _OutfitPage + 1);
    if (in.Down & BTN_LEFT) _OutfitPage = std::max(0, _OutfitPage - 1);
    if (in.Down & BTN_B) { FrontSound("ui_click"); s.RespondDialog(0); return true; } // 0 = cancel (FreeSO)
    if (in.TouchDown) {
        for (size_t i = 0; i < _OutfitRects.size(); i++) {
            if (!_OutfitRects[i].Contains(in.TouchX, in.TouchY)) continue;
            int idx = _OutfitPage * 8 + (int)i;
            if (idx >= n) break;
            FrontSound("ui_click");
            s.RespondDialog(1, std::to_string(idx)); // the index in temp 0/1 (VMDialogPrivateStrings TS1Clothes)
            return true;
        }
        if (Rect(4, 100, 20, 40).Contains(in.TouchX, in.TouchY)) _OutfitPage = std::max(0, _OutfitPage - 1);
        if (Rect(296, 100, 20, 40).Contains(in.TouchX, in.TouchY)) _OutfitPage = std::min(pages - 1, _OutfitPage + 1);
        if (Rect(120, 210, 80, 24).Contains(in.TouchX, in.TouchY)) { FrontSound("ui_click"); s.RespondDialog(0); return true; }
    }
    if ((in.Down & BTN_A) && _OutfitSel >= 0 && _OutfitSel < n) { s.RespondDialog(1, std::to_string(_OutfitSel)); return true; }
    return true;
}

void SimDialogView::DrawClothes(LotSession& s, Canvas& top, Canvas& bottom) {
    LoadOutfits(s);
    auto& d = s.Dialogs.front();
    auto& T = FrontStrings::Get();
    auto& art = FrontArt::Get();
    auto panel = [&](Canvas& c, const Rect& r) {
        if (art.DialogTiler.Width > 0) { NineSlice(c, art.DialogTiler, r, 14); NineSlice(c, art.DialogTiler, Rect(r.X + 3, r.Y + 3, r.W - 6, r.H - 6), 14); }
        else c.FillRoundRect(r, 10, RGBA(14, 22, 96, 245));
    };
    std::string title = ToLatin1(d.Title.empty() ? std::string("Clothing") : d.Title);
    top.Dim(0, 0, top.Width, top.Height, 150);
    Rect p(8, 40, top.Width - 16, 150);
    panel(top, p);
    TextCentered(top, TS1Size::S16, Rect(p.X, p.Y + 8, p.W, 22), title, RGBA(255, 216, 96), RGBA(0, 0, 30, 220));
    auto lines = Wrap(TS1Size::S12, ToLatin1(d.Message), p.W - 30);
    int y = p.Y + 38;
    for (size_t i = 0; i < lines.size() && i < 5; i++) { TextCentered(top, TS1Size::S12, Rect(p.X, y, p.W, 16), lines[i], RGBA(255, 255, 255), RGBA(0, 0, 30, 200)); y += 16; }
    // bottom: the outfits
    bottom.Dim(0, 0, bottom.Width, bottom.Height, 170);
    Rect bp(4, 4, bottom.Width - 8, 232);
    panel(bottom, bp);
    TextCentered(bottom, TS1Size::S12, Rect(bp.X, bp.Y + 6, bp.W, 16), title, RGBA(255, 216, 96), RGBA(0, 0, 30, 220));
    _OutfitRects.clear();
    int n = (int)_Outfits.size(), pages = std::max(1, (n + 7) / 8);
    for (int k = 0; k < 8; k++) {
        int idx = _OutfitPage * 8 + k;
        Rect r(32 + (k % 4) * 66, 30 + (k / 4) * 88, 60, 82);
        _OutfitRects.push_back(r);
        if (idx >= n) continue;
        bottom.FillRoundRect(r, 6, idx == _OutfitSel ? RGBA(40, 90, 190) : RGBA(0, 0, 40));
        auto& o = _Outfits[(size_t)idx];
        if (o.Img.Width > 0) bottom.Blit(o.Img.Px.data(), o.Img.Width, o.Img.Height, r.X + 2, r.Y + 2, false);
        if (idx == _OutfitSel) bottom.StrokeRoundRect(r.X, r.Y, r.W, r.H, 6, RGBA(0, 255, 255));
        std::string nm = o.Texture;
        auto u = nm.find('_'); if (u != std::string::npos) nm = nm.substr(u + 1);
        // FIX(sims) QA3: long names ("beach...") use two lines instead of an ellipsis
        if (TextW(TS1Size::S9, nm) <= r.W - 2) TextCentered(bottom, TS1Size::S9, Rect(r.X, r.Bottom() - 22, r.W, 12), nm, RGBA(220, 230, 255));
        else {
            size_t cut = nm.size() / 2;
            while (cut + 1 < nm.size() && TextW(TS1Size::S9, nm.substr(0, cut + 1)) <= r.W - 2) cut++;
            TextCentered(bottom, TS1Size::S9, Rect(r.X, r.Bottom() - 24, r.W, 11), nm.substr(0, cut), RGBA(220, 230, 255));
            TextCentered(bottom, TS1Size::S9, Rect(r.X, r.Bottom() - 13, r.W, 11), Fit(TS1Size::S9, nm.substr(cut), r.W - 2), RGBA(220, 230, 255));
        }
    }
    if (n == 0) TextCentered(bottom, TS1Size::S10, Rect(bp.X, 100, bp.W, 14), "No outfits to choose from.", RGBA(220, 230, 255));
    if (_OutfitPage > 0) bottom.FillTriangle(8, 120, 20, 104, 20, 136, RGBA(150, 200, 255));
    if (_OutfitPage + 1 < pages) bottom.FillTriangle(312, 120, 300, 104, 300, 136, RGBA(150, 200, 255));
    TextButton cancel; cancel.Label = T.Get(152, 1, "Cancel"); cancel.R = Rect(120, 210, 80, 24);
    cancel.Draw(bottom);
}

// ---- FIX(sims): the spellbook (dialog type 12)
static const int kSpellRowY = 50, kSpellRowH = 22;

bool SimDialogView::UpdateSpellbook(LotSession& s, const AppInput& in) {
    int n = (int)FSO::SimAntics::Engine::Utils::VMTS1Travel::Spellbook(s.VM()).size();
    int pages = std::max(1, (n + 5) / 6);
    auto setPage = [&](int p) { p = std::max(0, std::min(pages - 1, p)); if (p != _SpellPage) { _SpellPage = p; _SpellSel = 0; } };
    if ((in.Down & (BTN_RIGHT | BTN_R)) || (in.TouchDown && Rect(276, 200, 40, 32).Contains(in.TouchX, in.TouchY))) setPage(_SpellPage + 1);
    if ((in.Down & (BTN_LEFT | BTN_L)) || (in.TouchDown && Rect(4, 200, 40, 32).Contains(in.TouchX, in.TouchY))) setPage(_SpellPage - 1);
    int onPage = std::max(1, std::min(6, n - _SpellPage * 6));
    if (in.Down & BTN_DOWN) _SpellSel = std::min(onPage - 1, _SpellSel + 1);
    if (in.Down & BTN_UP) _SpellSel = std::max(0, _SpellSel - 1);
    if (in.TouchDown) for (int k = 0; k < onPage; k++)
        if (Rect(12, kSpellRowY + k * kSpellRowH, 146, kSpellRowH - 2).Contains(in.TouchX, in.TouchY)) _SpellSel = k;
    if ((in.Down & (BTN_A | BTN_B)) || (in.TouchDown && Rect(120, 202, 80, 28).Contains(in.TouchX, in.TouchY))) {
        FrontSound("ui_click");
        _SpellPage = 0; _SpellSel = 0;
        s.RespondDialog(0);
    }
    return true;
}

// FIX(sims) QA2: "Gold Thread, Gold Thread, Llama Spit" -> "Gold Thread x2, Llama Spit" (the top list was cut off)
static std::vector<std::string> GroupIngredients(const std::vector<std::string>& in) {
    std::vector<std::string> names; std::vector<int> counts;
    for (auto& i : in) {
        auto it = std::find(names.begin(), names.end(), i);
        if (it == names.end()) { names.push_back(i); counts.push_back(1); } else counts[(size_t)(it - names.begin())]++;
    }
    for (size_t k = 0; k < names.size(); k++) if (counts[k] > 1) names[k] += " x" + std::to_string(counts[k]);
    return names;
}

void SimDialogView::DrawSpellbook(LotSession& s, Canvas& top, Canvas& bottom) {
    auto book = FSO::SimAntics::Engine::Utils::VMTS1Travel::Spellbook(s.VM());
    auto& T = FrontStrings::Get();
    int n = (int)book.size(), pages = std::max(1, (n + 5) / 6);
    _SpellPage = std::max(0, std::min(_SpellPage, pages - 1));
    int onPage = std::max(0, std::min(6, n - _SpellPage * 6));
    _SpellSel = std::max(0, std::min(_SpellSel, std::max(0, onPage - 1)));
    int made = 0;
    for (auto& e : book) if (e.Made) made++;
    // the title is the book's, not the dialog operand's (that can be a stale string of the caller)
    std::string title = "Spellbook";
    auto artFor = [&](int idx) { // STR#3 order: charms (EverAfter Crafter) first, then the wand spells from "Mood Up"
        bool wand = false;
        for (int i = 0; i <= idx && i < n; i++) if (book[(size_t)i].Name == "Mood Up") wand = true;
        return std::string(wand ? "cpanel/Backgrounds/SPELLBOOK_WandCharger.bmp" : "cpanel/Backgrounds/SPELLBOOK_CharmMakerAdult.bmp");
    };
    char buf[64];
    // top: the book (cpanel\Backgrounds\magicbookbkg.bmp): the page's recipes, the made ones ticked
    top.Dim(0, 0, top.Width, top.Height, 150);
    Rect p(6, 6, top.Width - 12, top.Height - 12);
    if (!UIGraphics::Get().DrawImage(top, "cpanel/Backgrounds/magicbookbkg.bmp", p)) top.FillRoundRect(p, 8, RGBA(0, 0, 80));
    TextCentered(top, TS1Size::S14, Rect(p.X, p.Y + 4, p.W, 18), title, RGBA(255, 216, 96), RGBA(0, 0, 30, 220));
    UIGraphics::Get().DrawImage(top, artFor(_SpellPage * 6 + _SpellSel), Rect(p.X + 8, p.Y + 30, 104, 72));
    snprintf(buf, sizeof buf, "%d / %d made", made, n);
    TextCentered(top, TS1Size::S10, Rect(p.X + 8, p.Y + 106, 104, 14), buf, RGBA(200, 220, 255));
    int y = p.Y + 28;
    for (int k = 0; k < onPage; k++) {
        auto& e = book[(size_t)(_SpellPage * 6 + k)];
        int x = p.X + 120, w = p.Right() - x - 8;
        if (k == _SpellSel) top.FillRoundRect(Rect(x - 4, y - 2, w + 6, 32), 4, RGBA(40, 70, 170, 200));
        Text(top, TS1Size::S12, x, y, Fit(TS1Size::S12, (e.Made ? "* " : "  ") + e.Name, w), e.Made ? RGBA(120, 255, 140) : RGBA(255, 255, 255), RGBA(0, 0, 30, 200));
        std::string ing;
        auto grouped = GroupIngredients(e.Ingredients);
        for (size_t i = 0; i < grouped.size(); i++) ing += (i ? ", " : "") + grouped[i];
        // FIX(sims) QA3: a long ingredient list ("..., Pegasu...") drops to the smaller font before it is cut
        // FIX(sims) QA3: a long ingredient list drops to the smaller font before it is cut (two lines do not fit the
        // 33-pixel row); the touch screen shows the selected recipe in full.
        TS1Size ingSize = TextW(TS1Size::S10, ing) <= w - 10 ? TS1Size::S10 : TS1Size::S9;
        Text(top, ingSize, x + 10, y + 15, Fit(ingSize, ing, w - 4), RGBA(190, 210, 255), RGBA(0, 0, 30, 180));
        y += 33;
    }
    // touch screen: the page's names (tap to select) and the selected recipe in full
    bottom.Dim(0, 0, bottom.Width, bottom.Height, 170);
    Rect bp(4, 4, bottom.Width - 8, 232);
    bottom.FillRoundRect(bp, 10, RGBA(14, 22, 96, 255));
    snprintf(buf, sizeof buf, "%s  %d / %d", title.c_str(), _SpellPage + 1, pages);
    TextCentered(bottom, TS1Size::S12, Rect(bp.X, bp.Y + 6, bp.W, 18), buf, RGBA(255, 216, 96), RGBA(0, 0, 30, 220));
    for (int k = 0; k < onPage; k++) {
        auto& e = book[(size_t)(_SpellPage * 6 + k)];
        Rect r(12, kSpellRowY + k * kSpellRowH, 146, kSpellRowH - 2);
        bottom.FillRoundRect(r, 4, k == _SpellSel ? RGBA(40, 90, 190) : RGBA(0, 0, 50));
        if (k == _SpellSel) bottom.StrokeRoundRect(r.X, r.Y, r.W, r.H, 4, RGBA(0, 255, 255));
        Text(bottom, TS1Size::S10, r.X + 4, r.Y + 4, Fit(TS1Size::S10, (e.Made ? "* " : "") + e.Name, r.W - 8), e.Made ? RGBA(120, 255, 140) : RGBA(230, 235, 255));
    }
    if (onPage > 0) {
        auto& e = book[(size_t)(_SpellPage * 6 + _SpellSel)];
        Rect dr(164, 28, 148, 168);
        bottom.FillRoundRect(dr, 6, RGBA(0, 0, 40));
        // FIX(sims) QA2: a long name ("Send Doppelganger To Work", 3 lines) pushed the ingredients over "Not made yet":
        // the picture shrinks to leave room for the text, and equal ingredients are counted ("Gold Thread x2").
        auto nameLines = Wrap(TS1Size::S12, e.Name, dr.W - 8);
        auto ings = GroupIngredients(e.Ingredients);
        int textH = (int)nameLines.size() * 15 + (int)ings.size() * 12;
        int artH = std::max(30, std::min(90, dr.H - 4 - 16 - textH - 2));
        int artW = 130 * artH / 90;
        UIGraphics::Get().DrawImage(bottom, artFor(_SpellPage * 6 + _SpellSel), Rect(dr.X + (dr.W - artW) / 2, dr.Y + 4, artW, artH));
        int dy = dr.Y + 6 + artH;
        for (auto& l : nameLines) { TextCentered(bottom, TS1Size::S12, Rect(dr.X, dy, dr.W, 15), l, RGBA(255, 216, 96)); dy += 15; }
        for (auto& ing : ings) { if (dy + 12 > dr.Bottom() - 14) break; TextCentered(bottom, TS1Size::S10, Rect(dr.X, dy, dr.W, 12), Fit(TS1Size::S10, ing, dr.W - 8), RGBA(200, 220, 255)); dy += 12; }
        TextCentered(bottom, TS1Size::S10, Rect(dr.X, dr.Bottom() - 14, dr.W, 12), e.Made ? "Made by the family" : "Not made yet", e.Made ? RGBA(120, 255, 140) : RGBA(255, 180, 120));
    }
    if (_SpellPage > 0) bottom.FillTriangle(10, 216, 30, 204, 30, 228, RGBA(150, 200, 255));
    if (_SpellPage + 1 < pages) bottom.FillTriangle(310, 216, 290, 204, 290, 228, RGBA(150, 200, 255));
    TextButton ok; ok.Label = T.Get(152, 0, "OK"); ok.R = Rect(120, 202, 80, 28);
    ok.Draw(bottom);
}

bool SimDialogView::Update(LotSession& s, const AppInput& in) {
    if (s.Dialogs.empty() || IsLotSelection(s.Dialogs.front().Type)) return false;
    if (s.Dialogs.front().Type == 6) return UpdateClothes(s, in); // TS1(sims)
    if (s.Dialogs.front().Type == 12) return UpdateSpellbook(s, in); // FIX(sims)
    Layout(s);
    int chosen = -1;
    for (size_t i = 0; i < _Btns.size(); i++) if (_Btns[i].Update(in)) chosen = (int)i;
    // the buttons are rebuilt every frame: a tap is a touch-down inside a button
    if (chosen < 0 && in.TouchDown) for (size_t i = 0; i < _Btns.size(); i++) if (_Btns[i].R.Contains(in.TouchX, in.TouchY)) chosen = (int)i;
    if (chosen < 0 && (in.Down & BTN_A)) chosen = 0;
    if (chosen < 0 && (in.Down & BTN_B)) chosen = (int)_Btns.size() - 1;
    if (chosen >= 0) {
        FrontSound("ui_click");
        s.RespondDialog(_Codes[(size_t)chosen]);
    }
    return true;
}

void SimDialogView::Draw(LotSession& s, LotScreen& lot, Canvas& top, Canvas& bottom) {
    if (s.Dialogs.empty() || IsLotSelection(s.Dialogs.front().Type)) return;
    if (s.Dialogs.front().Type == 6) { DrawClothes(s, top, bottom); return; } // TS1(sims)
    if (s.Dialogs.front().Type == 12) { DrawSpellbook(s, top, bottom); return; } // FIX(sims)
    if (!_TileLoaded) { ResGraphics::Get().Load("Shared/Dlg/Gendlg.bmp", _Tile, false); _TileLoaded = true; }
    auto& d = s.Dialogs.front();
    Layout(s);
    auto& art = FrontArt::Get();
    auto panel = [&](Canvas& c, const Rect& r) {
        // the neighbourhood dialogs' blue tiler, framed with the system dialog tile's bevel (Shared/Dlg/Gendlg.bmp)
        if (art.DialogTiler.Width > 0) { NineSlice(c, art.DialogTiler, r, 14); NineSlice(c, art.DialogTiler, Rect(r.X + 3, r.Y + 3, r.W - 6, r.H - 6), 14); }
        else if (_Tile.Width > 0) NineSlice(c, _Tile, r, 10);
        else c.FillRoundRect(r, 10, RGBA(14, 22, 96, 245));
    };
    // ---- top: the whole dialog, large
    top.Dim(0, 0, top.Width, top.Height, 150);
    std::string title = ToLatin1(d.Title), msg = ToLatin1(d.Message);
    const Canvas* pic = nullptr;
    if (auto vm = s.VM()) {
        auto e = vm->GetObjectById(d.IconID ? d.IconID : d.CallerID);
        if (auto av = dynamic_cast<FSO::SimAntics::VMAvatar*>(e)) pic = lot.FaceFor(av, 0);
        else if (e) pic = lot.IconFor(e);
    }
    int textX = 18, textW = top.Width - 36;
    if (pic) { textX = 82; textW = top.Width - 100; }
    auto lines = Wrap(TS1Size::S12, msg, textW);
    if (lines.size() > 9) { lines.resize(9); lines.back() += "..."; }
    int h = std::max(pic ? 96 : 60, 38 + (int)lines.size() * 16 + 10);
    Rect p(8, std::max(4, (top.Height - h) / 2), top.Width - 16, h);
    panel(top, p);
    top.StrokeRoundRect(p.X + 1, p.Y + 1, p.W - 2, p.H - 2, 8, RGBA(120, 140, 230, 180));
    if (pic) {
        Rect pr(p.X + 12, p.Y + 30, 56, 56);
        top.FillRoundRect(pr, 4, RGBA(0, 0, 30));
        float sc = std::min(52.0f / pic->Width, 52.0f / pic->Height);
        int w = (int)(pic->Width * sc), hh = (int)(pic->Height * sc);
        top.BlitScaled(pic->Px.data(), pic->Width, pic->Height, Rect(pr.X + (pr.W - w) / 2, pr.Y + (pr.H - hh) / 2, w, hh), true);
        top.StrokeRoundRect(pr.X, pr.Y, pr.W, pr.H, 4, RGBA(140, 160, 230));
    }
    TextCentered(top, TS1Size::S14, Rect(p.X, p.Y + 6, p.W, 20), Fit(TS1Size::S14, title, p.W - 20), RGBA(255, 216, 96), RGBA(0, 0, 30, 220));
    int y = p.Y + 32;
    for (auto& l : lines) { Text(top, TS1Size::S12, p.X + textX - 8, y, l, RGBA(255, 255, 255), RGBA(0, 0, 30, 200)); y += 16; }
    // ---- bottom: title + the buttons
    bottom.Dim(0, 0, bottom.Width, bottom.Height, 170);
    Rect bp(8, 26, bottom.Width - 16, 186);
    panel(bottom, bp);
    bottom.StrokeRoundRect(bp.X + 1, bp.Y + 1, bp.W - 2, bp.H - 2, 8, RGBA(120, 140, 230, 180));
    TextCentered(bottom, TS1Size::S12, Rect(bp.X, bp.Y + 8, bp.W, 18), Fit(TS1Size::S12, title.empty() ? std::string("...") : title, bp.W - 16), RGBA(255, 216, 96), RGBA(0, 0, 30, 220));
    // the message again, smaller (the top screen has it large)
    auto small = Wrap(TS1Size::S10, msg, bp.W - 24);
    if (small.size() > 8) { small.resize(8); small.back() += "..."; }
    int sy = bp.Y + 28;
    for (auto& l : small) { Text(bottom, TS1Size::S10, bp.X + 12, sy, l, RGBA(220, 230, 255), RGBA(0, 0, 30, 180)); sy += 13; }
    for (auto& b : _Btns) b.Draw(bottom);
}

bool SimDialogView::ButtonPos(int index, int& x, int& y) const {
    if (index < 0 || index >= (int)_Btns.size()) return false;
    x = _Btns[(size_t)index].R.X + _Btns[(size_t)index].R.W / 2;
    y = _Btns[(size_t)index].R.Y + _Btns[(size_t)index].R.H / 2;
    return true;
}

}}
