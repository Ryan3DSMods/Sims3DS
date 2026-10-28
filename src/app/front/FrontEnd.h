/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims front end — the game's screens outside a lot, and the top-level screen flow:
//
//   Intro (EA logo, optional) -> Loading ("Reticulating Splines...") -> Neighbourhood <-> Create-A-Sim
//                                                                            |  ^
//                                                         enter a lot  ->  App loads the lot (live/buy/build panels)
//                                                         Options "Neighbourhood" / "Quit" from the lot
//
// The App (src/app/App.cpp) owns a FrontEnd and hands it both screens while no lot is loaded; it asks it for the
// lot to load (RequestedLot), draws the lot-loading screen through it, and gives control back with
// ReturnFromLot(). The in-lot options panel (START) is front::OptionsPanel.
//
// Dual screen: the top screen always shows the original art at its native pixel scale (a 400x240 window into
// the 800x600 screens), the touch screen holds the whole screen scaled 0.4 (800x600 -> 320x240 exactly) with
// the original buttons, so every original control is where the player expects it and the top screen is the
// close-up "monitor".
#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "app/Canvas.h"
#include "app/Input.h"
#include "app/front/IntroScreen.h"
#include "app/front/NhoodModel.h"
#include "app/front/UserDataSetup.h"

namespace simapp { namespace front {

/// Options the player sets (TS1 options panel, STR#145), saved in <SaveRoot>/sims_options.txt.
struct FrontSettings {
    // Graphics Options
    bool Shadows = true, Lighting = true, InterfaceEffects = true;
    int TerrainDetail = 2, CharacterDetail = 2;  // 0 low, 1 med, 2 high
    // Sound Options (0..100)
    int SoundVolume = 80, MusicVolume = 70, VoxVolume = 80;
    // Play Options
    bool AutoCentering = true, FreeWill = true, EdgeScrolling = false, QuickTips = true, AutoSnapshot = true, LivePIP = true;
    int AutonomyLevel = 3;                       // TS1(sims): 0 (none) .. 3 (full) for the family Sims
    // TS1(sims): 3DS
    bool PlayIntro = true;
    int LastNeighbourhood = 0;

    void Load(const std::string& path);
    void Save(const std::string& path) const;
};

/// A full-screen sub screen run by the front end (Create-A-Sim, credits...).
class SubScreen {
public:
    virtual ~SubScreen() {}
    /// One frame on both screens. Return false when the screen is finished.
    virtual bool Frame(const AppInput& in, double dtMs, Canvas& top, Canvas& bottom) = 0;
    /// TS1(sims) host scripting: screen position of a button id (CAS: CASScreen::ScriptButtonPos), state text.
    virtual bool ScriptButtonPos(const std::string&, int&, int&) { return false; }
    virtual std::string Describe() const { return std::string(); }
    /// TS1(sims) hardware pass: redraw a "loading" state during a blocking step of this screen (App::LoadProgressFrame).
    /// False = the screen has none (nothing is drawn).
    virtual bool DrawProgress(const std::string&, float, Canvas&, Canvas&) { return false; }
};

class NeighbourhoodScreen;

class FrontEnd {
public:
    struct Config {
        std::string DataRoot;   // install tree (GameData, UIGraphics, UserData, Template*)
        std::string SaveRoot;   // writable save area (default <DataRoot>/Saves)
        std::string CacheDir;   // objects.cache, intro video
        bool SkipIntro = false;
    };
    // hooks the App connects (all optional)
    std::function<void(const std::string&)> Music;        // Music/Modes/<name> ("Nhood", "Load", "dt_nhood"... "" = stop)
    std::function<void(const std::string&)> Sound;        // UI sound events (FreeSims UISounds names, e.g. "ui_nhood_click")
    std::function<void(const FrontSettings&)> SettingsChanged;
    /// ADDED(sims): the intro's soundtrack: play the file ("" = stop); returns the output latency (ms) or < 0 = silent.
    std::function<double(const std::string&)> IntroSound;
    /// Blocking content boot (Content::Init); the App passes its own so it keeps owning the engine set-up.
    std::function<bool(std::string& error)> BootContent;

    FrontEnd();
    ~FrontEnd();
    /// Fast: strings, UIGraphics, fonts, settings. The heavy loading runs inside Frame() (loading screen).
    bool Init(const Config& cfg);
    void Frame(const AppInput& in, double dtMs, Canvas& top, Canvas& bottom);

    enum class State { Intro, Loading, Neighbourhood, SubScreen, InLot, Error };
    State CurrentState() const { return _State; }
    bool QuitRequested = false;
    std::string Error;

