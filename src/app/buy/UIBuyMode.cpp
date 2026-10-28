/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims app — see UIBuyMode.h (FreeSims UI/Panels/UIBuyMode.cs, UI/Controls/Catalog/UICatalogItem.cs)
#include "app/buy/UIBuyMode.h"
#include "app/buy/UIObjectHolder.h"
#include "app/UISounds.h"
#include "app/LotScreen.h"
#include "app/Font.h"
#include "app/Theme.h"
#include "app/ui/UIGraphics.h"
#include "app/ui/GameText.h"
#include "app/ui/UIUCP.h"
#include "content/ContentManager.h"
#include "content/TS1NeighbourProvider.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMMultitileGroup.h"
#include "world/World.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace simapp {

using FSO::SimAntics::Model::VMPlacementError;
using FSO::LotView::Model::LotTilePos;
using FSO::LotView::Model::Direction;

static const char* FunctionArt[8] = { "BuyFSeating.BMP", "BuyFSurfaces.bmp", "BuyFDecorative.bmp", "BuyFElectronics.bmp",
                                      "BuyFAppliances.bmp", "BuyFPlumbing.bmp", "BuyFLighting.bmp", "BuyFMisc.bmp" };
static const char* RoomArt[8] = { "BuyRLiving.BMP", "BuyRDining.bmp", "BuyRBedroom.bmp", "BuyRStudy.bmp",
                                  "BuyRKitchen.bmp", "BuyRBathroom.bmp", "BuyROutside.bmp", "BuyRMisc.bmp" };
static const char* SubsortFolder[8] = { "Seating", "Surfaces", "Decorative", "Electronics", "Appliances", "Plumbing", "Lighting", "Miscellaneous" };
static const char* SubsortFile[4] = { "BuySubSortOne.BMP", "BuySubSortTwo.bmp", "BuySubSortThree.BMP", "BuySubSortFour.BMP" };
static const char* FunctionNames[8] = { "Seating", "Surfaces", "Decorative", "Electronics", "Appliances", "Plumbing", "Lighting", "Miscellaneous" };
static const char* RoomNames[8] = { "Living Room", "Dining Room", "Bedroom", "Study", "Kitchen", "Bathroom", "Outside", "Miscellaneous" };

UIBuyMode::UIBuyMode(LotScreen* owner) : UIModePanel(owner) {}

void UIBuyMode::Activate() {
    auto content = FSO::Content::Content::Get();
    if (!UICatalog::Get().Ready() && content) UICatalog::Get().Init(content->CacheDir.empty() ? content->SimsCompleteDir : content->CacheDir);
    HandView = false;
    Refresh();
}

void UIBuyMode::Deactivate() {
    // C# Destroy: delete the object that hasn't been placed yet
    if (L->Holder->Holding) L->Holder->ClearSelected();
    Selection = -1;
    HandView = false;
}

void UIBuyMode::Refresh() {
    auto& cat = UICatalog::Get();
    // FIX(sims): residential vs community catalogue (UICatalog::CommunityLot)
    {
        auto content = FSO::Content::Content::Get();
        cat.CommunityLot = content && content->Neighborhood && L && L->S && content->Neighborhood->GetZoning((int16_t)L->S->HouseNumber) != 0;
    }
    Current.clear();
    if (Category >= 0) Current = RoomCategories ? cat.ByRoom(Category) : cat.ByFunction(Category, Subsort);
    Page = 0;
    Selection = -1;
    cat.TrimDetails();
}

bool UIBuyMode::ViewDragScrolls() const { return L->Holder->Holding == nullptr; }

std::string UIBuyMode::CursorArt() const {
    if (L->Holder->Holding) return "Shared/cursors/SimsPlace.cur";
    return "Shared/cursors/hand.cur";
}

int UIBuyMode::PageSize(const Rect& r) {
    int cols = std::max(1, r.W / 40), rows = std::max(1, r.H / 46);
    return cols * rows;
}

