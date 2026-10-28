/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims — the App's top-level screen flow (App members kept here, next to the front end they drive):
//
//   Boot -> Front (intro, loading screen, neighbourhood, Create-A-Sim)  --enter lot-->  Loading  -->  Live
//   Live --Options "Neighborhood" / "Quit" (+ "Save?" dialog)-->  Front / quit
//
// TS1(sims): the dialogs are the game's own (STR#153 AppStrings 3/4 "Save" "Do you want to save the game?",
// 5/6 "Quit?" "Do you want to save before quitting?"; STR#152 Yes/No/Cancel).
#include "files/SafeFile.h" // FIX(sims) SAVE
#include "app/App.h"
#include "app/Font.h"
#include "app/front/FrontAudio.h"
#include "app/front/FrontEnd.h"
#include "app/front/FrontStrings.h"
#include "app/front/FrontUI.h"
#include "app/front/GameOptions.h"
#include "app/front/LotSave.h"
#include "app/front/SimDialogView.h"
#include "app/front/SaveOverlay.h"
#include "simantics/VM.h"
#include "files/FilePool.h"
#include "files/LoadProgress.h"
#include "app/front/TS1Font.h"
#include <new>
#include "simantics/engine/VMTS1Travel.h"
#include "content/ContentManager.h"
#include "content/TS1NeighbourProvider.h"
#include "files/formats/iff/chunks/FAMI.h"
#include "files/formats/iff/IffFile.h"
#include "vitaboy/content/TS1AvatarProvider.h"
#include "app/ui/UIGraphics.h"
#include "app/buy/UICatalog.h"
#include "app/live/UIHeadlineRenderer.h"
#include <algorithm>
#include <cstdio>
#include <cctype>
#include <unordered_set>

