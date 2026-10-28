/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims front end — the front end's music / UI sounds through the sound engine (src/audio/GameAudio.h,
// written separately). Guarded: without src/audio the hooks do nothing.
//
// The neighbourhood screens use the music of their area (Music/Modes: Nhood, dt_nhood, Vacation, Superstar,
// MakinMagic), the loading screen the load loop (bkground_load, Music/Modes/Load/loadloop.wav, as FreeSims'
// LoadingScreen plays UIMusic.LoadLoop), Create-A-Family bkground_family, the credits bkground_credits.
#include "app/front/FrontAudio.h"
#if defined(__has_include)
#if __has_include("audio/GameAudio.h")
#include "audio/GameAudio.h"
#define SIMS_HAVE_AUDIO 1
#endif
#endif

namespace simapp { namespace front {

#ifdef SIMS_HAVE_AUDIO
using simaudio::GameAudio;
using simaudio::MusicMode;
using simaudio::NeighbourhoodKind;

void FrontMusic(const std::string& mode) {
    auto& a = GameAudio::Get();
    if (!a.Ready()) return;
    if (mode == "Load") a.SetMusicMode(MusicMode::Load);
    else if (mode == "Nhood") { a.SetNeighbourhood(NeighbourhoodKind::Main); a.SetMusicMode(MusicMode::Neighborhood); }
    else if (mode == "dt_nhood") { a.SetNeighbourhood(NeighbourhoodKind::Downtown); a.SetMusicMode(MusicMode::Downtown); }
    else if (mode == "Vacation") { a.SetNeighbourhood(NeighbourhoodKind::Vacation); a.SetMusicMode(MusicMode::Vacation); }
    else if (mode == "Superstar") { a.SetNeighbourhood(NeighbourhoodKind::StudioTown); a.SetMusicMode(MusicMode::StudioTown); }
    else if (mode == "MakinMagic") { a.SetNeighbourhood(NeighbourhoodKind::MagicTown); a.SetMusicMode(MusicMode::MagicTown); }
    else if (mode == "Family") a.SetMusicMode(MusicMode::Family);
    else if (mode == "Credits") a.SetMusicMode(MusicMode::Credits);
    else if (mode == "Options") a.SetMusicMode(MusicMode::Options);
    else if (mode == "Live") a.SetMusicMode(MusicMode::Live);
    else a.SetMusicMode(MusicMode::Fade);
}

void FrontSound(const std::string& evt) {
    auto& a = GameAudio::Get();
    if (a.Ready()) a.PlayUI(evt);
}

void FrontVolumes(int sfx, int music, int vox) {
    auto& a = GameAudio::Get();
    if (a.Ready()) a.SetVolumes(sfx / 100.0f, music / 100.0f, vox / 100.0f);
}

bool FrontAudioAvailable() { return GameAudio::Get().Ready(); }
void FrontLotUnloaded() { auto& a = GameAudio::Get(); if (a.Ready()) a.LeaveLot(); }
// ADDED(sims): the intro's sound
double FrontIntroSound(const std::string& path) {
    auto& a = GameAudio::Get();
    if (path.empty()) { a.StopFile(); return -1; }
    return a.PlayFile(path) ? a.OutputLatencyMs() : -1;
}
#else
void FrontMusic(const std::string&) {}
void FrontSound(const std::string&) {}
void FrontVolumes(int, int, int) {}
bool FrontAudioAvailable() { return false; }
void FrontLotUnloaded() {}
double FrontIntroSound(const std::string&) { return -1; } // ADDED(sims)
#endif

}}