int UIBuyMode::DrawCatalogPage(LotScreen* L, Canvas& c, const Rect& r, const std::vector<const UICatalogElement*>& items,
                               int page, int selected, const std::string& prefix, int budget) {
    auto& cat = UICatalog::Get();
    int cols = std::max(1, r.W / 40), rows = std::max(1, r.H / 46);
    int per = cols * rows;
    int pages = std::max(1, ((int)items.size() + per - 1) / per);
    int x0 = r.X + (r.W - cols * 40) / 2;
    for (int i = 0; i < per; i++) {
        int idx = page * per + i;
        if (idx >= (int)items.size()) break;
        auto e = items[(size_t)idx];
        Rect cell(x0 + (i % cols) * 40, r.Y + (i / cols) * 46, 38, 44);
        bool sel = idx == selected;
        bool afford = (int)e->Price <= budget;
        // UICatalogItem: the TS1 thumbnail frame (cpanel\Catalog\ThumbTemplate.BMP, 4 states)
        Rect thumb(cell.X + 1, cell.Y, 36, 36);
        if (!UIGraphics::Get().DrawButton(c, "cpanel/Catalog/ThumbTemplate.BMP", sel ? ButtonState::Highlight : (afford ? ButtonState::Normal : ButtonState::Disabled), thumb)) {
            c.FillRect(thumb, RGBA(10, 14, 60));
            c.StrokeRect(thumb.X, thumb.Y, thumb.W, thumb.H, sel ? RGBA(0, 255, 255) : RGBA(80, 90, 170));
        }
        auto& d = cat.GetDetails(*e);
        if (d.Icon.Width > 0) {
            const Canvas& ic = (sel && d.IconActive.Width > 0) ? d.IconActive : d.Icon; // FIX(sims) FB1: one frame
            float sc = std::min(34.0f / ic.Width, 34.0f / ic.Height);
            if (sc > 1.0f && e->Special == UICatalogElement::SpecialType::None) sc = 1.0f;
            int w = std::max(1, (int)(ic.Width * sc)), h = std::max(1, (int)(ic.Height * sc));
            c.BlitScaled(ic.Px.data(), ic.Width, ic.Height, Rect(thumb.X + (36 - w) / 2, thumb.Y + (36 - h) / 2, w, h), true);
        } else {
            DrawTextCentered(c, FontId::Small, thumb, Ellipsize(FontId::Small, d.Name, 34), theme::TextDim);
        }
        if (!afford) c.Dim(thumb.X, thumb.Y, thumb.W, thumb.H, 140);
        if (sel) c.StrokeRect(thumb.X - 1, thumb.Y - 1, thumb.W + 2, thumb.H + 2, RGBA(0, 255, 255));
        char price[24]; snprintf(price, sizeof price, "\xA7%u", e->Price);
        DrawTextCentered(c, FontId::Small, Rect(cell.X - 1, cell.Y + 36, cell.W + 2, 10), price, afford ? RGBA(120, 255, 120) : RGBA(255, 90, 80));
        L->Hit(cell, prefix + std::to_string(idx));
    }
    if (items.empty()) DrawTextCentered(c, FontId::Small, r, "", theme::TextDim);
    return pages;
}

