/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
#include <algorithm>
// sims app — see UIOptionsPanel.h
#include "app/ui/UIOptionsPanel.h"
#include "app/LotScreen.h"
#include "app/Font.h"
#include "app/Theme.h"
#include "app/ui/UIGraphics.h"
#include "app/ui/GameText.h"
#include "world/World.h"
#include "app/front/GameOptions.h"
#include <cstdio>

namespace simapp {

void UIOptionsPanel::Draw(Canvas& c, const Rect& a) {
    L->DrawPanelFrame(c, a);
    auto& gt = GameText::Get();
    auto& s = *L->S;
    DrawText(c, FontId::Bold, a.X + 8, a.Y + 4, gt.UI(138, 13, "Options"), RGBA(255, 230, 120));
    // TS1(sims): the cheat console (PC: Ctrl+Shift+C; on the console also L+R+SELECT), see app/ui/UICheatConsole.h
    L->DrawArtButton(c, Rect(a.Right() - 58, a.Y + 3, 52, 16), "", 1, false, false, "Cheat", "opt_cheat");

    // ---- view controls (TS1 camera panel)
    int x0 = a.X + 8, y = a.Y + 24;
    DrawText(c, FontId::Small, x0, y, "View", RGBA(150, 170, 255));
    y += 14;
    L->DrawArtButton(c, Rect(x0, y, 30, 30), "cpanel/Buttons/rotleft.bmp", 4, false, false, "<", "opt_rotl");
    L->DrawArtButton(c, Rect(x0 + 34, y, 30, 30), "cpanel/Buttons/rotright.bmp", 4, false, false, ">", "opt_rotr");
    L->DrawArtButton(c, Rect(x0 + 74, y, 30, 30), "cpanel/Buttons/zoomin.bmp", 4, false, false, "+", "opt_zin");
    L->DrawArtButton(c, Rect(x0 + 108, y, 30, 30), "cpanel/Buttons/zoomout.bmp", 4, false, false, "-", "opt_zout");
    // walls: up / cutaway / down
    static const char* wallArt[3] = { "cpanel/Buttons/dynacut.bmp", "cpanel/Buttons/nowall.bmp", "cpanel/Buttons/nocut.bmp" };
    static const char* wallIds[3] = { "opt_walls0", "opt_walls1", "opt_walls2" };
    int wx = x0 + 150;
    for (int i = 0; i < 3; i++) {
        int mode = (i == 0) ? 2 : (i == 1 ? 0 : 1); // left to right: up, cutaway, down (TS1 order)
        L->DrawArtButton(c, Rect(wx + i * 36, y + 2, 32, 26), wallArt[mode], 4, s.WallsMode == mode, false, L->Str->Walls(mode), wallIds[mode]);
    }
    y += 34;
    char wl[64]; snprintf(wl, sizeof wl, "%s", L->Str->Walls(s.WallsMode).c_str());
    // FIX(sims) QA2: right-aligned under the wall buttons; at wx it ran into "Second Story" ("Second StoryWalls Cutaway")
    DrawText(c, FontId::Small, std::max(x0 + 160, wx + 3 * 36 - 4 - TextWidth(FontId::Small, wl)), y, wl, RGBA(200, 210, 255));
    // stories
    int level = s.World()->State.Level();
    L->DrawArtButton(c, Rect(x0, y, 40, 18), "cpanel/Buttons/Lev1.BMP", 4, level == 1, false, gt.UI(138, 0, "First Story"), "opt_lev1");
    L->DrawArtButton(c, Rect(x0 + 44, y, 40, 18), "cpanel/Buttons/Lev2.BMP", 4, level == 2, false, gt.UI(138, 1, "Second Story"), "opt_lev2");
    DrawText(c, FontId::Small, x0 + 90, y + 3, gt.UI(138, level - 1, level == 1 ? "First Story" : "Second Story"), RGBA(200, 210, 255));
    y += 28;

    // ---- game: TS1's Options panel (Neighborhood / Save / Quit, Graphics / Sound / Play pages) from the front end
    // (src/app/front/GameOptions.h); its actions (save dialogs, leaving the lot) are run by the App (front/AppFlow.cpp).
    front::GameOptions::Get().Draw(c, Rect(a.X, y - 6, a.W, a.Bottom() - (y - 6)), [this](const Rect& r, const std::string& id) { L->Hit(r, id); });
}

bool UIOptionsPanel::Tap(const std::string& id) {
    if (front::GameOptions::Get().Tap(id)) return true; // "gopt_*"
    auto& s = *L->S;
    if (id == "opt_rotl") s.Rotate(-1);
    else if (id == "opt_rotr") s.Rotate(+1);
    else if (id == "opt_zin") s.Zoom(+1);
    else if (id == "opt_zout") s.Zoom(-1);
    else if (id == "opt_walls0") s.SetWallsMode(0);
    else if (id == "opt_walls1") s.SetWallsMode(1);
    else if (id == "opt_walls2") s.SetWallsMode(2);
    else if (id == "walls") s.SetWallsMode(s.WallsMode + 1);
    else if (id == "opt_lev1" || id == "down") s.ChangeLevel(-1);
    else if (id == "opt_lev2" || id == "up") s.ChangeLevel(+1);
    else if (id == "opt_cheat") L->CheatRequested = true; // TS1(sims)
    else if (id == "resume") L->SetMode(LotScreen::Mode::Live);
    else if (id == "leave") L->LeaveRequested = true;
    else if (id == "quit") L->QuitRequested = true;
    else return false;
    return true;
}

void UIOptionsPanel::PointerUpdate() {}

}
