/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims app — the playable game shell shared by the 3DS build (src/platform/3ds/main.cpp) and the host
// simulator (src/tools/sim3ds_host.cpp). Platform code only feeds AppInput + elapsed time and shows the two
// canvases: Top (400x240, the lot) and Bottom (320x240, the touch UI).
//
// Screens: the front end (src/app/front: EA intro, loading screen, neighbourhood, Create-A-Sim; see
// front/FrontEnd.h) -> loading the lot -> the lot (LotScreen: live / buy / build / options) -> back to the
// neighbourhood (front/AppFlow.cpp: the options panel's Neighborhood / Save / Quit and their dialogs).
#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <memory>
#include <string>
#include <vector>
#include "app/Canvas.h"
#include "app/Input.h"
#include "app/LotSession.h"
#include "files/FilePool.h"
#include "app/PieMenu.h"
#include "app/UIStrings.h"
#include "app/LotScreen.h"
#include "app/ui/UICheatConsole.h"

namespace simapp { namespace front { class FrontEnd; class FrontDialog; class SimDialogView; class SaveOverlay; } }

namespace FSO { namespace SimAntics { class VMEntity; } }

namespace simapp {

class App {
public:
    static const int TOP_W = 400, TOP_H = 240, BOT_W = 320, BOT_H = 240;

    struct Config {
        std::string DataRoot;          // the install tree: <DataRoot>/GameData, <DataRoot>/UserData
        std::string CacheDir;          // writable (objects.cache)
        int ScrollBuffer = 64;         // World2D static layer margin (px); 3 layers x (400+sb)x(240+sb)x8 bytes
        double VMBudgetMs = 0;         // max ms of VM work per frame (0 = unlimited, deterministic host runs)
        std::function<double()> NowMs; // monotonic clock (for the budget and stats)
        bool ShowStats = false;        // frame timings on the top screen
        int ThumbInterval = 1;         // refresh the touch-screen copy of the lot every N frames (3DS: 2)
        std::string SaveRoot;          // writable save area (default <DataRoot>/Saves: UserData, UserData2... copies)
        bool SkipIntro = false;        // no EA intro (host scripts)
        // TS1(sims): loading diagnostics (front/AppFlow.cpp InstallLoadHooks)
        bool LogSteps = false;                 // print a timestamped line per loading step (3DS: log.txt, flushed)
        std::function<std::string()> MemInfo;  // platform memory summary for those lines ("heap 12.3 MB free ...")
        std::function<double()> CpuMs;         // host: process CPU time (the 3DS estimate of each step); empty on the 3DS
    };

    enum class Mode { Boot, Front, Loading, Live, Error };

    App();
    ~App();
    /// Starts the front end (fast); the content boots behind its loading screen.
    bool Init(const Config& cfg);
    /// One frame: input -> simulation -> both canvases.
    void Frame(const AppInput& in, double dtMs);

    Canvas Top, Bottom;
    bool QuitRequested = false;
    /// Set by the platform: show Top/Bottom now (used to display progress during blocking loads).
    std::function<void()> Present;

    Mode CurrentMode() const { return _Mode; }
    LotSession* Session() { return _Session.get(); }
    int Speed = 1; // 0 paused, 1 normal, 2 high, 3 ultra (TS1)

    struct FrameStats { double Vm = 0, World = 0, Ui = 0, Pick = 0, Total = 0; int Updates = 0; };
    FrameStats Stats;       // last frame
    double PickMsLast = 0;  // last pick

    // ---- helpers for scripted input (host simulator) ----
    /// Touch position of a lot on the neighbourhood map (finishes the boot, travels to the lot's area).
    bool ScriptHousePos(int houseNumber, int& x, int& y);
    front::FrontEnd* Front() { return _Front.get(); }
    /// Optional: Free Will / autonomy level changed (Options) or a lot was loaded (the engine's global 30 is set already).
    std::function<void(bool freeWill, int autonomyLevel)> OnPlaySettings;
    /// Bottom-screen touch point that hits the named object (or Sim) in the touch view, verified by picking.
    bool ScriptObjectPos(const std::string& name, int& x, int& y, std::string* found = nullptr);
    bool ScriptPieItemPos(const std::string& text, int& x, int& y);
    bool ScriptButtonPos(const std::string& id, int& x, int& y); // "speed0".."speed3", "sim0".., "menu", "queue0"..
    std::vector<std::string> PieLabels() const { return _Lot ? _Lot->Pie.VisibleLabels() : std::vector<std::string>(); }
    bool PieOpen() const { return _Lot && _Lot->Pie.IsOpen; }
    LotScreen* Lot() { return _Lot.get(); } // live / buy / build / options UI (src/app/LotScreen.h)
    std::string DescribeQueue();
    std::string Log; // recent notable events (host prints it)
    /// TS1(sims): version stamp of the files in the cache folder (<cache>/cache_version.txt, tools/simcache).
    static int CacheVersion();
    void ShowStats(bool on) { _Cfg.ShowStats = on; }
    /// TS1(sims): the cheat console (hold L+R, press SELECT; Options panel "Cheat"). See app/ui/UICheatConsole.h.
    UICheatConsole Cheats;
    void OpenCheats();

private:
    Config _Cfg;
    Mode _Mode = Mode::Boot;
    UIStrings _Str;
    std::string _Error;
    double _Time = 0;

