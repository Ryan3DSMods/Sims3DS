/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims app — see UIBuildMode.h (FreeSims UI/Panels/UIBuildMode.cs)
#include "app/build/UIBuildMode.h"
#include "app/build/UIWallPlacer.h"
#include "app/build/UIWallPainter.h"
#include "app/build/UIFloorPainter.h"
#include "app/build/UITerrainTools.h"
#include "app/WorldPicking.h"
#include "files/formats/iff/IffFile.h"
#include "files/formats/iff/chunks/STR.h"
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
#include "simantics/VM.h"
#include "simantics/VMContext.h"
#include "simantics/VMArchitecture.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMMultitileGroup.h"
#include "world/World.h"
#include "world/model/Blueprint.h"
#include <cmath>
#include <algorithm>
#include <cstdio>

namespace simapp {

using FSO::SimAntics::Model::VMPlacementError;
using FSO::LotView::Model::LotTilePos;
using FSO::LotView::Model::Direction;

// tool button art and the UIText.iff STR# 139 "BldTips" index of each tool (indexed by Tool)
static const char* ToolArt[12] = { "cpanel/Buttons/toolBtnWall.bmp", "cpanel/Buttons/toolBtnWallpaper.bmp", "cpanel/Buttons/toolBtnFloor.bmp",
    "cpanel/Buttons/toolBtnDoor.bmp", "cpanel/Buttons/toolBtnWindow.bmp", "cpanel/Buttons/toolBtnStairs.bmp",
    "cpanel/Buttons/toolBtnFireplace.bmp", "cpanel/Buttons/toolBtnTree.bmp", "cpanel/Buttons/toolBtnHand.bmp",
    "cpanel/Buttons/toolBtnTerrain.bmp", "cpanel/Buttons/toolBtnWater.bmp", "cpanel/Buttons/toolBtnRoof.bmp" };
static const int ToolTip[12] = { 2, 3, 7, 8, 9, 4, 5, 6, 11, 0, 1, 10 };
static const char* ToolNames[12] = { "Wall & Fence Tool", "Wallpaper Tool", "Floor Tool", "Door Tool", "Window Tool", "Stair Tool",
                                    "Fireplace Tool", "Plant Tool", "Hand Tool", "Terrain Tool", "Water Tool", "Roof Tool" };
// TS1(sims): the order of the build panel (UIText STR# 139: terrain, water, wall, wallpaper, stair, fireplace,
// plant, floor, door, window, roof, hand), two rows of six on the touch screen
static const UIBuildMode::Tool ToolOrder[12] = { UIBuildMode::Tool::Terrain, UIBuildMode::Tool::Pool, UIBuildMode::Tool::Wall,
    UIBuildMode::Tool::Wallpaper, UIBuildMode::Tool::Stairs, UIBuildMode::Tool::Fireplace, UIBuildMode::Tool::Plant,
    UIBuildMode::Tool::Floor, UIBuildMode::Tool::Door, UIBuildMode::Tool::Window, UIBuildMode::Tool::Roof, UIBuildMode::Tool::Hand };

// TS1(sims): GameData/Build.iff strings (STR# 128 "Terrain": price, name, description per tool; 133 "Pool")
static std::string BuildStr(int id, int index, const char* fallback) {
    static std::unique_ptr<FSO::Files::Formats::IFF::IffFile> iff;
    static bool tried = false;
    if (!tried) {
        tried = true;
        auto content = FSO::Content::Content::Get();
        if (content) {
            std::string path = content->SimsCompleteDir + "/GameData/Build.iff";
            if (FILE* f = fopen(path.c_str(), "rb")) {
                fclose(f);
                try { iff.reset(new FSO::Files::Formats::IFF::IffFile(path)); } catch (...) { iff.reset(); }
            }
        }
    }
    if (iff) {
        auto str = iff->Get<FSO::Files::Formats::IFF::Chunks::STR>((uint16_t)id);
        if (str != nullptr && index < str->Length()) { auto r = str->GetString(index); if (!r.empty()) return r; }
    }
    return fallback;
}

UIBuildMode::UIBuildMode(LotScreen* owner) : UIModePanel(owner) {}
UIBuildMode::~UIBuildMode() { Control.reset(); }

void UIBuildMode::Activate() {
    auto content = FSO::Content::Content::Get();
    if (!UICatalog::Get().Ready() && content) UICatalog::Get().Init(content->CacheDir.empty() ? content->SimsCompleteDir : content->CacheDir);
    Refresh();
}

void UIBuildMode::Deactivate() {
    ReleaseControl();
    if (L->Holder->Holding) L->Holder->ClearSelected();
    Selection = -1;
}

void UIBuildMode::ReleaseControl() {
    if (Control) { Control->Release(); Control.reset(); }
    _ActiveElement = nullptr;
    Erase = Shift = false;
    // the architecture preview has to be dropped (paused game: tick the architecture once)
    L->S->Context()->Architecture()->Tick();
}

void UIBuildMode::SetTool(Tool t) {
    ReleaseControl();
    if (L->Holder->Holding) L->Holder->ClearSelected();
    CurrentTool = t;
    Refresh();
}

void UIBuildMode::Refresh() {
    auto& cat = UICatalog::Get();
    Current.clear();
    switch (CurrentTool) {
        case Tool::Wall: Current = cat.WallStyles(); { auto cols = cat.ByBuildType(6); Current.insert(Current.end(), cols.begin(), cols.end()); } break;
        case Tool::Wallpaper: Current = cat.Wallpapers(); break;
        case Tool::Floor: Current = cat.Floors(); break;
        case Tool::Door: Current = cat.ByBuildType(1); break;
        case Tool::Window: Current = cat.ByBuildType(2); break;
        case Tool::Stairs: Current = cat.ByBuildType(3); break;
        case Tool::Fireplace: Current = cat.ByBuildType(5); break;
        case Tool::Plant: Current = cat.ByBuildType(4); break;
        default: break;
    }
    // TS1(sims): sub-tools of the terrain / water / roof tools
    SubTools.clear();
    SubSelection = -1;
    auto sub = [&](const char* art, const char* popup, const char* cur, int strId, int entry, const char* name, int id) {
        SubTool t; t.Art = art; t.Popup = popup; t.Cursor = cur; t.Id = id;
        t.Price = atoi(BuildStr(strId, entry * 3, "0").c_str());
        t.Name = BuildStr(strId, entry * 3 + 1, name);
        t.Desc = BuildStr(strId, entry * 3 + 2, "");
        SubTools.push_back(t);
    };
    if (CurrentTool == Tool::Terrain) {
        sub("cpanel/Build/TerrainUpIcon.BMP", "cpanel/Build/PopupTerrainUp.BMP", "Shared/cursors/SimsRaise.cur", 128, 0, "Raise Terrain", 0);
        sub("cpanel/Build/TerrainDownIcon.BMP", "cpanel/Build/PopupTerrainDown.bmp", "Shared/cursors/SimsLower.cur", 128, 1, "Lower Terrain", 1);
        sub("cpanel/Build/TerrainLevelIcon.BMP", "cpanel/Build/PopupTerrainLevel.bmp", "Shared/cursors/SimsLevel.cur", 128, 2, "Level Terrain", 2);
        if (UIGraphics::Get().Image("cpanel/HDBuild/TerrainGrassIcon.BMP"))   // Hot Date grass tool
            sub("cpanel/HDBuild/TerrainGrassIcon.BMP", "cpanel/HDBuild/popupGrass.bmp", "Shared/cursors/SimsPlace.cur", 128, 3, "Grass", 3);
    } else if (CurrentTool == Tool::Pool) {
        sub("cpanel/Build/PoolToolIcon.bmp", "cpanel/Build/PopupPool.BMP", "Shared/cursors/water.cur", 133, 0, "Pool Tool", 65535);
        if (UIGraphics::Get().Image("cpanel/HDBuild/WaterToolIcon.bmp"))      // Hot Date water tool (ponds)
            sub("cpanel/HDBuild/WaterToolIcon.bmp", "cpanel/HDBuild/PopupWaterTool.bmp", "Shared/cursors/water.cur", 133, 1, "Water Tool", 65534);
    } else if (CurrentTool == Tool::Roof) {
        static const char* pitchArt[4] = { "cpanel/Build/roofshallow.bmp", "cpanel/Build/roofmedium.bmp", "cpanel/Build/roofsteep.bmp", "cpanel/Build/roofflat.bmp" };
        static const char* pitchName[4] = { "Shallow Roof", "Medium Roof", "Steep Roof", "Flat Roof" };
        for (int i = 0; i < 4; i++) { SubTool t; t.Art = pitchArt[i]; t.Popup = "cpanel/Build/popuproofpitch.bmp"; t.Name = pitchName[i]; t.Id = i; SubTools.push_back(t); }
        // the roof is only drawn with walls up (TS1 shows it while the roof tool is open)
        if (L->S->WallsMode != 2) L->S->SetWallsMode(2);
        _RoofPage = 0;
    }
    Page = 0;
    Selection = -1;
    cat.TrimDetails();
}

bool UIBuildMode::ViewDragScrolls() const {
    if (Control || L->Holder->Holding) return false;
    return true; // hand tool / idle: drag scrolls, tap picks up
}

std::string UIBuildMode::CursorArt() const {
    if (Control && SubSelection >= 0 && SubSelection < (int)SubTools.size() && !SubTools[(size_t)SubSelection].Cursor.empty() && !Erase)
        return SubTools[(size_t)SubSelection].Cursor;
    if (Control) return Erase ? "Shared/cursors/SimsDelete.cur" : "Shared/cursors/SimsPlace.cur";
    if (L->Holder->Holding) return "Shared/cursors/SimsPlace.cur";
    return "Shared/cursors/hand.cur";
}

bool UIBuildMode::Select(int index) {
    if (index < 0 || index >= (int)Current.size()) return false;
    auto e = Current[(size_t)index];
    // FIX(sims) SND: C# Catalog_OnSelectionChange: anything (walls, floors, objects) dearer than the budget -> Error
    if ((int)e->Price > L->S->Budget()) {
        PlayUISound(UISounds::Error);
        L->ShowTopTip(GameText::Get().PlacementError((int)VMPlacementError::InsufficientFunds), 200, 100, RGBA(170, 0, 0));
        return false;
    }
    ReleaseControl();
    if (L->Holder->Holding) L->Holder->ClearSelected();
    Selection = index;
    _ActiveElement = e;
    switch (e->Special) {
        case UICatalogElement::SpecialType::WallStyle:
            Control.reset(new UIWallPlacer(L->S, { (int)e->Pattern, (int)e->ResID }));
            break;
        case UICatalogElement::SpecialType::Wallpaper:
            Control.reset(new UIWallPainter(L->S, { (int)e->ResID }));
            break;
        case UICatalogElement::SpecialType::Floor:
            Control.reset(new UIFloorPainter(L->S, { (int)e->ResID }));
            break;
        default: {
            auto group = L->S->VM()->Context->CreateObjectInstance(e->GUID, LotTilePos::OUT_OF_WORLD, Direction::NORTH, true, true);
            if (group == nullptr || group->Objects.empty()) { Selection = -1; return false; }
            L->Holder->SetSelected(group, (int)e->Price, e->GUID);
            L->Holder->Update(L->CursorX, L->CursorY, false);
            break;
        }
    }
    _ActiveElement = e;
    if (L->Log) *L->Log += "build: selected " + UICatalog::Get().GetDetails(*e).Name + "\n";
    return true;
}

bool UIBuildMode::SelectSub(int index) {
    if (index < 0 || index >= (int)SubTools.size()) return false;
    if (CurrentTool == Tool::Roof) {
        static const float pitches[4] = { 0.4f, 0.6f, 1.0f, 0.0f };
        SetRoof(pitches[index], -1);
        return true;
    }
    ReleaseControl();
    if (L->Holder->Holding) L->Holder->ClearSelected();
    SubSelection = index;
    auto& t = SubTools[(size_t)index];
    if (CurrentTool == Tool::Terrain) Control.reset(new UITerrainTool(L->S, (UITerrainTool::Mode)t.Id));
    else if (CurrentTool == Tool::Pool) Control.reset(new UIFloorPainter(L->S, { t.Id }));
    if (L->Log) *L->Log += "build: selected " + t.Name + "\n";
    return true;
}

void UIBuildMode::SetRoof(float pitch, int style) {
    // FreeSO UIRoofer: VMNetSetRoofCmd { Pitch, Style } -> VMArchitecture.SetRoof (TS1: roofs cost nothing)
    auto arch = L->S->Context()->Architecture();
    if (pitch < 0) pitch = arch->RoofPitch;
    uint32_t st = style < 0 ? arch->RoofStyle : (uint32_t)style;
    arch->SetRoof(pitch, st);
    arch->RoofUserSet = true;
    if (L->S->WallsMode != 2) L->S->SetWallsMode(2);
    if (L->Log) { char b[96]; snprintf(b, sizeof b, "build: roof pitch %.2f style %u\n", pitch, st); *L->Log += b; }
}

bool UIBuildMode::Tap(const std::string& id) {
    if (id.rfind("sub", 0) == 0) { SelectSub(atoi(id.c_str() + 3)); return true; }
    if (id.rfind("roofstyle", 0) == 0) { SetRoof(-1.0f, atoi(id.c_str() + 9)); return true; }
    if (id == "roofprev") { if (_RoofPage > 0) _RoofPage--; return true; }
    if (id == "roofnext") { _RoofPage++; return true; }
    if (id.rfind("tool", 0) == 0) {
        Tool t = (Tool)atoi(id.c_str() + 4);
        SetTool(t == CurrentTool && t != Tool::Hand ? Tool::None : t);
        return true;
    }
    if (id == "prev") { if (Page > 0) Page--; UICatalog::Get().TrimDetails(); return true; }
    if (id == "next") { if (Page + 1 < _Pages) Page++; UICatalog::Get().TrimDetails(); return true; }
    if (id.rfind("bitem", 0) == 0) { Select(atoi(id.c_str() + 5)); return true; }
    if (id == "draw_erase") { Erase = !Erase; return true; }
    if (id == "draw_shift") { Shift = !Shift; return true; }
    if (id == "draw_done") { ReleaseControl(); Selection = -1; SubSelection = -1; return true; }
    if (id.rfind("hold_", 0) == 0) {
        bool r = L->Buy->Tap(id);
        if (!L->Holder->Holding) Selection = -1;
        return r;
    }
    if (id == "catalog") { CurrentTool = Tool::None; return true; }
    return false;
}

bool UIBuildMode::Buttons(const AppInput& in) {
    auto& H = *L->Holder;
    if (H.Holding) {
        if (in.Down & BTN_Y) H.Rotate(+1);
        if (in.Down & BTN_X) { H.SellBack(); Selection = -1; }
        if (in.Down & BTN_B) { H.ClearSelected(); Selection = -1; return true; }
        return false;
    }
    if (Control) {
        if (in.Down & BTN_Y) Erase = !Erase;
        if (in.Down & BTN_X) Shift = !Shift;
        if (in.Down & BTN_B) { ReleaseControl(); Selection = -1; SubSelection = -1; return true; }
        return false;
    }
    if (in.Down & BTN_B) { if (CurrentTool != Tool::None) { SetTool(Tool::None); return true; } }
    if ((in.Down & BTN_RIGHT) && Page + 1 < _Pages) Page++;
    if ((in.Down & BTN_LEFT) && Page > 0) Page--;
    return false;
}

void UIBuildMode::PointerUpdate() {
    auto& P = L->P;
    auto& H = *L->Holder;
    auto arch = L->S->Context()->Architecture();
    if (H.Holding) {
        // objects (doors, windows, stairs, ...) are held exactly like buy mode objects
        L->Buy->PointerUpdate();
        if (!H.Holding) Selection = -1;
        return;
    }
    if (Control) {
        ControlState.MouseX = P.X; ControlState.MouseY = P.Y;
        ControlState.CtrlDown = Erase; ControlState.ShiftDown = Shift;
        // TS1(sims): the terrain tool keeps raising while the stylus is held: it needs Update every frame
        if (P.Down) Control->MouseDown(ControlState);
        Control->Update(ControlState, false);
        if (P.Up) {
            Control->MouseUp(ControlState);
            bool placed = false; int cost = 0;
            if (auto w = dynamic_cast<UIWallPlacer*>(Control.get())) { placed = w->LastPlaced; cost = w->LastCost; }
            else if (auto w2 = dynamic_cast<UIWallPainter*>(Control.get())) { placed = w2->LastPlaced; cost = w2->LastCost; }
            else if (auto f = dynamic_cast<UIFloorPainter*>(Control.get())) { placed = f->LastPlaced; cost = f->LastCost; }
            else if (auto t = dynamic_cast<UITerrainTool*>(Control.get())) {
                placed = t->LastPlaced && t->LastFailReason == 0; cost = t->LastCost;
                // TS1(sims): the terrain under walls, floors or objects cannot move
                if (t->LastFailReason != 0 && !t->MarkerError) PlayUISound(UISounds::Error); // FIX(sims) SND: FreeSO UITerrainRaiser ShowErrorAtMouse (unless the preview already did)
                if (t->LastFailReason != 0) L->ShowTopTip(t->LastFailReason == 1 ? "Something is in the way" : GameText::Get().PlacementError((int)VMPlacementError::LocationOutOfBounds), P.X, P.Y - 30, RGBA(170, 0, 0));
            }
            if (placed) {
                if (cost != 0) L->MoneyPopupAt(-cost, P.X, P.Y - 20); // FIX(sims) QA2
                if (L->Log) { char b[64]; snprintf(b, sizeof b, "build: placed, cost %d\n", cost); *L->Log += b; }
            } else if (!ControlState.Tooltip.empty() && ControlState.TooltipError) {
                L->ShowTopTip(GameText::Get().PlacementError((int)VMPlacementError::InsufficientFunds), P.X, P.Y - 30, RGBA(170, 0, 0));
            }
        }
        // ADDED(sims): TS1 pauses the game in build mode; the architecture still has to simulate the preview
        arch->Tick();
        if (P.Up) L->S->UpdateCutaway();
        return;
    }
    if (P.Click) {
        // hand tool (and the idle cursor): pick objects up to move / sell them (UIObjectHolder)
        if (H.PickUpAt(P.X, P.Y)) {
            H.Update(P.X, P.Y, false);
            if (L->Log) *L->Log += "picked up object\n";
        } else if (H.ShowTooltip) {
            L->ShowTopTip(GameText::Get().PlacementError((int)H.LastError), P.X, P.Y - 30, RGBA(170, 0, 0));
            H.ShowTooltip = false;
        }
    }
}

void UIBuildMode::Draw(Canvas& c, const Rect& a) {
    L->DrawPanelFrame(c, a);
    auto& gt = GameText::Get();
    auto& H = *L->Holder;
    if (H.Holding) {
        L->DrawTouchView(c, Rect(a.X + 1, a.Y + 1, a.W - 2, 162));
        UIBuyMode::DrawHoldToolbar(L, c, Rect(a.X, a.Y + 166, a.W, a.H - 166), H.Holding->CanPlace == VMPlacementError::Success);
        return;
    }
    if ((Control && CurrentTool != Tool::Roof) || CurrentTool == Tool::Hand) {
        L->DrawTouchView(c, Rect(a.X + 1, a.Y + 1, a.W - 2, 162));
        Rect tb(a.X, a.Y + 166, a.W, a.H - 166);
        c.FillRect(tb, RGBA(6, 10, 60));
        if (CurrentTool == Tool::Hand) {
            L->DrawArtButton(c, Rect(tb.X + 4, tb.Y + 4, 40, 34), "cpanel/Buttons/toolBtnHand.bmp", 4, true, false, "Hand", "tool8");
            DrawText(c, FontId::Bold, tb.X + 50, tb.Y + 6, gt.UI(139, 11, "Hand Tool"), RGBA(255, 230, 120));
            DrawText(c, FontId::Small, tb.X + 50, tb.Y + 22, "Tap an object to move or sell it.", theme::Text);
            Rect back(tb.Right() - 80, tb.Y + 44, 74, 26);
            c.FillRoundRect(back, 6, theme::ButtonBg);
            DrawTextCentered(c, FontId::Bold, back, "Tools", theme::TextBright);
            L->Hit(back, "catalog");
            return;
        }
        // drawing toolbar: Erase (C# Ctrl), Rectangle / Fill (C# Shift), Done
        bool wall = dynamic_cast<UIWallPlacer*>(Control.get()) != nullptr;
        bool terrain = dynamic_cast<UITerrainTool*>(Control.get()) != nullptr;
        bool pool = CurrentTool == Tool::Pool;
        Rect er(tb.X + 4, tb.Y + 4, 64, 28), sr(tb.X + 72, tb.Y + 4, 72, 28), dr(tb.Right() - 70, tb.Y + 4, 64, 28);
        c.FillRoundRect(er, 6, Erase ? RGBA(160, 50, 40) : theme::ButtonBg);
        if (!terrain) {
            DrawTextCentered(c, FontId::Bold, er, wall || pool ? "Delete" : "Erase", theme::TextBright);
            L->Hit(er, "draw_erase");
        } else c.FillRoundRect(er, 6, RGBA(6, 10, 60));
        if (!pool) {
            c.FillRoundRect(sr, 6, Shift ? RGBA(30, 120, 170) : theme::ButtonBg);
            DrawTextCentered(c, FontId::Bold, sr, wall ? "Room" : (terrain ? "Invert" : "Fill"), theme::TextBright);
            L->Hit(sr, "draw_shift");
        }
        c.FillRoundRect(dr, 6, RGBA(30, 120, 50));
        DrawTextCentered(c, FontId::Bold, dr, "Done", theme::TextBright);
        L->Hit(dr, "draw_done");
        std::string name = _ActiveElement ? UICatalog::Get().GetDetails(*_ActiveElement).Name : std::string("");
        int price = _ActiveElement ? (int)_ActiveElement->Price : 0;
        if (!_ActiveElement && SubSelection >= 0 && SubSelection < (int)SubTools.size()) { name = SubTools[(size_t)SubSelection].Name; price = SubTools[(size_t)SubSelection].Price; }
        char line[160];
        snprintf(line, sizeof line, "%s  %s", name.c_str(), price ? UIUCP::Money(price).c_str() : "");
        DrawText(c, FontId::Small, tb.X + 6, tb.Y + 37, Ellipsize(FontId::Small, line, tb.W - 12), RGBA(255, 230, 120));
        DrawText(c, FontId::Small, tb.X + 6, tb.Y + 51, wall ? "Drag to build walls  (Y delete, X room)" : terrain ? "Tap/drag, hold to continue  (X invert)" : pool ? "Drag to dig  (Y delete)" : "Drag to paint  (Y erase, X fill)", RGBA(120, 130, 200));
        return;
    }
    // ---- tool rows (TS1 order, two rows of six)
    int tw = (a.W - 4) / 6;
    for (int k = 0; k < 12; k++) {
        int i = (int)ToolOrder[k];
        Rect br(a.X + 2 + (k % 6) * tw, a.Y + 2 + (k / 6) * 22, tw - 1, 21);
        bool sel = (int)CurrentTool == i;
        if (sel) c.FillRoundRect(br, 4, RGBA(30, 60, 160));
        if (!UIGraphics::Get().DrawButtonFit(c, ToolArt[i], sel ? ButtonState::Highlight : ButtonState::Normal, br))
            DrawTextCentered(c, FontId::Small, br, Ellipsize(FontId::Small, ToolNames[i], br.W), sel ? RGBA(0, 255, 255) : theme::Text);
        L->Hit(br, "tool" + std::to_string(i));
    }
    int y = a.Y + 47;
    std::string head = CurrentTool == Tool::None ? gt.UI(138, 12, "Build Mode") : gt.UI(139, ToolTip[(int)CurrentTool], ToolNames[(int)CurrentTool]);
    DrawText(c, FontId::Bold, a.X + 6, y, head, RGBA(255, 230, 120));
    y += 16;
    Rect grid(a.X + 14, y, a.W - 28, 46 * 3);
    if (CurrentTool == Tool::None) {
        DrawTextCentered(c, FontId::Small, Rect(grid.X, grid.Y + 30, grid.W, 14), "Pick a tool", theme::Text);
        _Pages = 1;
    } else if (CurrentTool == Tool::Terrain || CurrentTool == Tool::Pool) {
        DrawSubTools(c, grid);
        _Pages = 1;
    } else if (CurrentTool == Tool::Roof) {
        DrawRoofPanel(c, grid);
        _Pages = 1;
    } else {
        _Pages = UIBuyMode::DrawCatalogPage(L, c, grid, Current, Page, Selection, "bitem", L->S->Budget());
        bool prev = Page > 0, next = Page + 1 < _Pages;
        Rect pl(a.X + 1, grid.Y, 12, grid.H), pr(a.Right() - 13, grid.Y, 12, grid.H);
        if (!UIGraphics::Get().DrawButtonFit(c, "cpanel/Buttons/ScrollLeft.BMP", prev ? ButtonState::Normal : ButtonState::Disabled, pl))
            c.FillTriangle(pl.X + 2, pl.Y + pl.H / 2, pl.Right() - 2, pl.Y + pl.H / 2 - 8, pl.Right() - 2, pl.Y + pl.H / 2 + 8, prev ? RGBA(0, 255, 255) : RGBA(50, 60, 100));
        if (!UIGraphics::Get().DrawButtonFit(c, "cpanel/Buttons/ScrollRight.BMP", next ? ButtonState::Normal : ButtonState::Disabled, pr))
            c.FillTriangle(pr.Right() - 2, pr.Y + pr.H / 2, pr.X + 2, pr.Y + pr.H / 2 - 8, pr.X + 2, pr.Y + pr.H / 2 + 8, next ? RGBA(0, 255, 255) : RGBA(50, 60, 100));
        if (prev) L->Hit(pl, "prev");
        if (next) L->Hit(pr, "next");
        char pg[32]; snprintf(pg, sizeof pg, "%d/%d", Page + 1, _Pages);
        DrawText(c, FontId::Small, a.Right() - 8 - TextWidth(FontId::Small, pg), a.Y + 48, pg, theme::TextDim);
    }
    int iy = grid.Bottom() + 2;
    if (a.Bottom() - iy > 14) {
        c.FillRect(a.X + 1, iy, a.W - 2, a.Bottom() - iy - 1, RGBA(4, 8, 50));
        DrawText(c, FontId::Small, a.X + 6, iy + 2, "Pick an item, then draw or place it.  Game paused.", theme::TextDim);
    }
}

// TS1(sims): the sub-tool buttons (4-state strips like catalogue items) with their price
void UIBuildMode::DrawSubTools(Canvas& c, const Rect& grid) {
    int cell = 58;
    for (size_t i = 0; i < SubTools.size(); i++) {
        auto& t = SubTools[i];
        Rect r(grid.X + (int)i * (cell + 6), grid.Y + 4, cell, 46);
        bool sel = (int)i == SubSelection;
        if (!UIGraphics::Get().DrawButtonFit(c, t.Art, sel ? ButtonState::Pressed : ButtonState::Normal, r))
            c.FillRoundRect(r, 6, sel ? RGBA(30, 120, 170) : theme::ButtonBg);
        if (t.Price > 0) DrawTextCentered(c, FontId::Small, Rect(r.X, r.Bottom() + 1, r.W, 12), UIUCP::Money(t.Price), RGBA(120, 255, 120));
        L->Hit(r, "sub" + std::to_string(i));
    }
    int ty = grid.Y + 66;
    if (SubSelection >= 0 && SubSelection < (int)SubTools.size()) {
        auto& t = SubTools[(size_t)SubSelection];
        DrawText(c, FontId::Bold, grid.X, ty, t.Name, RGBA(255, 230, 120));
        const char* hint = CurrentTool == Tool::Pool ? "Drag to dig  (Y: delete)" :
            (t.Id == 2 ? "Drag across to level" : (t.Id == 3 ? "Drag to plant grass" : "Tap or drag; hold to keep going  (X: invert)"));
        DrawText(c, FontId::Small, grid.X, ty + 15, hint, RGBA(120, 130, 200));
    } else {
        DrawText(c, FontId::Small, grid.X, ty, CurrentTool == Tool::Pool ? "Pick the pool or the water tool." : "Pick raise, lower or level.", theme::Text);
    }
}

const Canvas* UIBuildMode::RoofIcon(int style) {
    auto it = _RoofIcons.find(style);
    if (it != _RoofIcons.end()) return it->second.get();
    std::unique_ptr<Canvas> icon;
    auto content = FSO::Content::Content::Get();
    std::string path = content ? content->WorldRoofs->GetPath(style) : std::string();
    if (FILE* f = path.empty() ? nullptr : fopen(path.c_str(), "rb")) {
        std::vector<uint8_t> bytes;
        fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
        if (n > 0 && n < (8 << 20)) { bytes.resize((size_t)n); if (fread(bytes.data(), 1, (size_t)n, f) != (size_t)n) bytes.clear(); }
        fclose(f);
        Canvas tex;
        if (!bytes.empty() && UIGraphics::DecodeBMP(bytes, tex) && tex.Width > 0) {
            icon.reset(new Canvas());
            int side = std::min(tex.Width, tex.Height);
            UIGraphics::Resample(tex, Rect(0, 0, side, side), *icon, 34, 34);
        }
    }
    auto r = icon.get();
    _RoofIcons[style] = std::move(icon);
    return r;
}

// TS1(sims): roof tool — pitch buttons (roofshallow/medium/steep/flat.bmp) and the roof patterns
// (GameData/Roofs/*.bmp drawn into cpanel\Build\roofpatterntemplate.bmp)
void UIBuildMode::DrawRoofPanel(Canvas& c, const Rect& grid) {
    auto arch = L->S->Context()->Architecture();
    static const float pitches[4] = { 0.4f, 0.6f, 1.0f, 0.0f };
    for (size_t i = 0; i < SubTools.size() && i < 4; i++) {
        Rect r(grid.X + (int)i * 62, grid.Y, 58, 22);
        bool sel = std::fabs(arch->RoofPitch - pitches[i]) < 0.05f;
        if (sel) c.FillRoundRect(r, 4, RGBA(30, 60, 160));
        if (!UIGraphics::Get().DrawButtonFit(c, SubTools[i].Art, sel ? ButtonState::Highlight : ButtonState::Normal, r))
            DrawTextCentered(c, FontId::Small, r, SubTools[i].Name, theme::Text);
        L->Hit(r, "sub" + std::to_string(i));
    }
    auto content = FSO::Content::Content::Get();
    int count = content ? content->WorldRoofs->Count() : 0;
    const int perPage = 10;
    int pages = std::max(1, (count + perPage - 1) / perPage);
    if (_RoofPage >= pages) _RoofPage = pages - 1;
    for (int k = 0; k < perPage; k++) {
        int idx = _RoofPage * perPage + k;
        if (idx >= count) break;
        Rect r(grid.X + (k % 5) * 48, grid.Y + 26 + (k / 5) * 46, 44, 44);
        bool sel = (uint32_t)idx == arch->RoofStyle;
        UIGraphics::Get().DrawButtonFit(c, "cpanel/Build/roofpatterntemplate.bmp", sel ? ButtonState::Pressed : ButtonState::Normal, r);
        if (auto icon = RoofIcon(idx)) c.BlitScaled(icon->Px.data(), icon->Width, icon->Height, Rect(r.X + 5, r.Y + 5, r.W - 10, r.H - 10), false);
        if (sel) c.StrokeRoundRect(r.X, r.Y, r.W, r.H, 6, RGBA(0, 255, 255));
        L->Hit(r, "roofstyle" + std::to_string(idx));
    }
    if (pages > 1) {
        Rect pr(grid.Right() - 12, grid.Y + 26, 12, 90), pl(grid.X - 13, grid.Y + 26, 12, 90);
        if (_RoofPage + 1 < pages) { UIGraphics::Get().DrawButtonFit(c, "cpanel/Buttons/ScrollRight.BMP", ButtonState::Normal, pr); L->Hit(pr, "roofnext"); }
        if (_RoofPage > 0) { UIGraphics::Get().DrawButtonFit(c, "cpanel/Buttons/ScrollLeft.BMP", ButtonState::Normal, pl); L->Hit(pl, "roofprev"); }
    }
}

// TS1(sims): the terrain vertex markers (TSO used a cursor object TS1 does not have)
void UIBuildMode::DrawTerrainMarkers(Canvas& top) {
    auto t = dynamic_cast<UITerrainTool*>(Control.get());
    if (t == nullptr || t->MarkerX0 < 0) return;
    auto world = L->S->World();
    auto bp = world->GetBlueprint();
    int lvl = world->State.Level();
    uint32_t col = t->MarkerError ? RGBA(230, 40, 40) : RGBA(255, 255, 255);
    int n = 0;
    for (int y = t->MarkerY0; y <= t->MarkerY1; y++) for (int x = t->MarkerX0; x <= t->MarkerX1; x++) {
        if (++n > 400) return;
        bool edge = (x == t->MarkerX0 || x == t->MarkerX1 || y == t->MarkerY0 || y == t->MarkerY1);
        if (!edge) continue;
        float alt = bp ? bp->InterpAltitude(st::Vector3((float)x, (float)y, 0)) : 0.0f;
        auto sp = TileToScreen(*world, st::Vector3((float)x, (float)y, (lvl - 1) * 2.95f + alt));
        int sx = (int)sp.X, sy = (int)sp.Y;
        top.FillTriangle(sx - 4, sy, sx, sy - 3, sx + 4, sy, RGBA(0, 0, 0, 160));
        top.FillTriangle(sx - 4, sy, sx, sy + 3, sx + 4, sy, RGBA(0, 0, 0, 160));
        top.FillTriangle(sx - 3, sy, sx, sy - 2, sx + 3, sy, col);
        top.FillTriangle(sx - 3, sy, sx, sy + 2, sx + 3, sy, col);
    }
}

void UIBuildMode::DrawTop(Canvas& top) {
    if (L->Holder->Holding) { L->Buy->DrawTop(top); return; }
    if (Control) {
        // cost tooltip at the cursor (C# UIState.Tooltip)
        if (!ControlState.Tooltip.empty()) {
            int w = TextWidth(FontId::Bold, ControlState.Tooltip) + 10;
            int x = std::max(2, std::min(400 - w - 2, L->CursorX + 8)), y = std::max(2, L->CursorY - 26);
            top.FillRoundRect(x, y, w, 17, 4, RGBA(255, 255, 210, 235));
            top.StrokeRoundRect(x, y, w, 17, 4, RGBA(0, 0, 0, 180));
            DrawText(top, FontId::Bold, x + 5, y + 2, ControlState.Tooltip, ControlState.TooltipError ? RGBA(170, 0, 0) : RGBA(0, 0, 0));
        }
        if (_ActiveElement) UIBuyMode::DrawQueryPanel(L, top, _ActiveElement, (int)_ActiveElement->Price, false);
        DrawTerrainMarkers(top);
        if (!_ActiveElement && SubSelection >= 0 && SubSelection < (int)SubTools.size()) {
            // TS1(sims): the sub-tool's popup picture + Build.iff description (TS1 catalogue popups)
            auto& t = SubTools[(size_t)SubSelection];
            Rect p(4, L->CursorY > 150 ? 4 : 240 - 66, 392, 62);
            top.FillRoundRect(p, 6, RGBA(8, 12, 70, 225));
            top.StrokeRoundRect(p.X, p.Y, p.W, p.H, 6, RGBA(140, 150, 230));
            UIGraphics::Get().DrawImage(top, t.Popup, Rect(p.X + 4, p.Y + 4, 75, 54));
            int tx = p.X + 84;
            std::string head = t.Name + (t.Price > 0 ? "   " + UIUCP::Money(t.Price) : std::string());
            DrawText(top, FontId::Bold, tx, p.Y + 3, Ellipsize(FontId::Bold, head, p.Right() - tx - 6), RGBA(255, 230, 120));
            auto lines = WrapText(FontId::Small, t.Desc.substr(0, t.Desc.find("\r")), p.Right() - tx - 6);
            int yy = p.Y + 18;
            for (size_t i = 0; i < lines.size() && i < 3; i++) { DrawText(top, FontId::Small, tx, yy, lines[i], theme::TextBright); yy += LineHeight(FontId::Small); }
        }
        return;
    }
    DrawText(top, FontId::Bold, 6, 4, GameText::Get().UI(138, 12, "Build Mode"), RGBA(255, 255, 255));
}

}