void UIBuyMode::DrawHoldToolbar(LotScreen* L, Canvas& c, const Rect& r, bool canPlace) {
    auto& H = *L->Holder;
    c.FillRect(r, RGBA(6, 10, 60));
    int x = r.X + 4, y = r.Y + 3;
    L->DrawArtButton(c, Rect(x, y, 30, 30), "cpanel/Buttons/rotleft.bmp", 4, false, false, "<", "hold_rotl");
    L->DrawArtButton(c, Rect(x + 32, y, 30, 30), "cpanel/Buttons/rotright.bmp", 4, false, false, ">", "hold_rotr");
    // place (TS1 has no button: the click places; here a big check mark too)
    Rect pr(x + 68, y, 60, 30);
    c.FillRoundRect(pr, 6, canPlace ? RGBA(30, 140, 50) : RGBA(50, 60, 80));
    c.StrokeRoundRect(pr.X, pr.Y, pr.W, pr.H, 6, canPlace ? RGBA(150, 255, 150) : RGBA(100, 110, 140));
    bool bought = H.Holding && H.Holding->IsBought();
    DrawTextCentered(c, FontId::Bold, pr, bought ? "Place" : "Buy", canPlace ? RGBA(255, 255, 255) : theme::TextDim);
    L->Hit(pr, "hold_place");
    // sell back / cancel (C# SellBack: Delete key; the TS1 hand tool's sell button)
    Rect sr(x + 132, y, 60, 30);
    c.FillRoundRect(sr, 6, bought ? RGBA(150, 40, 30) : RGBA(70, 50, 90));
    c.StrokeRoundRect(sr.X, sr.Y, sr.W, sr.H, 6, RGBA(255, 160, 140));
    DrawTextCentered(c, FontId::Bold, sr, bought ? "Sell" : "Cancel", RGBA(255, 255, 255));
    L->Hit(sr, "hold_sell");
    Rect cr(x + 196, y, r.Right() - x - 200, 30);
    if (bought) {
        c.FillRoundRect(cr, 6, RGBA(40, 50, 110));
        DrawTextCentered(c, FontId::Small, cr, "Put back", theme::TextBright);
        L->Hit(cr, "hold_cancel");
    }
    // status line: name / price / error
    std::string status;
    uint32_t col = theme::TextBright;
    if (H.Holding) {
        if (H.Holding->CanPlace != VMPlacementError::Success) {
            status = GameText::Get().PlacementError((int)H.Holding->CanPlace);
            col = RGBA(255, 110, 90);
        } else if (bought) {
            auto obj = L->S->VM()->GetObjectById(H.Holding->MoveTarget);
            status = "Sells for " + UIUCP::Money(H.SaleValue(obj));
        } else {
            status = UIUCP::Money(H.Holding->Price) + "  -  tap again or A to buy";
        }
    }
    DrawText(c, FontId::Small, r.X + 6, r.Y + 37, Ellipsize(FontId::Small, status, r.W - 12), col);
    DrawText(c, FontId::Small, r.X + 6, r.Y + 51, "Y / buttons: rotate   B: cancel   X: sell", RGBA(120, 130, 200));
}

void UIBuyMode::DrawQueryPanel(LotScreen* L, Canvas& top, const UICatalogElement* e, int price, bool sell) {
    // TS1 query panel (cpanel\Backgrounds\PopupInfo.BMP is its frame): icon, name, price, description, ratings
    if (e == nullptr) return;
    auto& d = UICatalog::Get().GetDetails(*e);
    // bottom of the top screen, or its top while the cursor / ghost is down there
    Rect p(4, L->CursorY > 150 ? 4 : 240 - 66, 392, 62);
    top.FillRoundRect(p, 6, RGBA(8, 12, 70, 225));
    top.StrokeRoundRect(p.X, p.Y, p.W, p.H, 6, RGBA(140, 150, 230));
    int tx = p.X + 6;
    if (d.Icon.Width > 0) {
        top.FillRect(p.X + 5, p.Y + 5, 40, 40, RGBA(0, 0, 30));
        float sc = std::min(36.0f / d.Icon.Width, 36.0f / d.Icon.Height);
        if (sc > 1.0f && e->Special == UICatalogElement::SpecialType::None) sc = 1.0f;
        int w = (int)(d.Icon.Width * sc), h = (int)(d.Icon.Height * sc);
        top.BlitScaled(d.Icon.Px.data(), d.Icon.Width, d.Icon.Height, Rect(p.X + 5 + (40 - w) / 2, p.Y + 5 + (40 - h) / 2, w, h), true);
        tx = p.X + 50;
    }
    std::string head = d.Name + "   " + (sell ? "Sells for " : "") + UIUCP::Money(price);
    DrawText(top, FontId::Bold, tx, p.Y + 3, Ellipsize(FontId::Bold, head, p.Right() - tx - 6), RGBA(255, 230, 120));
    int lh = LineHeight(FontId::Small);
    auto lines = WrapText(FontId::Small, d.Description, p.Right() - tx - 6);
    int y = p.Y + 18;
    for (size_t i = 0; i < lines.size() && i < 2; i++) { DrawText(top, FontId::Small, tx, y, lines[i], theme::TextBright); y += lh; }
    // motive ratings (UIText STR# 160 "Hunger: %d" ...), skill flags
    std::string rat;
    auto& gt = GameText::Get();
    static const char* fmts[7] = { "Hunger: %d", "Comfort: %d", "Hygiene: %d", "Bladder: %d", "Energy: %d", "Fun: %d", "Room: %d" };
    for (int i = 0; i < 7; i++) {
        if (e->Ratings[i] == 0) continue;
        char b[48]; snprintf(b, sizeof b, gt.UI(160, i, fmts[i], false).c_str(), e->Ratings[i]);
        if (!rat.empty()) rat += "  ";
        rat += b;
    }
    static const char* skills[7] = { "+ Cooking", "+ Mechanical", "+ Logic", "+ Body", "+ Creativity", "+ Charisma", "+ Study" };
    for (int i = 0; i < 7; i++) if (e->SkillFlags & (1 << i)) { if (!rat.empty()) rat += "  "; rat += gt.UI(160, 7 + i, skills[i], false); }
    if (!rat.empty()) DrawText(top, FontId::Small, tx, p.Bottom() - lh - 2, Ellipsize(FontId::Small, rat, p.Right() - tx - 6), RGBA(120, 255, 120));
}