    // front end (src/app/front) + the lot's save / leave flow (front/AppFlow.cpp)
    std::unique_ptr<front::FrontEnd> _Front;
    std::unique_ptr<front::FrontDialog> _LotDialog;
    int _LoadHouse = -1, _LoadFrames = 0;
    int _PendingLeave = 0; // 1 save+leave, 2 leave, 3 save+quit, 4 quit (set by the dialogs)
    bool FrontBoot();
    void FrontFrame(const AppInput& in, double dtMs);
    bool LotFlow(const AppInput& in);
    void FinishLeave(bool save, bool quit);
    // expansion-pack travel (VMTS1Travel) + the TS1 dialog view (front/AppFlow.cpp, front/SimDialogView.h)
    std::unique_ptr<front::SimDialogView> _SimDialog;
    bool _LoadTravel = false;
    int _TravelFrom = 0;
    bool _TravelPending = false;
    int16_t _TravelRequest = 0; // VM::OnRequestLotSwitch (-1 = home: GameState.PendingLotSwitch cannot tell -1 from 'none')
    bool TravelFlow(const AppInput& in, double dtMs);
    void ApplyPlaySettings();
    void CheckCacheVersion();
    // TS1(sims): loading step log + progress redraws during blocking loads (files/LoadProgress.h)
    void InstallLoadHooks();
    void LoadProgressFrame(const std::string& text, float fraction);
    /// FIX(sims) FB1: ticks the sound VM during a blocking lot load (the load loop starts and keeps playing).
    void PumpLoadAudio();
    double _LastAudioPump = 0;
    std::string _LoadStep;
    double _LoadT0 = 0, _LastProgressDraw = -1e9, _LastProgressLog = -1e9;
    uint64_t _StepIoStart = 0;
    // TS1(sims) hardware log: what the current step has cost so far (files/FilePool.h FsOps), heartbeat lines
    double _StepT0 = 0, _StepCpu0 = 0, _LastLogLine = -1e9;
    FSO::Files::FsOps _StepFs0;
    FSO::Files::IOStats _StepIo0;
    // ADDED(sims) save progress: the "Saving..." overlay (front/SaveOverlay.h) over the lot while it is saved, driven
    // by the save steps' reports (files/LoadProgress.h "save: ..."); see front/AppFlow.cpp
    bool _Saving = false;
    float _SaveFraction = 0;
    std::string _SaveStepLabel;
    std::unique_ptr<front::SaveOverlay> _SaveView;
    double _SaveLastDraw = 0, _SaveGapMax = 0, _SaveBegin = 0;
    int _SaveDraws = 0;
    std::string _SaveGapWhere;
    void BeginSaveOverlay();
    void EndSaveOverlay();
    void SaveOverlayReport(const std::string& what, size_t done, size_t total, bool step);
    void SaveOverlayDraw();
    /// The clock of the save overlay's redraws: Now() on the console; on the host (CpuMs set) the 3DS estimate
    /// (CPU time x SdCostModel::CpuFactor + the modelled SD time), so the host redraws where the console would.
    double SaveClock() const;
public:
    /// Log line "[t s]   <tag> <step>: <elapsed> | fs ... | mem" for the current step (heartbeat / step end).
    void LogStepCost(const char* tag, const std::string& extra = std::string());
    /// Called after every file system operation (FsOps::OnOp): a heartbeat line every ~2 s inside a long step.
    void Heartbeat();
    /// Starts a named step (files/LoadProgress.h) from app code.
    static void BeginStep(const std::string& what);
private:

    // live: the lot UI (live / buy / build / options modes) is LotScreen (src/app/LotScreen.h)
    std::unique_ptr<LotSession> _Session;
    std::unique_ptr<LotScreen> _Lot;
    // sound (App::AudioFrame, src/audio/README.md)
    void AudioFrame(double dtMs);
    Mode _AudioMode = Mode::Boot;
    int _AudioLotMode = -1, _AudioSpeed = -1;
    bool _AudioPie = false;
    bool _AudioLotPaused = false;
    double _UpdateAccum = 0;
    void LiveFrame(const AppInput& in, double dtMs);
    // TS1(sims): L+R chord: a shoulder press waits a few frames to see whether the other shoulder joins it
    uint32_t _ShoulderPending = 0;
    int _ShoulderFrames = 0;
    bool _Chord = false;
    AppInput FilterChord(const AppInput& in, bool& openCheats);
    void LeaveLot();

    void StatusScreen(const std::string& title, const std::string& msg);
    void DrawPanelBg(Canvas& c, const Rect& r);
    void DrawButton(Canvas& c, const Rect& r, const std::string& label, bool on);
    double Now() const { return _Cfg.NowMs ? _Cfg.NowMs() : _Time; }
};

}
