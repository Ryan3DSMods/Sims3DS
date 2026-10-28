/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims app — see App.h
#include "app/App.h"
#include "files/LoadProgress.h"
#include <new>
#include "audio/GameAudio.h"
#include "app/Font.h"
#include "app/Theme.h"
#include "app/WorldPicking.h"
#include "app/front/FrontEnd.h"
#include "app/front/FrontUI.h"
#include "app/front/GameOptions.h"
#include "app/front/SimDialogView.h"
#include "app/front/SaveOverlay.h" // ADDED(sims) save progress
#include "app/ui/CheatState.h"
#include "simantics/VMArchitecture.h"
#include "simantics/model/VMPersonDataVariable.h"
#include "content/ContentManager.h"
#include "content/TS1NeighbourProvider.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"
#include "simantics/VMClock.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/entities/VMGameObject.h"
#include "simantics/engine/VMThread.h"
#include "simantics/engine/VMQueuedAction.h"
#include "simantics/engine/VMTS1Neighbourhood.h"
#include "simantics/entities/VMMultitileGroup.h"
#include "simantics/model/VMMotive.h"
#include "files/formats/iff/chunks/BMP.h"
#include "files/formats/iff/chunks/CTSS.h"
#include "files/formats/iff/chunks/OBJD.h"
#include "vitaboy/Texture.h"
#include "world/World.h"
#include "world/components/ObjectComponent.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace simapp {

using namespace FSO::SimAntics;
using FSO::SimAntics::Model::VMMotive;


App::App() : Top(TOP_W, TOP_H), Bottom(BOT_W, BOT_H) {}
App::~App() { _Session.reset(); }