void UIBuyMode::Draw(Canvas& c, const Rect& a) {
    L->DrawPanelFrame(c, a);
    auto& H = *L->Holder;
    auto& gt = GameText::Get();
    if (H.Holding || HandView) {
        L->DrawTouchView(c, Rect(a.X + 1, a.Y + 1, a.W - 2, 162));
        if (H.Holding) DrawHoldToolbar(L, c, Rect(a.X, a.Y + 166, a.W, a.H - 166), H.Holding->CanPlace == VMPlacementError::Success);
        else {
            c.FillRect(a.X, a.Y + 166, a.W, a.H - 166, RGBA(6, 10, 60));
            L->DrawArtButton(c, Rect(a.X + 4, a.Y + 170, 40, 34), "cpanel/Buttons/toolBtnHand.bmp", 4, true, false, "Hand", "hand");
            DrawText(c, FontId::Bold, a.X + 50, a.Y + 172, gt.UI(139, 11, "Hand Tool"), RGBA(255, 230, 120));
            DrawText(c, FontId::Small, a.X + 50, a.Y + 188, "Tap an object to move or sell it.", theme::Text);
            Rect back(a.Right() - 80, a.Y + 208, 74, 26);
            c.FillRoundRect(back, 6, theme::ButtonBg);
            DrawTextCentered(c, FontId::Bold, back, "Catalog", theme::TextBright);
            L->Hit(back, "catalog");
        }
        return;
    }
    // ---- category row: sort toggle + 8 categories (TS1 buy mode buttons)
    Rect tog1(a.X + 3, a.Y + 2, 28, 15), tog2(a.X + 3, a.Y + 18, 28, 15);
    c.FillRoundRect(tog1, 3, !RoomCategories ? RGBA(30, 60, 160) : RGBA(14, 22, 90));
    c.FillRoundRect(tog2, 3, RoomCategories ? RGBA(30, 60, 160) : RGBA(14, 22, 90));
    DrawTextCentered(c, FontId::Small, tog1, "Fn", !RoomCategories ? RGBA(0, 255, 255) : theme::Text);
    DrawTextCentered(c, FontId::Small, tog2, "Rm", RoomCategories ? RGBA(0, 255, 255) : theme::Text);
    L->Hit(tog1, "sort_function"); L->Hit(tog2, "sort_room");
    int cw = (a.W - 36) / 8;
    for (int i = 0; i < 8; i++) {
        Rect br(a.X + 34 + i * cw, a.Y + 2, cw - 1, 31);
        std::string art = std::string("cpanel/Catalog/") + (RoomCategories ? RoomArt[i] : FunctionArt[i]);
        bool sel = Category == i;
        if (sel) c.FillRoundRect(br, 4, RGBA(30, 60, 160));
        if (!UIGraphics::Get().DrawButtonFit(c, art, sel ? ButtonState::Highlight : ButtonState::Normal, br))
            DrawTextCentered(c, FontId::Small, br, Ellipsize(FontId::Small, RoomCategories ? RoomNames[i] : FunctionNames[i], br.W), sel ? RGBA(0, 255, 255) : theme::Text);
        L->Hit(br, "cat" + std::to_string(i));
    }
    // ---- sub-sort row (function sort only): All, the four sub-sorts, Other (UIText STR# 200-207, 210)
    int y = a.Y + 35;
    if (Category >= 0 && !RoomCategories) {
        int sw = (a.W - 50) / 6;
        for (int i = -1; i <= 4; i++) {
            Rect sr(a.X + 2 + (i + 1) * sw, y, sw - 2, 18);
            bool sel = Subsort == i;
            std::string label = (i == -1) ? gt.UI(210, 2, "All") : (i == 4 ? gt.UI(210, 1, "Other") : gt.UI(200 + Category, i, "?"));
            c.FillRoundRect(sr, 3, sel ? RGBA(30, 60, 160) : RGBA(14, 22, 90));
            std::string art = (i == -1) ? "cpanel/Catalog/SubSortIcons/BuySubSortAll.bmp" : (i == 4 ? "cpanel/Catalog/SubSortIcons/BuySubSortOther.bmp"
                : std::string("cpanel/Catalog/SubSortIcons/") + SubsortFolder[Category] + "/" + SubsortFile[i]);
            Rect ir(sr.X + 1, sr.Y + 1, 16, 16);
            bool art_ok = UIGraphics::Get().DrawButtonFit(c, art, sel ? ButtonState::Highlight : ButtonState::Normal, ir);
            if (art_ok) ir = Rect(sr.X + (sr.W - 16) / 2, sr.Y + 1, 16, 16);
            if (art_ok) { c.FillRoundRect(sr, 3, sel ? RGBA(30, 60, 160) : RGBA(14, 22, 90)); UIGraphics::Get().DrawButtonFit(c, art, sel ? ButtonState::Highlight : ButtonState::Normal, ir); }
            else DrawText(c, FontId::Small, sr.X + 2, sr.Y + 3, Ellipsize(FontId::Small, label, sr.W - 4), sel ? RGBA(0, 255, 255) : theme::Text);
            L->Hit(sr, "sub" + std::to_string(i));
        }
        // the hand tool (TS1 buy mode also has it)
    }
    L->DrawArtButton(c, Rect(a.Right() - 44, y - 1, 42, 20), "cpanel/Buttons/toolBtnHand.bmp", 4, false, false, "Hand", "hand");
    y += 21;
    // ---- catalogue page (prev/next arrows: cpanel\Buttons\ScrollLeftBuy.BMP / ScrollRight.BMP)
    Rect grid(a.X + 14, y, a.W - 28, 46 * 3);
    _GridRect = grid;
    if (Category < 0) {
        DrawTextCentered(c, FontId::Bold, Rect(grid.X, grid.Y + 40, grid.W, 16), RoomCategories ? "Pick a room" : "Pick a category", theme::Text);
        DrawTextCentered(c, FontId::Small, Rect(grid.X, grid.Y + 60, grid.W, 14), UIUCP::Money(L->S->Budget()) + " to spend", RGBA(120, 255, 120));
    } else {
        _Pages = DrawCatalogPage(L, c, grid, Current, Page, Selection, "item", L->S->Budget());
        bool prev = Page > 0, next = Page + 1 < _Pages;
        Rect pl(a.X + 1, grid.Y, 12, grid.H), pr(a.Right() - 13, grid.Y, 12, grid.H);
        if (!UIGraphics::Get().DrawButtonFit(c, "cpanel/Buttons/ScrollLeftBuy.BMP", prev ? ButtonState::Normal : ButtonState::Disabled, pl))
            c.FillTriangle(pl.X + 2, pl.Y + pl.H / 2, pl.Right() - 2, pl.Y + pl.H / 2 - 8, pl.Right() - 2, pl.Y + pl.H / 2 + 8, prev ? RGBA(0, 255, 255) : RGBA(50, 60, 100));
        if (!UIGraphics::Get().DrawButtonFit(c, "cpanel/Buttons/ScrollRight.BMP", next ? ButtonState::Normal : ButtonState::Disabled, pr))
            c.FillTriangle(pr.Right() - 2, pr.Y + pr.H / 2, pr.X + 2, pr.Y + pr.H / 2 - 8, pr.X + 2, pr.Y + pr.H / 2 + 8, next ? RGBA(0, 255, 255) : RGBA(50, 60, 100));
        if (prev) L->Hit(pl, "prev");
        if (next) L->Hit(pr, "next");
    }
    // ---- info line
    int iy = grid.Bottom() + 2;
    c.FillRect(a.X + 1, iy, a.W - 2, a.Bottom() - iy - 1, RGBA(4, 8, 50));
    std::string head = Category < 0 ? std::string(RoomCategories ? "Buy by room" : "Buy by function")
                                     : (RoomCategories ? gt.UI(150, Category, RoomNames[Category]) : gt.UI(150, 8 + Category, FunctionNames[Category]));
    char pg[32]; snprintf(pg, sizeof pg, "%d/%d", Page + 1, _Pages);
    DrawText(c, FontId::Bold, a.X + 6, iy + 2, head, RGBA(255, 230, 120));
    if (Category >= 0) DrawText(c, FontId::Small, a.Right() - 8 - TextWidth(FontId::Small, pg), iy + 3, pg, theme::TextDim);
    DrawText(c, FontId::Small, a.X + 6, iy + 17, Category < 0 ? "Tap an item to pick it up, then place it on the lot." : (std::to_string(Current.size()) + " items, cheapest first"), theme::Text);
}