namespace simapp {

static bool FlowFileExists(const std::string& p) {
    FILE* f = fopen(p.c_str(), "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

// The object index (<cache>/objects.cache) is rebuilt when the app's OBJD reading changes: a version stamp next to it
// (cache_version.txt) and the stale index deleted once (the next boot re-indexes Objects.far).
// 3: archive paths relative to the install + Downloads/*.far sorted (portable caches, tools/simcache), every cache
//    that stores archive indexes is rebuilt
static const int CACHE_VERSION = 3; // 2: OBJD parsing fixed (lot UI pass)
int App::CacheVersion() { return CACHE_VERSION; }

void App::CheckCacheVersion() {
    std::string stamp = _Cfg.CacheDir + "/cache_version.txt";
    int have = 0;
    if (FILE* f = fopen(stamp.c_str(), "r")) { if (fscanf(f, "%d", &have) != 1) have = 0; fclose(f); }
    if (have == CACHE_VERSION) return;
    if (remove((_Cfg.CacheDir + "/objects.cache").c_str()) == 0) Log += "objects.cache was made by an older version: re-indexing\n";
    // TS1(sims): the caches holding archive indexes (catalogue, expansion walls/floors) go with it
    for (const char* n : { "/catalog.cache", "/wallfloorwll.cache", "/wallfloorflr.cache" }) remove((_Cfg.CacheDir + n).c_str());
    FSO::Files::SafeFile::WriteText(stamp, std::to_string(CACHE_VERSION) + "\n"); // FIX(sims) SAVE
}

/// TS1(sims): memory between lots. The engine (like C#) keeps every object file it ever loaded; on the 3DS that
/// is 10-40 MB per lot visited, so once the lot's VM and World are gone the object files are freed again (the
/// controller objects every lot creates stay) together with the assembled BHAV routines keyed by their pointers.
static void ReleaseLotContent(std::string& log) {
    auto c = FSO::Content::Content::Get();
    if (c == nullptr || !c->WorldObjects) return;
    if (!FSO::SimAntics::VM::ReleaseAssembled()) return; // a VM is still alive: keep everything
    std::unordered_set<uint32_t> keep(c->WorldObjects->ControllerObjects.begin(), c->WorldObjects->ControllerObjects.end());
    int n = c->WorldObjects->ReleaseLoaded(keep);
    FSO::Files::Formats::IFF::IffFile::DropDecodedFrames(); // FIX(sims) memory: decoded wall/floor/kept-object frames
    // FIX(sims) memory: caches only the lot uses (avatar meshes/textures/keyframes, lot UI art, catalogue details,
    // headline sprites): they were kept for the whole session (~15 MB back in the neighbourhood)
    if (auto av = FSO::Content::TS1::TS1AvatarContent::Get()) av->ReleaseCaches();
    UIGraphics::Get().ReleaseImages();
    UICatalog::Get().TrimDetails(0);
    UIHeadlineRenderer::Unload();
    log += "released " + std::to_string(n) + " object files of the lot (" + std::to_string(c->WorldObjects->LoadedFileCount()) + " kept)\n";
}

/// Options -> engine: TS1's Free Will option is global 30 "Free Will" (Behavior.iff STR#129 'global labels'),
/// which the game's idle trees read before looking for something to do on their own. Autonomy level 0 = off.
void App::ApplyPlaySettings() {
    if (!_Session || !_Front) return;
    auto& st = _Front->Settings();
    bool freeWill = st.FreeWill && st.AutonomyLevel > 0;
    if (auto vm = _Session->VM()) { vm->SetGlobalValue(30, (int16_t)(freeWill ? 1 : 0)); Log += std::string("free will: global 30 = ") + (freeWill ? "1" : "0") + "\n"; }
    // TS1(sims) hook: anything else that wants the settings (sound volumes are applied by GameOptions)
    if (OnPlaySettings) OnPlaySettings(st.FreeWill, st.AutonomyLevel);
}

/// Expansion-pack travel (src/simantics/engine/VMTS1Travel.h): the lot selection dialogs open the destination map;
/// Generic Sims Call 17 (GameState.PendingLotSwitch) unloads this lot and loads the destination behind the trip screen.
bool App::TravelFlow(const AppInput& in, double dtMs) {
    using FSO::SimAntics::Engine::Utils::VMTS1Travel;
    auto content = FSO::Content::Content::Get();
    auto hood = content ? content->Neighborhood.get() : nullptr;
    if (!_Session) return false;
    if (auto vm = _Session->VM()) if (!vm->OnRequestLotSwitch) vm->OnRequestLotSwitch = [this](int16_t lot) { _TravelPending = true; _TravelRequest = lot; };
    // a lot switch was requested (taxi, boat, limo, phone): go
    if (_TravelPending || (hood != nullptr && hood->GameState.PendingLotSwitch > 0)) {
        int16_t lot = _TravelPending ? _TravelRequest : hood->GameState.PendingLotSwitch;
        _TravelPending = false;
        if (hood != nullptr) hood->GameState.PendingLotSwitch = -1;
        int16_t house = VMTS1Travel::ResolveLot(lot);
        Log += "travel: lot " + std::to_string(lot) + " -> house " + std::to_string(house) + "\n";
        if (house <= 0) return false;
        // FIX(sims): like TS1, a trip from the family's home lot saves the game first (house + neighbourhood). The
        // home lot is reloaded from its house file when the family comes back, so without this everything done at
        // home since the last save (objects bought, the Makin' Magic starter kit opened, skills ...) was lost and
        // the one-time expansion letters / starter kit came again after every trip.
        if (hood != nullptr && _Front && hood->GetFamilyForHouse((int16_t)_Session->HouseNumber) != nullptr) {
            auto fam = hood->GetFamilyForHouse((int16_t)_Session->HouseNumber);
            _Front->Model().SetBudget((int16_t)fam->ChunkID, _Session->Budget());
            BeginSaveOverlay(); // ADDED(sims) save progress
            std::string err;
            bool ok = front::SaveLot(*_Session, err, Log);
            if (!ok) Log += "house save failed: " + err + "\n";
            ok = _Front->SaveNeighbourhood() && ok;
            EndSaveOverlay();
            Log += ok ? "travel: saved house and neighbourhood before the trip\n" : "travel: save before the trip failed\n";
        }
        _TravelFrom = _Session->HouseNumber;
        _Lot.reset();
        _Session.reset();
        front::FrontLotUnloaded();
        ReleaseLotContent(Log);
        _LoadHouse = house;
        _LoadFrames = 0;
        _LoadTravel = true;
        _Mode = Mode::Loading;
        return true;
    }
    // a lot selection dialog (types 5/7/8/11/13): the map, answered with the house number
    if (!_Session->Dialogs.empty() && front::SimDialogView::IsLotSelection(_Session->Dialogs.front().Type)) {
        if (!_Front->Picking()) {
            auto& d = _Session->Dialogs.front();
            // FIX(sims) REL: the phone plugins' 'Ask for Lot' dialogs point their message at string 1 of the plugin's own
            // table, which is not a map prompt: "Do you need a ride downtown?" in the Vacation plugin too, "There's almost
            // always a balloon drifting nearby. Call back..." in Magic Town's, empty in Studio Town's. The map title now
            // comes from UIText STR# 213 'Vacation Dialog Strings' / 212 'Go Downtown Dialog Strings' (the EXE's tables).
            auto& FS = front::FrontStrings::Get();
            std::string title = VMTS1Travel::LotMapForDialog(d.Type) == 3 ? FS.Get(213, 0, "Where would you like to stay?")
                                                                           : FS.Get(212, 0, "Where would you like to go?");
            while (!title.empty() && (unsigned char)title.back() <= 32) title.pop_back();
            if (title.empty()) {
                title = ToLatin1(d.Message.empty() ? d.Title : d.Message);
                size_t nl = title.find('\n');
                if (nl != std::string::npos) title.resize(nl);
            }
            _Front->BeginLotPick(VMTS1Travel::LotMapForDialog(d.Type), title, [this](int house) {
                if (!_Session) return;
                if (house > 0) _Session->RespondDialog(0, std::to_string(house));
                else _Session->RespondDialog(1, "");
            });
        }
        if (_Front->Picking()) {
            _Front->PickFrame(in, dtMs, Top, Bottom);
            if (!_Front->Log.empty()) { Log += _Front->Log; _Front->Log.clear(); }
            return true;
        }
    }
    return false;
}

bool App::FrontBoot() {
    _Front.reset(new front::FrontEnd());
    _LotDialog.reset(new front::FrontDialog());
    _SimDialog.reset(new front::SimDialogView());
    CheckCacheVersion();
    _Front->SettingsChanged = [this](const front::FrontSettings&) { ApplyPlaySettings(); };
    front::GameOptions::Get().FE = _Front.get();
    _Front->Music = front::FrontMusic;
    _Front->Sound = front::FrontSound;
    _Front->IntroSound = front::FrontIntroSound; // ADDED(sims)
    _Front->BootContent = [this](std::string& error) -> bool {
        try {
            FSO::Content::Content::Init(_Cfg.DataRoot, _Cfg.CacheDir);
        } catch (std::bad_alloc&) {
            error = "Out of memory while loading the game data (step: " + _LoadStep + "). Start sims as the CIA "
                    "(178 MB mode) or from the Homebrew Launcher via title takeover.";
            FSO::Files::LoadProgress::Step("FATAL: " + error);
            return false;
        } catch (std::exception& e) {
            error = std::string("Could not load the game data: ") + e.what();
            FSO::Files::LoadProgress::Step("FATAL: " + error);
            return false;
        }
        auto content = FSO::Content::Content::Get();
        if (content == nullptr || content->WorldObjects->Entries.empty()) {
            error = "No objects found. Copy GameData/Objects/Objects.far (and the rest of GameData) to " + _Cfg.DataRoot;
            return false;
        }
        return true;
    };
    front::FrontEnd::Config fc;
    fc.DataRoot = _Cfg.DataRoot;
    fc.SaveRoot = _Cfg.SaveRoot;
    fc.CacheDir = _Cfg.CacheDir;
    fc.SkipIntro = _Cfg.SkipIntro;
    _Front->Init(fc);
    auto& st = _Front->Settings();
    front::FrontVolumes(st.SoundVolume, st.MusicVolume, st.VoxVolume);
    if (!FlowFileExists(_Cfg.CacheDir + "/objects.cache")) Log += "first run: Objects.far will be indexed during the loading screen\n";
    _Mode = Mode::Front;
    return true;
}

// TS1(sims): the loaders report steps and progress through files/LoadProgress.h. Each step becomes a timestamped
// log line with the platform's memory summary and the SD I/O so far (3DS: log.txt when the debug file exists, flushed
// per line, so a hang or crash shows the last step reached); long steps redraw the loading screen with a counter.
// Hardware pass (26 Oct): when a step ends, a "done" line gives what it cost (time, file system operations and the time
// the FS service took for them, from files/FilePool.h FsOps; the host adds its 3DS estimate), and every ~2 s inside a
// step that touches the SD card a "...still in" heartbeat line is written (FsOps::OnOp), so the last lines of a log
// always say where the time goes.
static App* g_LogApp = nullptr;

static std::string FsDelta(const FSO::Files::FsOps& a, const FSO::Files::FsOps& b, const FSO::Files::IOStats& ia, const FSO::Files::IOStats& ib) {
    char buf[320];
    std::snprintf(buf, sizeof buf, "fs %llu opens %.2f s, %llu stats %.2f s, %llu dirs+%llu dir reads %.2f s, %llu reads %.2f MB %.2f s, %llu seeks, %llu writes %.2f s | pool %llu opens, %llu files",
                  (unsigned long long)(a.Opens - b.Opens), (a.OpenMs - b.OpenMs) / 1000.0, (unsigned long long)(a.Stats - b.Stats), (a.StatMs - b.StatMs) / 1000.0,
                  (unsigned long long)(a.DirOpens - b.DirOpens), (unsigned long long)(a.DirReads - b.DirReads), (a.DirMs - b.DirMs) / 1000.0,
                  (unsigned long long)(a.Reads - b.Reads), (a.BytesRead - b.BytesRead) / 1048576.0, (a.ReadMs - b.ReadMs + a.SeekMs - b.SeekMs) / 1000.0,
                  (unsigned long long)(a.Seeks - b.Seeks), (unsigned long long)(a.Writes - b.Writes), (a.WriteMs - b.WriteMs + a.CloseMs - b.CloseMs) / 1000.0,
                  (unsigned long long)(ia.Opens - ib.Opens), (unsigned long long)(ia.Files - ib.Files));
    return buf;
}

void App::LogStepCost(const char* tag, const std::string& extra) {
    if (!_Cfg.LogSteps || _LoadStep.empty()) return;
    double now = Now();
    auto& fs = FSO::Files::Fs;
    std::string est;
    if (_Cfg.CpuMs) { // host: the console estimate = modelled SD time + CPU time scaled to the 804 MHz ARM11
        FSO::Files::FsOps d;
        d.Opens = fs.Opens - _StepFs0.Opens; d.Stats = fs.Stats - _StepFs0.Stats; d.DirOpens = fs.DirOpens - _StepFs0.DirOpens;
        d.DirReads = fs.DirReads - _StepFs0.DirReads; d.Reads = fs.Reads - _StepFs0.Reads; d.Seeks = fs.Seeks - _StepFs0.Seeks;
        d.Closes = fs.Closes - _StepFs0.Closes; d.BytesRead = fs.BytesRead - _StepFs0.BytesRead; d.BytesWritten = fs.BytesWritten - _StepFs0.BytesWritten;
        double io = fs.TotalMs() - _StepFs0.TotalMs();
        double cpu = (_Cfg.CpuMs() - _StepCpu0) * FSO::Files::SdCostModel::CpuFactor;
        char b[96];
        std::snprintf(b, sizeof b, " | est. 3DS %.1f s (sd %.1f + cpu %.1f)", (io + cpu) / 1000.0, io / 1000.0, cpu / 1000.0);
        est = b;
    }
    std::printf("[%8.3f s]   %s %s: %.2f s%s | %s%s%s\n", (now - _LoadT0) / 1000.0, tag, _LoadStep.c_str(), (now - _StepT0) / 1000.0, est.c_str(),
                FsDelta(fs, _StepFs0, FSO::Files::FilePool::Stats, _StepIo0).c_str(), extra.empty() ? "" : " | ", extra.c_str());
    std::fflush(stdout);
    _LastLogLine = now;
}

void App::Heartbeat() {
    if (!_Cfg.LogSteps) return;
    double now = Now();
    if (now - _LastLogLine < 2000) return;
    LogStepCost("...still in", _Cfg.MemInfo ? _Cfg.MemInfo() : std::string());
}

void App::BeginStep(const std::string& what) { FSO::Files::LoadProgress::Step(what); }

void App::InstallLoadHooks() {
    using FSO::Files::LoadProgress;
    _LoadT0 = Now();
    _StepT0 = _LoadT0;
    if (_Cfg.CpuMs) _StepCpu0 = _Cfg.CpuMs();
    g_LogApp = this;
    FSO::Files::FsOps::OnOp = []() { if (g_LogApp) g_LogApp->Heartbeat(); };
    LoadProgress::StepSink = [this](const std::string& what) {
        if (_Cfg.LogSteps) LogStepCost("done"); // what the previous step cost
        _LoadStep = what;
        _StepIoStart = FSO::Files::FilePool::Stats.BytesRead;
        _StepT0 = Now();
        _StepFs0 = FSO::Files::Fs;
        _StepIo0 = FSO::Files::FilePool::Stats;
        if (_Cfg.CpuMs) _StepCpu0 = _Cfg.CpuMs();
        if (_Saving) SaveOverlayReport(what, 0, 0, true); // ADDED(sims) save progress
        if (_Mode == Mode::Loading && _Front) { // FIX(sims) FB1: the classic lot loading screen follows the steps
            _Front->LotLoadStep(what);
            PumpLoadAudio();
            if (Present) { _LastProgressDraw = Now(); LoadProgressFrame(std::string(), -1.0f); }
        }
        if (_Mode == Mode::Front && _Front && _Front->InSubScreen() && Present) {
            // the sub screen's loading state shows the step: "cas: skin catalogue" -> "Skin catalogue"
            std::string label = what;
            size_t colon = label.find(": ");
            if (colon != std::string::npos && colon < 10) label = label.substr(colon + 2);
            if (!label.empty()) label[0] = (char)std::toupper((unsigned char)label[0]);
            _LastProgressDraw = Now();
            LoadProgressFrame(label, -1.0f);
        }
        if (!_Cfg.LogSteps) return;
        auto& io = FSO::Files::FilePool::Stats;
        std::printf("[%8.3f s] %s | %s | io %.1f MB, %llu opens, %llu seeks\n", (Now() - _LoadT0) / 1000.0, what.c_str(),
                    _Cfg.MemInfo ? _Cfg.MemInfo().c_str() : "", io.BytesRead / 1048576.0, (unsigned long long)io.Opens,
                    (unsigned long long)io.Seeks);
        std::fflush(stdout);
        _LastLogLine = Now();
    };
    LoadProgress::ProgressSink = [this](const std::string& what, size_t done, size_t total) {
        double now = Now();
        if (_Mode == Mode::Loading) PumpLoadAudio(); // FIX(sims) FB1
        if (_Cfg.LogSteps && now - _LastLogLine > 2000) {
            char b[160];
            std::snprintf(b, sizeof b, "%s %zu/%zu", what.c_str(), done, total);
            LogStepCost("...still in", std::string(b) + (_Cfg.MemInfo ? " | " + _Cfg.MemInfo() : std::string()));
        }
        // ADDED(sims) save progress: the "Saving..." overlay; a save's reports never draw another screen (Bulldoze
        // saves from the neighbourhood screen, which would otherwise show the boot loading screen)
        if (_Saving) { SaveOverlayReport(what, done, total, false); return; }
        if (what.compare(0, 6, "save: ") == 0 || what == "front: neighbourhood map") return;
        if (now - _LastProgressDraw < 150 || !Present) return;
        _LastProgressDraw = now;
        char buf[160];
        auto& io = FSO::Files::FilePool::Stats;
        if (total > 0) std::snprintf(buf, sizeof buf, "%s: %zu / %zu", what.c_str(), done, total);
        else std::snprintf(buf, sizeof buf, "%s: %zu (%.1f MB read)", what.c_str(), done, (io.BytesRead - _StepIoStart) / 1048576.0);
        LoadProgressFrame(buf, total > 0 ? (float)done / (float)total : -1.0f);
    };
}

// ADDED(sims) save progress: the "Saving..." overlay (front/SaveOverlay.h). The save steps report through
// files/LoadProgress.h: front/LotSave "save: house pictures" (one report per World::Draw pass of the two neighbourhood
// pictures), VMTS1LotSaver "save: house file" (records, the other houses' type tables, the file), and
// TS1NeighbourhoodWriter "save: neighbourhood". Each report pumps the sound (PumpLoadAudio) and the overlay is redrawn
// and shown at most every kSaveRedrawMs; the reports are placed so that no stretch between two of them takes more than
// ~0.18 s on the 3DS (host check: the "save: ... longest without a redraw" log line, in the 3DS estimate; ~0.2 s).
// The share of each step in the bar follows its cost on the 3DS (host estimate, House05).
namespace {
struct SavePhase { const char* Step; float From, To; const char* Label; };
const SavePhase kSavePhases[] = {
    { "save: house pictures",             0.00f, 0.78f, "Drawing house picture" },   // ~3.3 s
    { "save: house file",                 0.78f, 0.98f, "Writing house file" },      // ~0.5-0.9 s
    { "front: back to the neighbourhood", 0.98f, 0.98f, "Writing neighbourhood" },
    { "save: neighbourhood",              0.98f, 1.00f, "Writing neighbourhood" },   // < 0.1 s
    // leaving the lot: the overlay stays up (at 100 %) while the neighbourhood map loads (~3.5 s), which reports per lot
    // and per picture of its art (NhoodModel::LoadArea, NeighbourhoodScreen::BuildArt)
    { "front: neighbourhood map",         1.00f, 1.00f, "Loading the neighbourhood" },
};
}

/// Redraw interval of the overlay. A redraw (copy of the composed screens, two lines of text, the bar, and the
/// platform's blit of both screens) is a few ms on the 3DS, so this costs little and keeps the time between two shown
/// frames under ~kSaveRedrawMs + the longest stretch between two reports.
static const double kSaveRedrawMs = 50;

double App::SaveClock() const {
    if (_Cfg.CpuMs) return _Cfg.CpuMs() * FSO::Files::SdCostModel::CpuFactor + FSO::Files::Fs.TotalMs();
    return Now();
}

void App::BeginSaveOverlay() {
    if (_Saving) return;
    std::string lotName;
    if (_Session && _Front) if (auto lot = _Front->Model().Lot(_Session->HouseNumber)) lotName = lot->Name.empty() ? lot->Address : lot->Name;
    if (!_SaveView) _SaveView.reset(new front::SaveOverlay());
    _SaveView->Begin(Top, Bottom, lotName);
    _Saving = true;
    _SaveFraction = 0;
    _SaveStepLabel = kSavePhases[0].Label;
    _SaveGapMax = 0;
    _SaveGapWhere.clear();
    _SaveDraws = 0;
    _SaveBegin = SaveClock();
    _SaveLastDraw = _SaveBegin;
    SaveOverlayDraw();
}

void App::SaveOverlayReport(const std::string& what, size_t done, size_t total, bool step) {
    for (auto& ph : kSavePhases) {
        if (what != ph.Step) continue;
        float f = ph.From;
        if (!step && total > 0) f = ph.From + (ph.To - ph.From) * std::min(1.0f, (float)done / (float)total);
        _SaveFraction = std::max(_SaveFraction, f);
        _SaveStepLabel = ph.Label;
        break;
    }
    PumpLoadAudio(); // the lot's sounds / music keep playing
    if (step || SaveClock() - _SaveLastDraw >= kSaveRedrawMs) SaveOverlayDraw();
}

void App::SaveOverlayDraw() {
    _SaveView->Draw(Top, Bottom, _SaveStepLabel, _SaveFraction);
    double t = SaveClock(); // what the screen stood still for, up to this redraw (the platform's Present not included)
    if (t - _SaveLastDraw > _SaveGapMax) { _SaveGapMax = t - _SaveLastDraw; _SaveGapWhere = _LoadStep; }
    _SaveDraws++;
    if (Present) Present();
    _SaveLastDraw = SaveClock();
}

void App::EndSaveOverlay() {
    if (!_Saving) return;
    // TS1 shows nothing when a save is done (no "saved" message in the game), so the overlay just closes at 100 %
    _SaveFraction = 1.0f;
    SaveOverlayDraw();
    char b[256];
    std::snprintf(b, sizeof b, "save: %d redraws in %.1f s%s, longest without a redraw %.0f ms (in '%s')\n", _SaveDraws,
                  (SaveClock() - _SaveBegin) / 1000.0, _Cfg.CpuMs ? " (3DS estimate)" : "", _SaveGapMax, _SaveGapWhere.c_str());
    Log += b;
    if (_Cfg.LogSteps) { std::printf("%s", b); std::fflush(stdout); }
    _Saving = false;
    _SaveView->End();
}

/// Redraws the current loading screen with a progress line during a blocking load and shows it (App::Present).
void App::LoadProgressFrame(const std::string& text, float fraction) {
    if (_Mode == Mode::Front && _Front && _Front->InSubScreen()) {
        if (!_Front->DrawSubProgress(text, fraction, Top, Bottom)) return; // TS1(sims): Create-A-Sim opening
    } else if (_Mode == Mode::Front && _Front) {
        _Front->DrawBootProgress(text, fraction, Top, Bottom);
    } else if (_Mode == Mode::Loading && _Front) {
        // FIX(sims) FB1: the classic loading screen shows the detail line itself (touch screen, above its bar)
        if (!text.empty()) _Front->LotLoadDetail(text);
        (void)fraction;
        if (_LoadTravel) _Front->DrawTravelLoading(_LoadHouse, _TravelFrom, Top, Bottom);
        else _Front->DrawLotLoading(_LoadHouse, Top, Bottom);
    } else {
        return;
    }
    Present();
}

void App::FrontFrame(const AppInput& in, double dtMs) {
    _Front->Frame(in, dtMs, Top, Bottom);
    if (!_Front->Log.empty()) { Log += _Front->Log; _Front->Log.clear(); }
    if (_Front->QuitRequested) { QuitRequested = true; return; }
    if (_Front->RequestedLot >= 0) {
        _LoadHouse = _Front->RequestedLot;
        _LoadFrames = 0;
        _Mode = Mode::Loading;
    }
}

void App::FinishLeave(bool save, bool quit) {
    int16_t family = -1;
    int budget = -1;
    if (_Session) {
        budget = _Session->Budget();
        auto c = FSO::Content::Content::Get();
        auto fam = c && c->Neighborhood ? c->Neighborhood->GetFamilyForHouse((int16_t)_Session->HouseNumber) : nullptr;
        if (fam) family = (int16_t)fam->ChunkID;
        if (save) {
            // TS1(sims): the house file and the lot's Sims (front/LotSave); ReturnFromLot then writes the neighbourhood
            BeginSaveOverlay(); // ADDED(sims) save progress (closed once the neighbourhood is written)
            std::string err;
            if (!front::SaveLot(*_Session, err, Log)) Log += "house save failed: " + err + "\n";
        }
    }
    _Lot.reset();
    _Session.reset();
    front::FrontLotUnloaded();
    ReleaseLotContent(Log);
    FSO::Files::LoadProgress::Step("front: back to the neighbourhood"); // TS1(sims) hardware pass
    _Front->ReturnFromLot(family, budget, save);
    EndSaveOverlay(); // ADDED(sims) save progress
    FSO::Files::LoadProgress::Step("front: neighbourhood ready");
    if (!_Front->Log.empty()) { Log += _Front->Log; _Front->Log.clear(); }
    Log += std::string("left the lot") + (save ? " (saved)" : "") + "\n";
    if (quit) { QuitRequested = true; return; }
    _Mode = Mode::Front;
}

/// Before the lot UI sees the input: the options panel's game buttons and their dialogs. True = input consumed.
bool App::LotFlow(const AppInput& in) {
    auto& T = front::FrontStrings::Get();
    if (_LotDialog->Open) { _LotDialog->Update(in); return true; }
    auto act = front::GameOptions::Get().TakeAction();
    if (_Lot && _Lot->LeaveRequested) { _Lot->LeaveRequested = false; act = front::GameOptions::Action::Neighbourhood; }
    if (_Lot && _Lot->QuitRequested) { _Lot->QuitRequested = false; act = front::GameOptions::Action::Quit; }
    std::vector<std::string> ync = { T.Get(152, 2, "Yes"), T.Get(152, 3, "No"), T.Get(152, 1, "Cancel") };
    switch (act) {
        case front::GameOptions::Action::Neighbourhood:
            _LotDialog->Show(T.Get(153, 3, "Save"), T.Get(153, 4, "Do you want to save the game?"), ync, [this](int r) {
                if (r == 2) return;
                _PendingLeave = r == 0 ? 1 : 2;
            });
            return true;
        case front::GameOptions::Action::Quit:
            _LotDialog->Show(T.Get(153, 5, "Quit?"), T.Get(153, 6, "Do you want to save before quitting?"), ync, [this](int r) {
                if (r == 2) return;
                _PendingLeave = r == 0 ? 3 : 4;
            });
            return true;
        case front::GameOptions::Action::Save: {
            int16_t family = -1;
            auto c = FSO::Content::Content::Get();
            auto fam = c && c->Neighborhood && _Session ? c->Neighborhood->GetFamilyForHouse((int16_t)_Session->HouseNumber) : nullptr;
            if (fam) family = (int16_t)fam->ChunkID;
            if (family >= 0) _Front->Model().SetBudget(family, _Session->Budget());
            // TS1(sims): the house (objects, walls/floors, Sims' state, time, picture: front/LotSave ->
            // VMTS1LotSaver), then the neighbourhood (NhoodModel::Save -> TS1NeighbourhoodWriter).
            BeginSaveOverlay(); // ADDED(sims) save progress: "Saving..." with the bar while the house is saved
            std::string err;
            bool ok = _Session && front::SaveLot(*_Session, err, Log);
            if (!ok) Log += "house save failed: " + err + "\n";
            ok = _Front->SaveNeighbourhood() && ok;
            EndSaveOverlay();
            // ADDED(sims) save progress: like TS1, a successful save just returns to the game (the overlay was the
            // feedback; the port's "The game has been saved." box is gone); a failure keeps the game's error box
            if (!ok) _LotDialog->Show(T.Get(153, 15, "Save House"), T.Get(153, 14, "An error was encountered saving the house."), { T.Get(152, 0, "OK") }, nullptr);
            Log += ok ? "saved house and neighbourhood\n" : "save failed\n";
            return true;
        }
        default:
            break;
    }
    return false;
}

}