static bool FileExists(const std::string& p) {
    FILE* f = fopen(p.c_str(), "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

static std::string Lower(const std::string& s) {
    std::string r = s;
    for (auto& c : r) c = (char)std::tolower((unsigned char)c);
    return r;
}

static std::string FirstName(const std::string& n) {
    size_t s = n.find(' ');
    return s == std::string::npos ? n : n.substr(0, s);
}

static std::string Money(int v) {
    char buf[32];
    std::string digits = std::to_string(v < 0 ? -v : v), out;
    int c = 0;
    for (int i = (int)digits.size() - 1; i >= 0; i--) {
        out.insert(out.begin(), digits[(size_t)i]);
        if (++c % 3 == 0 && i > 0) out.insert(out.begin(), ',');
    }
    snprintf(buf, sizeof buf, "%s\xA7%s", v < 0 ? "-" : "", out.c_str()); // '\xA7' = Latin-1 section sign (Simoleons)
    return buf;
}

// Decodes a BMP_ chunk (Windows BMP) into a canvas. Catalog icons use a key colour for transparency.
static bool DecodeBmp(FSO::Files::Formats::IFF::Chunks::BMP* bmp, Canvas& out, bool keyTransparent) {
    if (bmp == nullptr || bmp->GetData().empty()) return false;
    FSO::Vitaboy::Texture tex;
    if (!tex.LoadBMP(bmp->GetData()) || tex.Width <= 0 || tex.Height <= 0) return false;
    out.Resize(tex.Width, tex.Height);
    for (int y = 0; y < tex.Height; y++)
        for (int x = 0; x < tex.Width; x++) out.Px[(size_t)y * tex.Width + x] = tex.Fetch(x, y) | 0xFF000000u;
    // TS1 images use magenta (255,0,255) for "no pixel"; catalog icons also key on their corner colour
    uint32_t key = keyTransparent ? (out.Px[0] & 0x00FFFFFFu) : 0x00FF00FFu;
    for (auto& p : out.Px) {
        uint32_t r = p & 255, g = (p >> 8) & 255, b = (p >> 16) & 255;
        bool magenta = r >= 240 && g <= 8 && b >= 240; // (255,0,255), or (248,0,248) from 16-bit BMPs
        if (magenta || (keyTransparent && (p & 0x00FFFFFFu) == key)) p = 0;
    }
    return true;
}

// ======================================================================= boot

void App::StatusScreen(const std::string& title, const std::string& msg) {
    Top.GradientV(0, 0, TOP_W, TOP_H, theme::PanelTop, theme::ScreenBg);
    DrawTextCentered(Top, FontId::Title, Rect(0, 70, TOP_W, 30), title, theme::TextYellow);
    auto lines = WrapText(FontId::Small, msg, TOP_W - 40);
    int y = 112;
    for (auto& l : lines) { DrawTextCentered(Top, FontId::Small, Rect(0, y, TOP_W, 14), l, theme::Text); y += 14; }
    Bottom.GradientV(0, 0, BOT_W, BOT_H, theme::PanelBottom, theme::ScreenBg);
    DrawTextCentered(Bottom, FontId::Small, Rect(0, 200, BOT_W, 16), "sims3ds - unofficial engine port (needs your own game data)", theme::TextDim);
    if (Present) Present();
}

bool App::Init(const Config& cfg) {
    _Cfg = cfg;
    InstallLoadHooks(); // TS1(sims): step log + loading progress (front/AppFlow.cpp)
    _Str.Load(cfg.DataRoot + "/GameData");
    // the front end: EA intro, loading screen (the content boots behind it), neighbourhood (front/AppFlow.cpp)
    return FrontBoot();
}

void App::DrawPanelBg(Canvas& c, const Rect& r) {
    c.GradientV(r.X, r.Y, r.W, r.H, theme::PanelTop, theme::PanelBottom);
    c.FillRect(r.X, r.Y, r.W, 1, theme::PanelEdge);
}

void App::DrawButton(Canvas& c, const Rect& r, const std::string& label, bool on) {
    c.FillRoundRect(r, 6, on ? theme::ButtonOn : theme::ButtonBg);
    c.StrokeRoundRect(r.X, r.Y, r.W, r.H, 6, theme::PanelEdge);
    DrawTextCentered(c, FontId::Bold, r, Ellipsize(FontId::Bold, label, r.W - 6), on ? RGBA(30, 30, 30) : theme::TextBright);
}

// ======================================================================= live mode

// back to the neighbourhood without saving (FinishLeave also stops the lot's radios, TVs and loops through
// GameAudio::LeaveLot before its VM goes away)
void App::LeaveLot() { FinishLeave(false, false); }

void App::LiveFrame(const AppInput& in, double dtMs) {
    auto& s = *_Session;
    if (!_Lot) {
        _Lot.reset(new LotScreen(_Session.get(), &_Str, &Top, &Bottom));
        _Lot->Log = &Log;
        _Lot->NowMs = [this]() { return Now(); };
        _Lot->ThumbInterval = _Cfg.ThumbInterval;
    }
    auto& lot = *_Lot;
    // the options panel's Neighborhood / Save / Quit and their dialogs (front/AppFlow.cpp)
    if (_PendingLeave) { int p = _PendingLeave; _PendingLeave = 0; FinishLeave(p == 1 || p == 3, p >= 3); return; }
    front::GameOptions::Get().FE = _Front.get();
    bool flowConsumed = LotFlow(in);
    if (TravelFlow(in, dtMs)) return; // expansion-pack trips and lot selection maps (front/AppFlow.cpp)
    if (!flowConsumed && _SimDialog->Update(s, in)) flowConsumed = true; // TS1 dialogs, both screens
    if (_PendingLeave) { int p = _PendingLeave; _PendingLeave = 0; FinishLeave(p == 1 || p == 3, p >= 3); return; }
    lot.Speed = Speed;
    lot.ShowStats = _Cfg.ShowStats;
    // TS1(sims): cheats that act on the running lot (app/ui/CheatState.h)
    auto& cheat = CheatState::Get();
    if (auto ctx = s.Context()) if (auto arch = ctx->Architecture()) arch->MapEdit = cheat.MapEdit;
    if (cheat.Autonomy > 0)
        for (auto sim : s.Family) if (sim && !sim->Dead) sim->SetPersonData(FSO::SimAntics::Model::VMPersonDataVariable::AutonomyLevel, (int16_t)cheat.Autonomy);
    // ---------------- input (LotScreen: pie menu, dialogs, panels, cursor, touch view)
    lot.Input(flowConsumed ? AppInput() : in, dtMs, _Time);
    Speed = lot.Speed;
    PickMsLast = lot.PickMsLast;
    // lot.LeaveRequested / QuitRequested are answered by LotFlow next frame (the TS1 "Save?" dialogs)

    // ---------------- simulation (TS1 speeds: normal = 30 ticks/s = 60 VM::Update calls)
    static const double UPS[4] = { 0, 60, 180, 600 };
    int speed = lot.EffectiveSpeed(); // paused in buy/build/options and while a blocking dialog waits (TS1)
    if (_LotDialog && _LotDialog->Open) speed = 0;
    if (!cheat.Sweep) speed = 0; // TS1(sims): "sweep off" stops the simulation
    // TS1(sims): sim_speed -1000..1000 scales every speed setting, 1/16x .. 16x (the port's mapping; 0 = normal)
    double ups = UPS[speed] * std::pow(2.0, cheat.SimSpeed / 250.0);
    double t0 = Now();
    _UpdateAccum += ups * dtMs / 1000.0;
    _UpdateAccum = std::min(_UpdateAccum, ups * 0.25 + 2); // never queue more than 1/4 s of catch-up
    int n = (int)_UpdateAccum;
    int done = 0;
    while (done < n) {
        s.Update(1);
        done++;
        if (_Cfg.VMBudgetMs > 0 && (Now() - t0) > _Cfg.VMBudgetMs) { _UpdateAccum = 0; break; }
    }
    if (done == n) _UpdateAccum -= n;
    Stats.Updates = done;
    Stats.Vm = Now() - t0;

    // ---------------- render
    double t1 = Now();
    // FreeSims: walls cut around the mouse. ADDED(sims): around the pointer or the stylus, whichever is in use;
    // with the pointer hidden and no touch the last cut stays.
    if (lot.CutFollow) s.UpdateMouseCut(true, lot.CutX, lot.CutY);
    s.Draw();
    Stats.World = Now() - t1;
    double t2 = Now();
    if (_Cfg.ShowStats) {
        char sb[128];
        snprintf(sb, sizeof sb, "vm %.1f ms (%d upd)  world %.1f  ui %.1f  pick %.1f", Stats.Vm, Stats.Updates, Stats.World, Stats.Ui, PickMsLast);
        lot.StatsLine = sb;
    }
    lot.Draw();
    if (lot.CheatRequested) { lot.CheatRequested = false; OpenCheats(); } // TS1(sims): Options panel "Cheat"
    _SimDialog->Draw(s, lot, Top, Bottom);
    if (lot.CurrentMode == LotScreen::Mode::Options) front::GameOptions::Get().DrawHelp(Top, dtMs);
    if (_LotDialog && _LotDialog->Open) _LotDialog->Draw(Bottom);
    Stats.Ui = Now() - t2;
}

// ======================================================================= frame

void App::OpenCheats() {
    if (Cheats.Open) return;
    Cheats.Show();
    Log += "cheat console\n";
}

// TS1(sims): The Sims opens its cheat box with Ctrl+Shift+C; here: hold L and R together and press SELECT. L and R
// alone rotate the lot / switch neighbourhood, so a shoulder press is held back up to 6 frames (0.1 s): if the other
// shoulder follows, it is the chord and neither acts; otherwise it is delivered (at release at the latest).
AppInput App::FilterChord(const AppInput& in0, bool& openCheats) {
    AppInput in = in0;
    openCheats = false;
    const uint32_t LR = BTN_L | BTN_R;
    bool both = (in.Held & LR) == LR;
    if (both) {
        _Chord = true;
        _ShoulderPending = 0;
        in.Down &= ~LR;
        if (in.Down & BTN_SELECT) { openCheats = true; in.Down &= ~BTN_SELECT; }
        return in;
    }
    if (_Chord) { // until both shoulders are released
        if ((in.Held & LR) == 0) _Chord = false;
        in.Down &= ~LR;
        return in;
    }
    uint32_t newDown = in.Down & LR;
    in.Down &= ~LR;
    if (newDown) { _ShoulderPending |= newDown; _ShoulderFrames = 0; return in; }
    if (_ShoulderPending) {
        bool released = (in.Held & _ShoulderPending) != _ShoulderPending;
        if (released || ++_ShoulderFrames >= 6) { in.Down |= _ShoulderPending; _ShoulderPending = 0; }
    }
    return in;
}

void App::Frame(const AppInput& inRaw, double dtMs) {
    _Time += dtMs;
    double t0 = Now();
    Mode frameMode = _Mode; // TS1(sims) hardware pass: the slow-frame log line names the screen the frame started on
    Stats = FrameStats();
    // TS1(sims): the cheat console (app/ui/UICheatConsole.h) in the neighbourhood and on a lot
    bool chord = false;
    AppInput in = FilterChord(inRaw, chord);
    bool cheatScreen = (_Mode == Mode::Front && _Front && _Front->CurrentState() == front::FrontEnd::State::Neighbourhood) || (_Mode == Mode::Live && _Lot);
    if (chord && cheatScreen) { if (Cheats.Open) Cheats.Close(); else OpenCheats(); }
    if (!cheatScreen && Cheats.Open) Cheats.Close();
    if (Cheats.Open) {
        Cheats.S = _Mode == Mode::Live ? _Session.get() : nullptr;
        Cheats.L = _Mode == Mode::Live ? _Lot.get() : nullptr;
        Cheats.FE = _Front.get();
        Cheats.Log = &Log;
        if (Cheats.Update(in)) { in.Down &= ~(uint32_t)(BTN_A | BTN_B); }
    }
    switch (_Mode) {
        case Mode::Boot:
            StatusScreen("sims", "Starting...");
            break;
        case Mode::Error:
            StatusScreen("sims", _Error + "  (START quits)");
            if (in.Down & BTN_START) QuitRequested = true;
            if ((in.TouchDown || (in.Down & BTN_A)) && _Front) { _Front->ReturnFromLot(-1, -1, false); _Mode = Mode::Front; }
            break;
        case Mode::Front:
            FrontFrame(in, dtMs);
            break;
        case Mode::Loading: {
            // the lot loading screen (front end: the house picture, or the expansion packs' taxi / bus / boat pictures)
            if (_LoadFrames == 0) { _Front->BeginLotLoading(); _LastAudioPump = Now(); } // FIX(sims) FB1
            if (_LoadTravel) _Front->DrawTravelLoading(_LoadHouse, _TravelFrom, Top, Bottom);
            else _Front->DrawLotLoading(_LoadHouse, Top, Bottom);
            if (_LoadFrames++ == 0) break; // show it before the blocking load
            std::unique_ptr<LotSession> s(new LotSession());
            std::string err;
            double tl = Now();
            bool travel = _LoadTravel;
            // FIX(sims) FB1: _LoadTravel stays set during the blocking load (the progress redraws show the trip's
            // picture, not the home lot's), cleared once the load returns
            bool loaded = false;
            // TS1(sims): any exception (std::bad_alloc on the 3DS) ends on the error screen + log.txt instead of
            // retrying the load every frame
            try { loaded = s->Load(_LoadHouse, TOP_W, TOP_H, _Cfg.ScrollBuffer, err, travel); }
            catch (std::bad_alloc&) { err = "Out of memory while loading house " + std::to_string(_LoadHouse) + " (step: " + _LoadStep + ")"; }
            catch (std::exception& e) { err = std::string("Lot load failed: ") + e.what() + " (step: " + _LoadStep + ")"; }
            _LoadTravel = false;
            if (!loaded) {
                s.reset();
                FSO::Files::LoadProgress::Step("FATAL: " + err);
                _Error = err;
                _Mode = Mode::Error;
                break;
            }
            char lb[128]; snprintf(lb, sizeof lb, "loaded house %d in %.0f ms, %zu Sims\n", _LoadHouse, Now() - tl, s->Family.size());
            Log += lb;
            _Session = std::move(s);
            _Front->EnteredLot();
            ApplyPlaySettings(); // Free Will -> global 30 (front/AppFlow.cpp)
            _UpdateAccum = 0;
            Speed = 1;
            _Mode = Mode::Live;
            // TS1(sims) hardware pass: the first frame draws the whole lot (sprites, walls, floors decoded on demand)
            FSO::Files::LoadProgress::Step("lot " + std::to_string(_LoadHouse) + ": first frame");
            LiveFrame(AppInput(), 0);
            FSO::Files::LoadProgress::Step("lot " + std::to_string(_LoadHouse) + ": playing");
            break;
        }
        case Mode::Live:
            LiveFrame(in, dtMs);
            break;
    }
    if (Cheats.Open && (_Mode == Mode::Live || _Mode == Mode::Front)) Cheats.Draw(Top, _Time);
    AudioFrame(dtMs);
    Stats.Total = Now() - t0;
    // TS1(sims) hardware pass: a frame that blocked for over a second (a load without step lines, a stall) is named
    // in log.txt with the screen it happened on
    if (_Cfg.LogSteps && Stats.Total > 1000) {
        static const char* modes[] = { "boot", "front end", "loading", "live", "error" };
        std::string where = modes[(int)frameMode];
        if (_Mode == Mode::Front && _Front) where += _Front->InSubScreen() ? " (sub screen)" : "";
        if (_Mode == Mode::Live && _Lot) where += " (lot mode " + std::to_string((int)_Lot->CurrentMode) + ")";
        std::printf("[%8.3f s]   slow frame: %.2f s in %s, last step '%s'\n", (Now() - _LoadT0) / 1000.0, Stats.Total / 1000.0, where.c_str(), _LoadStep.c_str());
        std::fflush(stdout);
        _LastLogLine = Now();
    }
}

void App::PumpLoadAudio() {
    // FIX(sims) FB1: called from the loader's step / progress reports only (never from inside a file operation: a HIT
    // tick may open sound archives through the file pool). GameAudio::Update catches up at most 1/4 s per call.
    double now = Now();
    double dt = now - _LastAudioPump;
    if (dt < 30) return;
    _LastAudioPump = now;
    auto& ga = simaudio::GameAudio::Get();
    if (!ga.Ready()) return;
    dt = std::min(dt, 1000.0);
    while (dt > 0) { double d = std::min(dt, 250.0); ga.Update(d); dt -= d; }
}

// Sound (src/audio/README.md): runs the HIT VM, and follows the screen with the music like TS1 (neighbourhood
// tune in the chooser, loadloop while loading, no score in live mode, buy/build music), plus the speed clicks and
// the pie menu's pop. Edge-triggered, so screens that call GameAudio themselves are not overridden every frame.
void App::AudioFrame(double dtMs) {
    auto& ga = simaudio::GameAudio::Get();
    ga.Update(dtMs);
    if (!ga.Ready()) return;
    if (_Mode != _AudioMode) {
        // Mode::Front: the front end plays its own screens' music (front/FrontAudio.cpp)
        if (_Mode == Mode::Loading) ga.SetMusicMode(simaudio::MusicMode::Load);
        _AudioMode = _Mode;
        _AudioLotMode = -1;
        _AudioSpeed = -1;
        _AudioPie = false;
    }
    if (_Mode == Mode::Live && _Lot) {
        int lm = (int)_Lot->CurrentMode;
        if (lm != _AudioLotMode) {
            if (_Lot->CurrentMode == LotScreen::Mode::Live) ga.SetMusicMode(simaudio::MusicMode::Live);
            else if (_Lot->CurrentMode == LotScreen::Mode::Buy) ga.SetMusicMode(simaudio::MusicMode::Buy);
            else if (_Lot->CurrentMode == LotScreen::Mode::Build) ga.SetMusicMode(simaudio::MusicMode::Build);
            _AudioLotMode = lm;
        }
        if (_AudioSpeed >= 0 && Speed != _AudioSpeed) ga.SpeedChanged(_AudioSpeed, Speed);
        _AudioSpeed = Speed;
        if (_Lot->Pie.IsOpen && !_AudioPie) ga.PlayUI("ui_piemenu_appear");
        _AudioPie = _Lot->Pie.IsOpen;
        // TS1: pausing (and buy/build mode, which pause the lot under their own music) freezes the lot's sounds:
        // radios, TVs, loops. A blocking dialog does not (the sting that came with it keeps playing).
        // (VM::Update pauses on SpeedMultiplier, but the app stops calling it while paused.)
        bool paused = Speed == 0 || _Lot->CurrentMode == LotScreen::Mode::Buy || _Lot->CurrentMode == LotScreen::Mode::Build;
        if (paused != _AudioLotPaused && _Session && _Session->VM()) {
            for (auto ent : _Session->VM()->Entities)
                for (auto& snd : ent->SoundThreads)
                    if (snd.Sound) { if (paused) snd.Sound->Pause(); else snd.Sound->Resume(); }
            _AudioLotPaused = paused;
        }
    } else _AudioLotPaused = false;
}

// ======================================================================= scripting helpers

bool App::ScriptHousePos(int houseNumber, int& x, int& y) {
    if (_Front && _Front->Picking()) return _Front->ScriptPickLotPos(houseNumber, x, y); // lot selection map
    if (!_Front || (_Mode != Mode::Front && _Mode != Mode::Boot)) return false;
    return _Front->ScriptLotPos(houseNumber, x, y);
}

bool App::ScriptObjectPos(const std::string& name, int& x, int& y, std::string* found) {
    if (!_Lot) return false;
    return _Lot->ScriptObjectPos(name, x, y, found);
}

bool App::ScriptPieItemPos(const std::string& text, int& x, int& y) { return _Lot && _Lot->Pie.FindItem(text, x, y); }

bool App::ScriptButtonPos(const std::string& id, int& x, int& y) {
    if (_Mode == Mode::Front) return _Front && _Front->ScriptButtonPos(id, x, y);
    if (_Front && _Front->Picking()) return _Front->ScriptPickButtonPos(id, x, y);
    if (id.rfind("simdlg", 0) == 0 && _SimDialog) return _SimDialog->ButtonPos(atoi(id.c_str() + 6), x, y);
    if (_LotDialog && _LotDialog->Open && id.rfind("dlg", 0) == 0) return _LotDialog->ButtonPos(atoi(id.c_str() + 3), x, y);
    return _Lot && _Lot->ScriptButtonPos(id, x, y);
}

std::string App::DescribeQueue() {
    return _Lot ? _Lot->DescribeQueue() : std::string();
}

}