bool UIBuyMode::Select(int index) {
    if (index < 0 || index >= (int)Current.size()) return false;
    auto item = Current[(size_t)index];
    if ((int)item->Price > L->S->Budget()) {
        PlayUISound(UISounds::Error); // FIX(sims) SND: C# Catalog_OnSelectionChange
        L->ShowTopTip(GameText::Get().PlacementError((int)VMPlacementError::InsufficientFunds), 200, 100, RGBA(170, 0, 0));
        return false;
    }
    auto vm = L->S->VM();
    auto group = vm->Context->CreateObjectInstance(item->GUID, LotTilePos::OUT_OF_WORLD, Direction::NORTH, true, true);
    if (group == nullptr || group->Objects.empty()) { if (L->Log) *L->Log += "buy: could not create object\n"; return false; }
    L->Holder->SetSelected(group, (int)item->Price, item->GUID);
    Selection = index;
    _TapPlaceArmed = false; _LastTileX = _LastTileY = -9999;
    // start where the cursor is (centre of the view if it was never moved)
    L->Holder->Update(L->CursorX, L->CursorY, false);
    if (L->Log) *L->Log += "holding " + UICatalog::Get().GetDetails(*item).Name + "\n";
    return true;
}

bool UIBuyMode::Tap(const std::string& id) {
    auto& H = *L->Holder;
    if (id == "sort_function" || id == "sort_room") {
        bool room = id == "sort_room";
        if (room != RoomCategories) { RoomCategories = room; Category = -1; Subsort = -1; Refresh(); }
        return true;
    }
    if (id.rfind("cat", 0) == 0) { Category = atoi(id.c_str() + 3); Subsort = -1; Refresh(); return true; }
    if (id.rfind("sub", 0) == 0) { Subsort = atoi(id.c_str() + 3); Refresh(); return true; }
    if (id == "prev") { if (Page > 0) Page--; UICatalog::Get().TrimDetails(); return true; }
    if (id == "next") { if (Page + 1 < _Pages) Page++; UICatalog::Get().TrimDetails(); return true; }
    if (id.rfind("item", 0) == 0) { Select(atoi(id.c_str() + 4)); return true; }
    if (id == "hand") { HandView = !HandView; return true; }
    if (id == "catalog") { HandView = false; return true; }
    if (id == "hold_rotl") { H.Rotate(-1); return true; }
    if (id == "hold_rotr") { H.Rotate(+1); return true; }
    if (id == "hold_place") {
        if (!H.PutDown()) L->ShowTopTip(GameText::Get().PlacementError((int)H.LastError), L->CursorX, L->CursorY - 30, RGBA(170, 0, 0));
        else { Selection = -1; if (L->Log) *L->Log += "placed\n"; }
        return true;
    }
    if (id == "hold_sell") { H.SellBack(); Selection = -1; return true; }
    if (id == "hold_cancel") { H.ClearSelected(); Selection = -1; return true; }
    return false;
}

