/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims front end — see GameOptions.h
#include "app/front/GameOptions.h"
#include "app/front/FrontAudio.h"
#include "app/front/FrontStrings.h"
#include "app/front/FrontUI.h"
#include "app/front/ResGraphics.h"
#include "app/front/TS1Font.h"
#include <algorithm>

namespace simapp { namespace front {

static UIButton g_Nbhd, g_Save, g_Exit, g_Graphics, g_Sound, g_Play;
static Canvas g_Check, g_Radio, g_RadioSel, g_Knob;

GameOptions& GameOptions::Get() {
    static GameOptions o;
    return o;
}

void GameOptions::LoadArt() {
    if (_ArtLoaded) return;
    auto& ui = ResGraphics::Get();
    const float s = 0.7f;
    g_Nbhd.Load(ui, "kOptionNbhd", s);
    g_Save.Load(ui, "kOptionSave", s);
    g_Exit.Load(ui, "kOptionExit", s);
    g_Graphics.Load(ui, "kOptionGraphics", s);
    g_Sound.Load(ui, "kOptionSound", s);
    g_Play.Load(ui, "kOptionPlay", s);
    ui.LoadScaled("Shared/Sys/CheckBtnNew.BMP", 0.5f, g_Check);      // 6 frames: off, off lit, on, on lit, disabled...
    ui.LoadScaled("kNewOptionRadio", 0.6f, g_Radio);                  // 4 frames
    ui.LoadScaled("kNewOptionRadioSelected", 0.6f, g_RadioSel);
    ui.LoadScaled("kOptSliderOn", 0.8f, g_Knob);
    _ArtLoaded = true;
}

static void DrawFrame(Canvas& c, const Canvas& strip, int frames, int frame, int x, int y) {
    if (strip.Width == 0) return;
    int fw = strip.Width / frames;
    for (int yy = 0; yy < strip.Height; yy++) for (int xx = 0; xx < fw; xx++) {
        uint32_t p = strip.Px[(size_t)yy * strip.Width + frame * fw + xx];
        if (p >> 24) c.Blend(x + xx, y + yy, p);
    }
}

void GameOptions::Changed() {
    if (!FE) return;
    auto& st = FE->Settings();
    FrontVolumes(st.SoundVolume, st.MusicVolume, st.VoxVolume);
    FE->SaveSettings(); // also fires FrontEnd::SettingsChanged (free will / autonomy for the simulation)
}

void GameOptions::Draw(Canvas& c, const Rect& r, const std::function<void(const Rect&, const std::string&)>& hit) {
    LoadArt();
    auto& T = FrontStrings::Get();
    FrontSettings dummy;
    FrontSettings& st = FE ? FE->Settings() : dummy;
    // ---- the six big buttons: Neighborhood, Save, Quit | Graphics, Sound, Play
    struct B { UIButton* Btn; const char* Id; std::string Label; bool Sel; };
    B row[6] = {
        { &g_Nbhd, "gopt_nbhd", T.Get(145, 1, "Neighborhood"), false },
        { &g_Save, "gopt_save", T.Get(145, 3, "Save"), false },
        { &g_Exit, "gopt_quit", T.Get(145, 5, "Quit"), false },
        { &g_Graphics, "gopt_graphics", T.Get(145, 7, "Graphics Options"), Current == Page::Graphics },
        { &g_Sound, "gopt_sound", T.Get(145, 9, "Sound Options"), Current == Page::Sound },
        { &g_Play, "gopt_play", T.Get(145, 11, "Play Options"), Current == Page::Play },
    };
    int x = r.X + 4, y = r.Y + 2;
    for (int i = 0; i < 6; i++) {
        auto& b = row[i];
        if (i == 3) x += 10;
        b.Btn->X = x; b.Btn->Y = y;
        b.Btn->Selected = b.Sel;
        if (b.Btn->Texture.Width > 0) b.Btn->Draw(c);
        else { c.FillRoundRect(x, y, 36, 28, 6, b.Sel ? RGBA(90, 140, 220) : RGBA(40, 70, 140)); TextCentered(c, TS1Size::S9, Rect(x, y, 36, 28), b.Label.substr(0, 5), RGBA(255, 255, 255)); }
        int w = b.Btn->Texture.Width > 0 ? b.Btn->Width() : 36, h = b.Btn->Texture.Width > 0 ? b.Btn->Height() : 28;
        hit(Rect(x, y, w, h), b.Id);
        x += w + 4;
    }
    // ---- the page
    int py = r.Y + 34;
    std::string title = Current == Page::Graphics ? T.Get(145, 7, "Graphics Options") : Current == Page::Sound ? T.Get(145, 9, "Sound Options") : T.Get(145, 11, "Play Options");
    Text(c, TS1Size::S11, r.X + 6, py, title, RGBA(255, 216, 96), RGBA(0, 0, 30, 200));
    py += 16;
    auto check = [&](int cx, int cy, const std::string& label, bool on, const std::string& id, bool disabled) {
        if (g_Check.Width > 0) DrawFrame(c, g_Check, 6, disabled ? (on ? 5 : 4) : (on ? 2 : 0), cx, cy);
        else { c.StrokeRect(cx + 1, cy + 1, 12, 12, RGBA(200, 220, 255)); if (on) c.FillRect(cx + 4, cy + 4, 6, 6, RGBA(0, 255, 255)); }
        Text(c, TS1Size::S10, cx + 17, cy + 1, label, disabled ? RGBA(120, 130, 170) : RGBA(255, 255, 255), RGBA(0, 0, 30, 160));
        if (!disabled) hit(Rect(cx - 2, cy - 2, 17 + TextW(TS1Size::S10, label) + 4, 18), id);
    };
    auto radios = [&](int cx, int cy, const std::string& label, int value, int count, const std::string& idBase, const std::vector<std::string>& names) {
        Text(c, TS1Size::S10, cx, cy + 1, label, RGBA(255, 255, 255), RGBA(0, 0, 30, 160));
        int rx = cx + 104;
        for (int i = 0; i < count; i++) {
            const Canvas& art = (i == value) ? g_RadioSel : g_Radio;
            if (art.Width > 0) DrawFrame(c, art, 4, 0, rx, cy);
            else { c.StrokeCircle(rx + 6, cy + 6, 5, RGBA(200, 220, 255)); if (i == value) c.FillCircle(rx + 6, cy + 6, 3, RGBA(0, 255, 255)); }
            if (i < (int)names.size()) Text(c, TS1Size::S9, rx + 14, cy + 2, names[(size_t)i], RGBA(200, 215, 245));
            int w = 14 + (i < (int)names.size() ? TextW(TS1Size::S9, names[(size_t)i]) : 0) + 6;
            hit(Rect(rx - 2, cy - 2, w, 16), idBase + std::to_string(i));
            rx += w;
        }
    };
    std::vector<std::string> lmh = { T.Get(145, 39, "Low"), T.Get(145, 40, "Med"), T.Get(145, 41, "High") };
    int col2 = r.X + r.W / 2 + 2;
    _Sliders.clear();
    if (Current == Page::Graphics) {
        check(r.X + 6, py, T.Get(145, 14, "Anti-alias"), true, "gopt_aa", true);          // the 3DS LCD: nothing to choose
        check(col2, py, T.Get(145, 17, "Shadows"), st.Shadows, "gopt_shadows", false);
        check(r.X + 6, py + 18, T.Get(145, 20, "Lighting"), st.Lighting, "gopt_lighting", false);
        check(col2, py + 18, T.Get(145, 23, "Interface Effects"), st.InterfaceEffects, "gopt_effects", false);
        radios(r.X + 6, py + 42, T.Get(145, 28, "Terrain Detail"), st.TerrainDetail, 3, "gopt_terrain", lmh);
        radios(r.X + 6, py + 60, T.Get(145, 31, "Character Detail"), st.CharacterDetail, 3, "gopt_chardetail", lmh);
    } else if (Current == Page::Sound) {
        struct S { const char* Id; std::string Label; int Value; };
        S sl[3] = { { "fx", T.Get(145, 45, "Sound Effects"), st.SoundVolume }, { "music", T.Get(145, 47, "Music"), st.MusicVolume }, { "vox", T.Get(145, 50, "Vox"), st.VoxVolume } };
        for (int i = 0; i < 3; i++) {
            int sy = py + i * 24;
            Text(c, TS1Size::S10, r.X + 6, sy + 2, sl[i].Label, RGBA(255, 255, 255), RGBA(0, 0, 30, 160));
            Rect track(r.X + 104, sy + 6, r.W - 120, 4);
            c.FillRoundRect(track, 2, RGBA(10, 20, 60));
            c.GradientV(track.X, track.Y, track.W * sl[i].Value / 100, track.H, RGBA(120, 230, 255), RGBA(40, 120, 210));
            int kx = track.X + track.W * sl[i].Value / 100;
            if (g_Knob.Width > 0) c.Blit(g_Knob.Px.data(), g_Knob.Width, g_Knob.Height, kx - g_Knob.Width / 2, track.Y + 2 - g_Knob.Height / 2, true);
            else c.FillRoundRect(kx - 8, track.Y - 4, 16, 12, 5, RGBA(200, 210, 220));
            // ten touch steps along the track (the lot UI reports taps, not drags)
            for (int k = 0; k <= 10; k++) hit(Rect(track.X + track.W * k / 10 - track.W / 20, sy - 2, track.W / 10 + 1, 20), std::string("gopt_sl_") + sl[i].Id + "_" + std::to_string(k));
        }
        if (!FrontAudioAvailable()) Text(c, TS1Size::S9, r.X + 6, py + 74, "(sound engine not running)", RGBA(140, 150, 190));
    } else {
        check(r.X + 6, py, T.Get(145, 56, "Auto-Centering"), st.AutoCentering, "gopt_autocenter", false);
        check(col2, py, T.Get(145, 59, "Free Will"), st.FreeWill, "gopt_freewill", false);
        check(r.X + 6, py + 18, T.Get(145, 62, "Edge Scrolling"), st.EdgeScrolling, "gopt_edgescroll", false);
        check(col2, py + 18, T.Get(145, 72, "Quick Tips"), st.QuickTips, "gopt_quicktips", false);
        check(r.X + 6, py + 36, T.Get(145, 75, "Auto Snapshot"), st.AutoSnapshot, "gopt_snapshot", false);
        check(col2, py + 36, T.Get(145, 78, "Live PIP"), st.LivePIP, "gopt_pip", false);
        // TS1(sims): how much the family's Sims act on their own (0 = wait for orders .. 3 = full free will)
        radios(r.X + 6, py + 58, "Autonomy", st.FreeWill ? st.AutonomyLevel : 0, 4, "gopt_autonomy", { "0", "1", "2", "3" });
    }
}

bool GameOptions::Tap(const std::string& id, int) {
    if (id.rfind("gopt_", 0) != 0) return false;
    auto& T = FrontStrings::Get();
    FrontSettings dummy;
    FrontSettings& st = FE ? FE->Settings() : dummy;
    auto help = [&](int titleIdx) { Help = T.Get(145, titleIdx + 1, "") + "\n" + T.Get(145, titleIdx + 2, ""); HelpTime = 6000; };
    FrontSound("ui_click");
    if (id == "gopt_nbhd") Pending = Action::Neighbourhood;
    else if (id == "gopt_save") Pending = Action::Save;
    else if (id == "gopt_quit") Pending = Action::Quit;
    else if (id == "gopt_graphics") Current = Page::Graphics;
    else if (id == "gopt_sound") Current = Page::Sound;
    else if (id == "gopt_play") Current = Page::Play;
    else if (id == "gopt_shadows") { st.Shadows = !st.Shadows; help(17); Changed(); }
    else if (id == "gopt_lighting") { st.Lighting = !st.Lighting; help(20); Changed(); }
    else if (id == "gopt_effects") { st.InterfaceEffects = !st.InterfaceEffects; help(23); Changed(); }
    else if (id.rfind("gopt_terrain", 0) == 0) { st.TerrainDetail = atoi(id.c_str() + 12); help(28); Changed(); }
    else if (id.rfind("gopt_chardetail", 0) == 0) { st.CharacterDetail = atoi(id.c_str() + 15); help(31); Changed(); }
    else if (id.rfind("gopt_sl_", 0) == 0) {
        std::string rest = id.substr(8);
        size_t us = rest.rfind('_');
        int v = std::max(0, std::min(100, atoi(rest.c_str() + us + 1) * 10));
        std::string which = rest.substr(0, us);
        if (which == "fx") { st.SoundVolume = v; help(44); }
        else if (which == "music") { st.MusicVolume = v; help(47); }
        else { st.VoxVolume = v; help(50); }
        Changed();
    }
    else if (id == "gopt_autocenter") { st.AutoCentering = !st.AutoCentering; help(56); Changed(); }
    else if (id == "gopt_freewill") { st.FreeWill = !st.FreeWill; help(59); Changed(); }
    else if (id == "gopt_edgescroll") { st.EdgeScrolling = !st.EdgeScrolling; help(62); Changed(); }
    else if (id == "gopt_quicktips") { st.QuickTips = !st.QuickTips; help(72); Changed(); }
    else if (id == "gopt_snapshot") { st.AutoSnapshot = !st.AutoSnapshot; help(75); Changed(); }
    else if (id == "gopt_pip") { st.LivePIP = !st.LivePIP; help(78); Changed(); }
    else if (id.rfind("gopt_autonomy", 0) == 0) {
        st.AutonomyLevel = atoi(id.c_str() + 13);
        st.FreeWill = st.AutonomyLevel > 0;
        help(59);
        Changed();
    }
    else if (id == "gopt_aa") help(14);
    return true;
}

void GameOptions::DrawHelp(Canvas& top, double dtMs) {
    if (HelpTime <= 0 || Help.empty()) return;
    HelpTime -= dtMs;
    size_t nl = Help.find('\n');
    std::string title = Help.substr(0, nl), body = nl == std::string::npos ? "" : Help.substr(nl + 1);
    auto lines = Wrap(TS1Size::S10, body, top.Width - 60);
    if (lines.size() > 5) lines.resize(5);
    int h = 26 + (int)lines.size() * 13;
    Rect p(20, top.Height - h - 8, top.Width - 40, h);
    auto& art = FrontArt::Get();
    if (art.PopupTiler.Width > 0) NineSlice(top, art.PopupTiler, p, 24); else top.FillRoundRect(p, 10, RGBA(0, 0, 80, 220));
    Text(top, TS1Size::S11, p.X + 10, p.Y + 6, title, RGBA(255, 216, 96), RGBA(0, 0, 30, 200));
    int y = p.Y + 22;
    for (auto& l : lines) { Text(top, TS1Size::S10, p.X + 10, y, l, RGBA(255, 255, 255), RGBA(0, 0, 30, 180)); y += 13; }
}

}}
