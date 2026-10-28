/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims app — see UICheatConsole.h. TS1(sims): the whole file (FreeSims has no TS1 cheat console).
#include "app/ui/UICheatConsole.h"
#include "app/ui/CheatState.h"
#include "app/LotScreen.h"
#include "app/LotSession.h"
#include "app/Font.h"
#include "app/cas/CASTextInput.h"
#include "app/front/FrontEnd.h"
#include "app/front/FrontUI.h"
#include "app/front/NhoodModel.h"
#include "app/front/TS1Font.h"
#include "audio/GameAudio.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"
#include "simantics/VMClock.h"
#include "simantics/VMArchitecture.h"
#include "simantics/model/VMArchitectureTerrain.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/model/VMPersonDataVariable.h"
#include "world/World.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>

namespace simapp {

using namespace FSO::SimAntics;
using FSO::SimAntics::Model::VMPersonDataVariable;

static const int MONEY_CAP = 9999999; // TS1: the family funds never go above §9,999,999

static std::string Trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

static std::string Lower(std::string s) {
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

// "on" / "off" (anything else, or nothing: toggle)
static bool OnOff(const std::vector<std::string>& a, bool current) {
    if (a.empty()) return !current;
    if (a[0] == "on" || a[0] == "1" || a[0] == "true") return true;
    if (a[0] == "off" || a[0] == "0" || a[0] == "false") return false;
    return !current;
}

static bool IntArg(const std::vector<std::string>& a, int lo, int hi, int& v) {
    if (a.empty()) return false;
    char* end = nullptr;
    long n = strtol(a[0].c_str(), &end, 10);
    if (end == a[0].c_str() || *end != 0 || n < lo || n > hi) return false;
    v = (int)n;
    return true;
}

void UICheatConsole::Show() {
    Open = true;
    _KeyboardIn = 2; // draw the box, then the keyboard
    simaudio::GameAudio::Get().PlayUI("ui_click");
}

void UICheatConsole::Close() {
    Open = false;
    _KeyboardIn = 0;
}

void UICheatConsole::Print(const std::string& s) {
    History.push_back(s);
    while (History.size() > 5) History.pop_front();
    if (Log) *Log += "cheat: " + s + "\n";
}

bool UICheatConsole::Update(const AppInput& in) {
    if (!Open) return false;
    if (_KeyboardIn > 0 && --_KeyboardIn == 0) {
        cas::CASTextRequest req;
        req.Hint = "Enter Cheat Code:";
        req.MaxLength = 120;
        std::string text;
        if (!cas::CASSystemKeyboard(req, text)) { Close(); return true; } // Cancel closes the box (Ctrl+Shift+C again)
        text = Trim(text);
        if (!text.empty()) Execute(text);
        return true;
    }
    if (_KeyboardIn > 0) return true;
    if (in.Down & BTN_A) { _KeyboardIn = 1; return true; }
    if (in.Down & BTN_B) { Close(); return true; }
    return false;
}

int UICheatConsole::Execute(const std::string& line) {
    Print("> " + line);
    std::vector<std::string> out;
    int known = 0, n = 0;
    _MoneySound = false;
    size_t pos = 0;
    std::string text = Lower(line);
    while (pos <= text.size() && n < 200) { // TS1: ';' separates cheats
        size_t e = text.find(';', pos);
        std::string tok = Trim(text.substr(pos, e == std::string::npos ? std::string::npos : e - pos));
        pos = (e == std::string::npos) ? text.size() + 1 : e + 1;
        if (tok.empty()) continue;
        n++;
        if (tok == "!") { if (LastCheat.empty()) continue; tok = LastCheat; } // '!' = the previous cheat again
        if (ExecuteOne(tok, out)) known++;
        LastCheat = tok;
    }
    for (auto& o : out) Print(o);
    if (_MoneySound) simaudio::GameAudio::Get().PlayUI("ui_moneyback"); // the cash register of the original
    return known;
}

bool UICheatConsole::ExecuteOne(const std::string& cheat, std::vector<std::string>& out) {
    std::vector<std::string> a;
    {
        size_t p = 0;
        while (p < cheat.size()) {
            size_t s = cheat.find_first_not_of(' ', p);
            if (s == std::string::npos) break;
            size_t e = cheat.find(' ', s);
            a.push_back(cheat.substr(s, e == std::string::npos ? std::string::npos : e - s));
            p = e == std::string::npos ? cheat.size() : e;
        }
    }
    if (a.empty()) return false;
    std::string name = a[0];
    a.erase(a.begin());
    auto& C = CheatState::Get();
    VM* vm = S ? S->VM() : nullptr;

    // ---- money: +§1,000 to the family funds (rosebud: The Sims; klapaucius: added by the expansion packs;
    // both work in the Complete Collection). Only a lot has a family budget: in the neighbourhood nothing happens.
    if (name == "rosebud" || name == "klapaucius") {
        if (S && vm) {
            int b = S->Budget();
            int nb = std::min(MONEY_CAP, b + 1000);
            S->SetBudget(nb);
            if (L) L->QuietBudget(); // the funds display just counts up (no "+§" balloon, as in TS1)
            if (nb != b) _MoneySound = true;
        }
        return true;
    }
    // ---- set_hour <1-24>: the clock jumps to that hour (24 = midnight), minutes reset
    if (name == "set_hour") {
        int h;
        if (!IntArg(a, 1, 24, h)) return true;
        if (vm && vm->Context && vm->Context->Clock) {
            auto clock = vm->Context->Clock.get();
            clock->Hours = h % 24;
            clock->Minutes = 0;
            clock->MinuteFractions = 0;
            if (vm->GlobalState.size() > 5) { vm->GlobalState[0] = (int16_t)clock->Hours; vm->GlobalState[5] = 0; }
        }
        return true;
    }
    if (name == "move_objects") { C.MoveObjects = OnOff(a, C.MoveObjects); return true; }
    if (name == "map_edit") { C.MapEdit = OnOff(a, C.MapEdit); if (vm && vm->Context->Architecture()) vm->Context->Architecture()->MapEdit = C.MapEdit; return true; }
    if (name == "tile_info") { C.TileInfo = OnOff(a, C.TileInfo); return true; }
    if (name == "route_balloons") { C.RouteBalloons = OnOff(a, C.RouteBalloons); return true; }
    if (name == "draw_routes") { C.DrawRoutes = OnOff(a, C.DrawRoutes); return true; }
    if (name == "preview_anims") { C.PreviewAnims = OnOff(a, C.PreviewAnims); return true; }
    if (name == "draw_all_frames") { C.DrawAllFrames = OnOff(a, C.DrawAllFrames); return true; }
    if (name == "draw_floorable") { C.DrawFloorable = OnOff(a, C.DrawFloorable); return true; }
    if (name == "sweep") { C.Sweep = OnOff(a, C.Sweep); return true; }
    // ---- sim_speed <-1000..1000>: slower / faster simulation at every speed setting (0 = normal)
    if (name == "sim_speed") { int v; if (IntArg(a, -1000, 1000, v)) C.SimSpeed = v; return true; }
    // ---- autonomy <1-100>: how much the Sims act on their own (App::LiveFrame applies it, see README "Cheats")
    if (name == "autonomy") { int v; if (IntArg(a, 1, 100, v)) C.Autonomy = v; return true; }
    // ---- grow_grass <0-150>: the whole lawn at that growth (0 = dead brown, 150 = lush green)
    if (name == "grow_grass") {
        int v;
        if (!IntArg(a, 0, 150, v)) return true;
        auto arch = vm ? vm->Context->Architecture() : nullptr;
        if (arch && arch->Terrain) {
            uint8_t state = (uint8_t)(255 - v * 255 / 150); // GrassState: 0 green .. 255 brown
            for (int y = 0; y < arch->Height; y++)
                for (int x = 0; x < arch->Width; x++) arch->SetTerrainGrass((int16_t)x, (int16_t)y, state);
            arch->Terrain->VisGrass = arch->Terrain->GrassState;
            arch->SignalRedraw();
        }
        return true;
    }
    // ---- interests: the selected Sim's interests (person data 46-53, 0..10)
    if (name == "interests") {
        auto sim = S ? S->SelectedSim() : nullptr;
        if (sim) {
            bool child = sim->GetPersonData(VMPersonDataVariable::PersonsAge) < 18;
            static const char* adult[8] = { "Travel", "Violence", "Politics", "60's", "Weather", "Sports", "Music", "Outdoors" };
            static const char* kid[8] = { "Toys", "Aliens", "Pets", "School", "Weather", "Sports", "Music", "Outdoors" };
            std::string l1, l2;
            for (int i = 0; i < 8; i++) {
                int v = sim->GetPersonData((VMPersonDataVariable)((int)VMPersonDataVariable::Interest_TravelToys + i));
                char b[40]; snprintf(b, sizeof b, "%s%s %d", (i % 4) ? ", " : "", child ? kid[i] : adult[i], v);
                (i < 4 ? l1 : l2) += b;
            }
            out.push_back(l1);
            out.push_back(l2);
        }
        return true;
    }
    // ---- rotation <0-3>: the camera
    if (name == "rotation") {
        int r;
        if (IntArg(a, 0, 3, r) && S && S->World()) for (int i = 0; i < 4 && (int)S->World()->State.Rotation() != r; i++) S->Rotate(+1);
        return true;
    }
    // ---- house <n>: loads that house (from the neighbourhood; its family must live there)
    if (name == "house") {
        int h;
        if (IntArg(a, 1, 999, h) && FE && !S && FE->CurrentState() == front::FrontEnd::State::Neighbourhood) {
            auto lot = FE->Model().Lot(h);
            if (lot && lot->Exists && FE->Model().FamilyInHouse(h)) FE->RequestedLot = h;
        }
        return true;
    }
    // ---- quit: the game's own Quit (asks to save first)
    if (name == "quit") { if (L) L->QuitRequested = true; return true; }
    // ---- water_tool: in the Complete Collection the water tool is part of build mode (Hot Date); accepted
    if (name == "water_tool") return true;
    // engine / authoring cheats of the original with nothing to act on in this port: accepted silently like the
    // original does for every line (file dumps, logs, asset reloads, the PC camera, the tutorial...)
    static const char* silent[] = {
        "edit_char", "prepare_lot", "history", "hist_add", "sim_log", "log_mask", "dump_happy", "html", "reload_people",
        "cam_mode", "music", "sound", "soundevent", "draw_origins", "debug_social", "import", "lot_size",
        "visitor_control", "auto_level", "tutorial", "genable_objects", "nessie", "xyzzy", "crash", "vsync",
    };
    for (auto s : silent) if (name == s) return true;
    return false; // unknown: nothing happens, nothing is printed (TS1)
}

void UICheatConsole::Draw(Canvas& top, double timeMs) {
    if (!Open) return;
    using namespace front;
    // TS1: a small box at the top of the screen: the prompt, the text field, and what was entered/printed before
    int lines = (int)History.size();
    int lh = 12;
    Rect box(6, 4, 300, 46 + lines * lh);
    auto& art = FrontArt::Get();
    if (art.DialogTiler.Width > 0) NineSlice(top, art.DialogTiler, box, 12);
    else {
        top.FillRoundRect(box, 6, RGBA(14, 22, 96, 235));
        top.StrokeRoundRect(box.X, box.Y, box.W, box.H, 6, RGBA(140, 150, 230));
    }
    int y = box.Y + 6;
    Text(top, TS1Size::S10, box.X + 10, y, "Enter Cheat Code:", col::TextGold, col::Shadow);
    y += 14;
    Rect field(box.X + 8, y, box.W - 16, 16);
    top.FillRect(field, RGBA(0, 10, 50, 230));
    top.StrokeRect(field.X, field.Y, field.W, field.H, RGBA(150, 170, 230));
    bool caret = ((int)(timeMs / 500.0) & 1) == 0;
    std::string hint = _KeyboardIn > 0 ? std::string() : std::string("A: type   B: close");
    if (caret) top.FillRect(field.X + 4, field.Y + 3, 1, 10, RGBA(255, 255, 255));
    if (!hint.empty()) Text(top, TS1Size::S9, field.Right() - 4 - TextW(TS1Size::S9, hint), field.Y + 2, hint, col::TextDim);
    y += 20;
    for (auto& h : History) {
        Text(top, TS1Size::S9, box.X + 10, y, Fit(TS1Size::S9, h, box.W - 20), h.rfind("> ", 0) == 0 ? col::Text : col::TextBlue, col::Shadow);
        y += lh;
    }
}

}
