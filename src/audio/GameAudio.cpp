/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims audio — the game's sound facade.
#include "audio/GameAudio.h"
#include <cstdio>
#include <vector> // ADDED(sims)
#include "content/Audio.h"
#include "hit/HITTVOn.h"
#include "hit/HITVM.h"
#include "audio/WavFile.h" // ADDED(sims): PlayFile

namespace simaudio {

using FSO::HIT::HITVM;
using FSO::HIT::HITVolumeGroup;

GameAudio& GameAudio::Get() {
    static GameAudio instance;
    return instance;
}

bool GameAudio::Init(const Config& cfg) {
    Shutdown();
    _Cfg = cfg;
    auto content = FSO::Content::Audio::Init(cfg.DataRoot, cfg.SampleCacheBytes);
    if (content == nullptr || content->Groups.empty()) {
        std::printf("audio: no sound banks under %s/SoundData - sound disabled\n", cfg.DataRoot.c_str());
        FSO::Content::Audio::Shutdown();
        return false;
    }
    _Mixer.reset(new AudioMixer(cfg.OutputRate, cfg.Voices));
    HITVM::Init(_Mixer.get());
    auto vm = HITVM::Get();
    vm->EventLog = [this](const std::string& evt, const std::string& info) { if (Log) Log(evt + ": " + info); };
    // music the lot itself asks for (the community-lot controllers' station_* events): live mode returns to it
    vm->OnMusicEvent = [this](const std::string& evt) {
        if (_Requesting) return;
        _LotMusicEvent = evt;
        _ModeSet = false; // the next SetMusicMode call takes effect again
    };
    _Ready = true;
    SetVolumes(cfg.SfxVolume, cfg.MusicVolume, cfg.VoiceVolume);
    std::printf("audio: %d banks, %zu samples indexed, mixer %d Hz x %d voices\n", (int)content->Groups.size(),
                content->IndexedSamples(), cfg.OutputRate, cfg.Voices);
    return true;
}

void GameAudio::SetBackend(std::unique_ptr<AudioBackend> backend) {
    if (_Backend) _Backend->Stop();
    _Backend = std::move(backend);
    if (_Backend && _Mixer) _Backend->Start(*_Mixer);
}

void GameAudio::Shutdown() {
    if (_Backend) { _Backend->Stop(); _Backend.reset(); }
    HITVM::Shutdown();
    _Mixer.reset();
    FSO::Content::Audio::Shutdown();
    _Ready = false;
    _ModeSet = false;
    _FileVoice = 0; // ADDED(sims)
    _DeviceReleased = false;
}

void GameAudio::Update(double dtMs) {
    if (!_Ready) return;
    _Accum += dtMs;
    int ticks = 0;
    const double tick = 1000.0 / 60.0;
    while (_Accum >= tick && ticks < 15) { // after a long stall, catch up at most 1/4 s
        HITVM::Get()->Tick();
        _Accum -= tick;
        ticks++;
    }
    if (_Accum > tick) _Accum = 0;
}

void GameAudio::PlayUI(const std::string& evt) {
    if (!_Ready) return;
    HITVM::Get()->PlaySoundEvent(evt);
}

void GameAudio::SpeedChanged(int from, int to) {
    if (from == to) return;
    auto n = [](int s) { return s <= 0 ? std::string("p") : std::to_string(s > 3 ? 3 : s); };
    PlayUI("ui_speed_" + n(from) + "to" + n(to));
}

static const char* ModeEvent(MusicMode m) {
    switch (m) {
    case MusicMode::Live: return "bkground_live";
    case MusicMode::Buy: return "bkground_buy1";
    case MusicMode::Build: return "bkground_build";
    case MusicMode::Neighborhood: return "bkground_nhood1";
    case MusicMode::Load: return "bkground_load";
    case MusicMode::Credits: return "bkground_credits";
    case MusicMode::Options: return "bkground_options";
    case MusicMode::Family: return "bkground_family";
    case MusicMode::Fade: return "bkground_fade";
    case MusicMode::Downtown: return "station_dtnhood";
    case MusicMode::Vacation: return "station_vacation";
    case MusicMode::OldTown: return "station_unleashed";
    case MusicMode::StudioTown: return "station_superstar";
    case MusicMode::StudioTownTransition: return "music_superstar_transition";
    case MusicMode::MagicTown: return "music_magictown";
    case MusicMode::MagicTownCredits: return "music_magictown_credits";
    case MusicMode::MagicTownBuild: return "music_magictown_build";
    case MusicMode::MagicTownBuy: return "music_magictown_buy";
    }
    return "bkground_fade";
}

void GameAudio::SetMusicMode(MusicMode mode) {
    if (!_Ready) return;
    if (mode == MusicMode::Family) mode = MusicMode::Neighborhood; // Create-A-Family keeps the neighbourhood tune
    // the expansion neighbourhoods have their own score for the same screens
    MusicMode m = mode;
    switch (_Nhood) {
    case NeighbourhoodKind::MagicTown:
        if (mode == MusicMode::Build) m = MusicMode::MagicTownBuild;
        else if (mode == MusicMode::Buy) m = MusicMode::MagicTownBuy;
        else if (mode == MusicMode::Neighborhood) m = MusicMode::MagicTown;
        break;
    case NeighbourhoodKind::Downtown:
        if (mode == MusicMode::Neighborhood || mode == MusicMode::Buy) m = MusicMode::Downtown; // station_dtnhood/dtbuy
        break;
    case NeighbourhoodKind::Vacation:
        if (mode == MusicMode::Neighborhood) m = MusicMode::Vacation;
        break;
    case NeighbourhoodKind::OldTown:
        if (mode == MusicMode::Neighborhood) m = MusicMode::OldTown;
        break;
    case NeighbourhoodKind::StudioTown:
        if (mode == MusicMode::Neighborhood) m = MusicMode::StudioTown;
        break;
    default: break;
    }
    if (_ModeSet && m == _Mode) return;
    _Mode = m;
    _ModeSet = true;
    std::string evt = ModeEvent(m);
    if (HITVM::Get()->GetEvent(evt) == nullptr) evt = ModeEvent(mode); // pack not installed: base mode
    // live mode on a lot whose objects picked a station (Downtown jazz...) goes back to that station, not silence
    if (m == MusicMode::Live && !_LotMusicEvent.empty()) evt = _LotMusicEvent;
    _Requesting = true;
    HITVM::Get()->PlaySoundEvent(evt);
    _Requesting = false;
}

void GameAudio::SetMusicEvent(const std::string& evt) {
    if (!_Ready) return;
    _Requesting = true;
    HITVM::Get()->PlaySoundEvent(evt);
    _Requesting = false;
    _ModeSet = false;
}

std::string GameAudio::NowPlaying() const {
    return _Ready ? HITVM::Get()->MusicStation() : std::string();
}

void GameAudio::SetVolumes(float sfx, float music, float voice) {
    _Cfg.SfxVolume = sfx; _Cfg.MusicVolume = music; _Cfg.VoiceVolume = voice;
    if (!_Ready) return;
    auto vm = HITVM::Get();
    vm->SetMasterVolume(HITVolumeGroup::FX, sfx);
    vm->SetMasterVolume(HITVolumeGroup::MUSIC, music);
    vm->SetMasterVolume(HITVolumeGroup::VOX, voice);
    vm->SetMasterVolume(HITVolumeGroup::AMBIENCE, sfx);
}

void GameAudio::LeaveLot() {
    _LotMusicEvent.clear();
    if (!_Ready) return;
    HITVM::Get()->KillOwned();
}

// ADDED(sims): one-shot sound files (the EA intro's soundtrack).
bool GameAudio::PlayFile(const std::string& path, float volume) {
    StopFile();
    if (!_Ready || !_Mixer) return false;
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::vector<uint8_t> data;
    uint8_t chunk[16384];
    size_t n;
    while ((n = std::fread(chunk, 1, sizeof chunk, f)) > 0) data.insert(data.end(), chunk, chunk + n);
    std::fclose(f);
    std::shared_ptr<SoundBuffer> buf = DecodeWav(data.data(), data.size());
    if (!buf || buf->Frames() == 0) {
        std::printf("audio: %s is not a PCM WAV - not played\n", path.c_str());
        return false;
    }
    _FileVoice = _Mixer->Play(buf, volume, 0.0f, false, AudioMixer::GroupMusic);
    if (Log) {
        char info[160];
        std::snprintf(info, sizeof info, "%s (%.2f s, %d Hz, %d ch)%s", path.c_str(), buf->Seconds(), buf->Rate,
                      buf->Channels, _FileVoice ? "" : " - no free voice");
        Log(std::string("file play: ") + info);
    }
    return _FileVoice != 0;
}

void GameAudio::StopFile() {
    if (_FileVoice == 0) return;
    if (_Mixer) {
        bool was = _Mixer->IsPlaying(_FileVoice);
        _Mixer->Stop(_FileVoice);
        if (Log) Log(was ? "file stop: stopped" : "file stop: had already ended");
    }
    _FileVoice = 0;
}

bool GameAudio::FilePlaying() const {
    return _FileVoice != 0 && _Mixer && _Mixer->IsPlaying(_FileVoice);
}

void GameAudio::SetSuspended(bool suspended) {
    if (_Backend) _Backend->SetSuspended(suspended);
}

void GameAudio::ReleaseDevice() { // FIX(sims) HOME crash
    if (_Backend && !_DeviceReleased) { _Backend->Stop(); _DeviceReleased = true; }
}

void GameAudio::ReacquireDevice() { // FIX(sims) HOME crash
    if (_Backend && _DeviceReleased && _Mixer) _Backend->Start(*_Mixer);
    _DeviceReleased = false;
}

}