    /// Set when the player enters a lot (house number); the App loads it and calls EnteredLot().
    int RequestedLot = -1;
    void EnteredLot();
    /// Back from a lot: family budget to keep (or -1), whether the player chose to save.
    void ReturnFromLot(int16_t family, int budget, bool save);
    /// Draws the lot loading screen (EP load screens / the house picture), Loading text index cycles with t.
    void DrawLotLoading(int house, Canvas& top, Canvas& bottom);
    /// Expansion-pack travel (src/simantics/engine/VMTS1Travel.h): the lot selection dialogs (types 5/7/8/11/13)
    /// show the destination map (LotMapForDialog id: 2 Downtown, 3 Vacation, 4/1/8 the neighbourhood (Old Town),
    /// 5 Studio Town, 7 Magic Town) in pick mode; done(house) or done(-1) when cancelled.
    void BeginLotPick(int mapId, const std::string& title, std::function<void(int)> done);
    bool Picking() const;
    void PickFrame(const AppInput& in, double dtMs, Canvas& top, Canvas& bottom);
    /// The trip screen: the expansion pack's loading picture (taxi, boat, bus, limo, Magic Town) for the
    /// destination, or for the lot being left when going home.
    void DrawTravelLoading(int destHouse, int fromHouse, Canvas& top, Canvas& bottom);
    static Area AreaOfHouse(int house);
    /// FIX(sims) FB1: the lot loader's current step (moves the loading screen's progress / label).
    void LotLoadStep(const std::string& step);
    void LotLoadDetail(const std::string& text) { _LotLoading.CurrentStep = text; }
    void BeginLotLoading() { _LotLoadKey.clear(); }
    /// Saves the neighbourhood (Options "Save"). Returns false with Error set on failure.
    bool SaveNeighbourhood();

    NhoodModel& Model() { return _Model; }
    FrontSettings& Settings() { return _Settings; }
    void SaveSettings();
    const std::string& SaveRoot() const { return _Cfg.SaveRoot; }

    /// TS1(sims): the boot loading screen with a progress line (redrawn during the blocking content boot).
    /// fraction: 0..1 of the current step (or < 0 = unknown).
    void DrawBootProgress(const std::string& text, float fraction, Canvas& top, Canvas& bottom);
    /// TS1(sims) hardware pass: the open sub screen's loading state (Create-A-Sim opening); false = no sub screen /
    /// it has none.
    bool DrawSubProgress(const std::string& text, float fraction, Canvas& top, Canvas& bottom) {
        return _State == State::SubScreen && _Sub && _Sub->DrawProgress(text, fraction, top, bottom);
    }
    bool InSubScreen() const { return _State == State::SubScreen && _Sub != nullptr; }
    /// Runs the boot to the neighbourhood screen without the intro/label animations (host scripts, tests).
    void FinishBootNow();
    void ShowSubScreen(std::unique_ptr<SubScreen> s, std::function<void()> onDone = nullptr);

    // ---- scripting (host simulator) ----
    bool ScriptLotPos(int house, int& x, int& y);
    bool ScriptPickButtonPos(const std::string& id, int& x, int& y);
    bool ScriptPickLotPos(int house, int& x, int& y);
    bool ScriptButtonPos(const std::string& id, int& x, int& y);
    std::string Describe() const;
    std::string Log;

private:
    Config _Cfg;
    State _State = State::Intro;
    FrontSettings _Settings;
    UserDataSetup _Setup;
    NhoodModel _Model;
    IntroVideo _Video;
    bool _IntroStarted = false;  // ADDED(sims): the intro's first frame (and its sound) has started
    void EndIntro();             // ADDED(sims): stops the intro's sound
    LoadingScreen _Loading;
    LoadingScreen _LotLoading;   // FIX(sims) FB1: lot / trip loading (classic screen)
    std::string _LotLoadKey;
    bool _BootDone = false;
    std::unique_ptr<NeighbourhoodScreen> _Nhood;
    std::unique_ptr<SubScreen> _Sub;
    std::function<void()> _SubDone;
    // boot steps
    int _BootStep = 0;
    int _PendingBroken = -1;  // FIX(sims) SAVE: a neighbourhood that failed at boot (offered for restore on the map)
    int _RestoreOffer = -1;   // FIX(sims) SAVE: no neighbourhood loads at all: the error screen offers this restore
    bool _ContentReady = false;
    bool BootStep(std::string& label); // returns true while steps remain
    double _Time = 0;
    friend class NeighbourhoodScreen;
};

}}
