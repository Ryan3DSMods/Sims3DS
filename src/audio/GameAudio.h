/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims audio — the game's sound facade (the API the app and the UI screens call). See src/audio/README.md.
//
//   GameAudio::Get().Init({dataRoot})        once at boot (indexes SoundData/, Music/, the expansion packs)
//   GameAudio::Get().Update(dtMs)            every frame (ticks the HIT VM at 60 Hz)
//   GameAudio::Get().PlayUI("ui_click")      UI sounds (UISounds names: ui_piemenu_appear, ui_queued, ui_buy_place ...)
//   GameAudio::Get().SetMusicMode(MusicMode::Buy)   mode music with the original 3 s cross-fade
//
// Object/Sim sounds need nothing from the UI: the SimAntics Play Sound primitive and animation "sound" events
// start them (src/simantics/primitives/VMPlaySound.cpp), positioned by the lot camera (VMEntity::TickSounds).
#pragma once
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include "audio/AudioBackend.h"
#include "audio/AudioMixer.h"

namespace simaudio {

/// kSetMusicMode ids (SimsSound.hot bkground_* and the packs' station_* / music_*).
enum class MusicMode : int {
    Live = 0,            // bkground_live: no score in live mode (radios/TVs only)
    Buy = 1,             // bkground_buy1
    Build = 2,           // bkground_build
    Neighborhood = 3,    // bkground_nhood1
    Load = 5,            // bkground_load (loadloop.wav)
    Credits = 6,         // bkground_credits
    Options = 7,         // bkground_options
    Family = 8,          // bkground_family (Create-A-Family)
    Fade = 9,            // bkground_fade (silence)
    Downtown = 10,       // station_dtnhood / station_dtbuy (Hot Date)
    Vacation = 11,       // station_vacation
    OldTown = 12,        // station_unleashed
    StudioTown = 13,     // station_superstar
    StudioTownTransition = 14, // music_superstar_transition
    MagicTown = 15,      // music_magictown
    MagicTownCredits = 16,
    MagicTownBuild = 17,
    MagicTownBuy = 18,
};

/// Which neighbourhood the player is in: picks the expansion variant of the mode music (Magic Town build/buy...).
enum class NeighbourhoodKind { Main, Downtown, Vacation, OldTown, StudioTown, MagicTown };

class GameAudio {
public:
    static GameAudio& Get();

    struct Config {
        std::string DataRoot;                // the install tree (SoundData/, Music/, ExpansionPack*/ ...)
        int OutputRate = 32728;              // 3DS DSP rate; the host may use anything
        int Voices = 32;
        size_t SampleCacheBytes = 6u << 20;  // decoded sample cache (the 3DS default keeps RAM low)
        float SfxVolume = 1.0f, MusicVolume = 0.75f, VoiceVolume = 1.0f;
    };

    /// Indexes the sound content and starts the HIT VM. False (and every call a no-op) when SoundData is missing.
    bool Init(const Config& cfg);
    /// Attaches a platform backend (started immediately). GameAudio owns it.
    void SetBackend(std::unique_ptr<AudioBackend> backend);
    AudioBackend* Backend() { return _Backend.get(); }
    void Shutdown();
    bool Ready() const { return _Ready; }
    AudioMixer* Mixer() { return _Mixer.get(); }

    /// Every frame: runs the HIT VM at 60 ticks/s of real time (sounds keep their pace whatever the game speed).
    void Update(double dtMs);

    /// UI / global sound event by name (UISounds.*: "ui_click", "ui_piemenu_appear", "ui_buy_place"...).
    void PlayUI(const std::string& evt);
    /// The speed-change click of TS1's control panel (ui_speed_1to2, ui_speed_3top ...). 0 = pause.
    void SpeedChanged(int from, int to);

    /// Mode music, with the original cross-fade. Repeating the current mode does not restart it.
    void SetMusicMode(MusicMode mode);
    /// Any kSetMusicMode event by name ("bkground_buy1", "station_dtnhood", "music_magictown_build").
    void SetMusicEvent(const std::string& evt);
    MusicMode CurrentMusicMode() const { return _Mode; }
    std::string NowPlaying() const;
    void SetNeighbourhood(NeighbourhoodKind kind) { _Nhood = kind; }

    /// Options panel sliders (0..1).
    void SetVolumes(float sfx, float music, float voice);
    /// Leaving/unloading a lot: stops every object's sound (radios, TVs, loops) — their VM is going away.
    void LeaveLot();
    /// Pauses everything (HOME menu, sleep). The game's own pause is handled by the VM (VM::Update).
    void SetSuspended(bool suspended);

    /// Log of every event played / note started (host tools); empty = off.
    std::function<void(const std::string&)> Log;

    int ActiveVoices() const { return _Mixer ? _Mixer->ActiveVoices() : 0; }

    // ADDED(sims): a one-shot sound file outside the HIT banks — the EA intro's soundtrack (<cache>/intro.wav, written by
    // app/tools/convert_intro.py). PCM WAV of any rate / channel count (the mixer resamples; 32728 Hz s16 stereo is
    // played as is), preloaded whole (the intro is ~0.4 MB) so the audio thread never reads the SD card for it, and
    // played on the music group (the options' music volume) next to whatever else plays. Replaces a file still playing.
    // False (silence, nothing else changes) when audio is not running or the file is missing / not a PCM WAV.
    bool PlayFile(const std::string& path, float volume = 1.0f);
    /// ADDED(sims): stops the file started by PlayFile (2 ms fade); a no-op when it already ended.
    void StopFile();
    /// ADDED(sims): true while the PlayFile sound is still playing.
    bool FilePlaying() const;
    /// ADDED(sims): how long after a mixer change it is heard (the backend's queued audio, ms), for A/V sync.
    double OutputLatencyMs() const { return _Backend ? _Backend->LatencyMs() : 0.0; }

private:
    GameAudio() {}
    bool _Ready = false;
    Config _Cfg;
    std::unique_ptr<AudioMixer> _Mixer;
    std::unique_ptr<AudioBackend> _Backend;
    double _Accum = 0;
    MusicMode _Mode = MusicMode::Fade;
    bool _ModeSet = false;
    NeighbourhoodKind _Nhood = NeighbourhoodKind::Main;
    bool _Requesting = false;     // inside our own SetMusicMode
    int _FileVoice = 0;           // ADDED(sims): the PlayFile voice
    std::string _LotMusicEvent;   // a station a lot object chose (Downtown / Vacation / Magic Town controllers)
};

}