bool UIBuyMode::Buttons(const AppInput& in) {
    auto& H = *L->Holder;
    if (H.Holding) {
        if (in.Down & BTN_Y) H.Rotate(+1);
        if (in.Down & BTN_X) { H.SellBack(); Selection = -1; }
        if (in.Down & BTN_B) { H.ClearSelected(); Selection = -1; return true; } // C#: Escape
        return false;
    }
    if (in.Down & BTN_B) { if (HandView) { HandView = false; return true; } if (Category >= 0) { Category = -1; Refresh(); return true; } }
    if ((in.Down & BTN_RIGHT) && Page + 1 < _Pages) Page++;
    if ((in.Down & BTN_LEFT) && Page > 0) Page--;
    return false;
}

void UIBuyMode::PointerUpdate() {
    auto& P = L->P;
    auto& H = *L->Holder;
    if (!P.Active) return;
    if (H.Holding) {
        if (P.FromTouch) {
            // ADDED(sims): the stylus is the hover; a second tap on the ghost's tile is the click
            auto tile = L->S->World()->State.WorldSpace.GetTileAtPosWithScroll(st::Vector2((float)P.X, (float)P.Y));
            int tx = (int)std::floor(tile.X + H.Holding->TilePosOffset.X), ty = (int)std::floor(tile.Y + H.Holding->TilePosOffset.Y);
            if (P.Down) _TapPlaceArmed = (tx == _LastTileX && ty == _LastTileY);
            if (P.Held || P.Down) {
                if (tx != _LastTileX || ty != _LastTileY) { if (!P.Down) _TapPlaceArmed = false; }
                H.Update(P.X, P.Y, false);
            }
            if (P.Up) {
                if (_TapPlaceArmed && tx == _LastTileX && ty == _LastTileY) Tap("hold_place");
                _LastTileX = tx; _LastTileY = ty;
                _TapPlaceArmed = false;
            }
        } else {
            // cursor + A = the mouse (C# MouseDown / Update / MouseUp: hold A and move to rotate)
            if (P.Down) H.MouseDown(P.X, P.Y);
            H.Update(P.X, P.Y, P.Held);
            if (P.Up) {
                bool had = H.Holding != nullptr;
                H.MouseUp(P.X, P.Y);
                if (had && H.Holding) L->ShowTopTip(GameText::Get().PlacementError((int)H.LastError), P.X, P.Y - 30, RGBA(170, 0, 0));
                if (!H.Holding) Selection = -1;
            }
        }
        return;
    }
    if (P.Click) {
        // hand tool: pick up what is under the pointer (move / sell)
        if (H.PickUpAt(P.X, P.Y)) {
            auto tile = L->S->World()->State.WorldSpace.GetTileAtPosWithScroll(st::Vector2((float)P.X, (float)P.Y));
            _LastTileX = (int)std::floor(tile.X + H.Holding->TilePosOffset.X); _LastTileY = (int)std::floor(tile.Y + H.Holding->TilePosOffset.Y);
            H.Update(P.X, P.Y, false);
            if (L->Log) *L->Log += "picked up object\n";
        } else if (H.ShowTooltip) {
            L->ShowTopTip(GameText::Get().PlacementError((int)H.LastError), P.X, P.Y - 30, RGBA(170, 0, 0));
            H.ShowTooltip = false;
        }
    }
}

void UIBuyMode::DrawTop(Canvas& top) {
    auto& H = *L->Holder;
    if (H.Holding) {
        auto e = UICatalog::Get().ByGUID(H.Holding->GUID);
        bool sell = H.Holding->IsBought();
        int price = sell ? H.SaleValue(L->S->VM()->GetObjectById(H.Holding->MoveTarget)) : H.Holding->Price;
        DrawQueryPanel(L, top, e, price, sell);
        if (H.Holding->CanPlace != VMPlacementError::Success) {
            std::string err = GameText::Get().PlacementError((int)H.Holding->CanPlace);
            int w = TextWidth(FontId::Bold, err) + 10;
            int x = std::max(2, std::min(400 - w - 2, L->CursorX - w / 2)), y = std::max(2, L->CursorY - 40);
            top.FillRoundRect(x, y, w, 17, 4, RGBA(255, 255, 210, 235));
            DrawText(top, FontId::Bold, x + 5, y + 2, err, RGBA(170, 0, 0));
        }
    } else {
        DrawText(top, FontId::Bold, 6, 4, GameText::Get().UI(138, 11, "Buy Mode"), RGBA(255, 255, 255));
    }
}

}
