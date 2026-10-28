/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims SimsVille/HIT/HITTVOn.cs
#include "hit/HITTVOn.h"
#include <algorithm>
#include "audio/AudioMixer.h"
#include "audio/Mp3Stream.h"
#include "audio/WavFile.h"
#include "content/Audio.h"
#include "files/HIT/HITText.h"
#include "hit/HITVM.h"

namespace FSO { namespace HIT {

namespace Text = FSO::Files::HIT::Text;

void HITTVOn::Kill()
{
    if (Instance != 0)
    {
        KillLast();
    }
    Dead = true;
}

float HITTVOn::CurrentVolume() const
{
    // C#: Instance.Volume = InstVolume * GetVolFactor(). The group factor is applied by the mixer's group volume.
    // TS1(sims): an owned TV/radio also follows its positional volume (VMEntity.TickSounds), the station
    // track's volume and the sting ducking.
    float v = InstVolume * TrackVolume;
    if (EverHadOwners) v *= Volume;
    if (VM != nullptr) v *= VM->DuckFactor(this);
    return v;
}

bool HITTVOn::Tick()
{
    if (Dead) return false;
    if (EverHadOwners && Owners.size() == 0)
    {
        Kill();
        return false;
    }

    auto mixer = VM != nullptr ? VM->Mixer : nullptr;
    if (Paused) return true;

    if (Instance != 0 && mixer != nullptr && FadeOut <= 0)
    {
        mixer->SetVolumePan(Instance, CurrentVolume(), (!IsMusic || EverHadOwners) ? Pan : 0.0f);
    }

    if (FadeOut > 0)
    {
        FadeOut--;
        if (Instance != 0 && mixer != nullptr) mixer->SetVolumePan(Instance, std::max(0.0f, ((FadeOut - 60) / 120.0f)) * CurrentVolume(), 0.0f);
        if (FadeOut == 0)
        {
            Kill();
            return false;
        }
    }

    VolumeSet = false;

    if (Instance == 0 || mixer == nullptr || !mixer->IsPlaying(Instance))
    {
        if (FadeOut > 0) return true; // fading out: don't start the next song
        if (PlayNext()) return true;
        else {
            Dead = true;
            return false;
        }
    }
    return true;
}

HITTVOn::HITTVOn(uint32_t TrackID, HITVM* vm)
{
    this->VM = vm;
    // C#: Station = the 4 characters of TrackID ("KCOU"), looked up in StationPaths.
    // TS1(sims): The Sims' station tracks are numbers (stereo_country -> 260, tv_action_* -> 28 ...).
    auto content = FSO::Content::Audio::Get();
    if (content == nullptr) return;
    auto& paths = content->StationPaths();
    auto it = paths.find(TrackID);
    if (it != paths.end())
    {
        Station = it->second.Label;
        IsTV = it->second.TV;
        LoadStation(CleanPath(it->second.Path));
    }
    auto trk = content->GetTrack(TrackID);
    if (trk != nullptr && trk->Volume > 0) TrackVolume = std::min(1.0f, trk->Volume / 1024.0f);
}

HITTVOn::HITTVOn(uint32_t id, HITVM* vm, bool IDMode)
{
    (void)IDMode;
    this->VM = vm;
    Station = "";

    auto aud = FSO::Content::Audio::Get();
    if (aud == nullptr) return;
    auto it = aud->MusicModes().find((int)id);
    if (it != aud->MusicModes().end())
    {
        Station = it->second;
        if (!Station.empty()) LoadStation(Station);
    }
}

std::string HITTVOn::CleanPath(const std::string& path)
{
    // C#: path.ToLowerInvariant(). TS1(sims): the install keeps its mixed case ("Music/Stations/Country"),
    // which matters on case-sensitive host file systems, so music paths are kept as written.
    return IsTV ? Text::ToLower(path) : path;
}

void HITTVOn::Fade()
{
    if (FadeOut == -1) FadeOut = 60 * 3;
}

void HITTVOn::LoadStation(const std::string& path)
{
    auto content = FSO::Content::Audio::Get();
    if (IsTV)
    {
        //tv.
        //include commercials from previous directory
        // TS1(sims): a station is a set of shows (TVStations/<station>/a, b, c ...: a few clips each) plus
        // the commercials of its pack. The C# shuffled every clip together; here a show's clips stay together and
        // a commercial break follows each show, which is how the original channels sound.
        auto tv = content->GetTVStation(path);
        if (tv == nullptr) return;
        std::vector<size_t> shows(tv->Episodes.size());
        for (size_t i = 0; i < shows.size(); i++) shows[i] = i;
        for (size_t i = shows.size(); i > 1; i--) std::swap(shows[i - 1], shows[(size_t)HITVM::NextRandom((int)i)]);
        for (auto s : shows)
        {
            for (auto& clip : tv->Episodes[s]) Sounds.push_back(clip);
            if (!tv->Commercials.empty()) Sounds.push_back(tv->Commercials[(size_t)HITVM::NextRandom((int)tv->Commercials.size())]);
        }
    }
    else
    {
        //mp3 music
        auto files = content->ListMusic(path);
        IsMusic = true;

        for (auto& file : files)
        {
            Sounds.insert(Sounds.begin() + HITVM::NextRandom((int)Sounds.size() + 1), file);
        }
    }
}

void HITTVOn::KillLast()
{
    if (Instance == 0) return;

    if (VM != nullptr && VM->Mixer != nullptr) VM->Mixer->Stop(Instance);

    Instance = 0;
}

bool HITTVOn::PlayNext()
{
    if (Sounds.size() == 0) return false;
    auto mixer = VM != nullptr ? VM->Mixer : nullptr;
    if (mixer == nullptr) return false;
    KillLast();
    auto sound = Sounds[0];
    Sounds.erase(Sounds.begin());
    if (IsMusic) Sounds.insert(Sounds.begin() + (Sounds.size() + (size_t)HITVM::NextRandom(1)), sound); //put back at end, with a little bit of shuffle.
    else Sounds.push_back(sound); // TV: the show order repeats

    NowPlaying = sound;
    if (VM->EventLog) VM->EventLog(Name, std::string(IsMusic ? "music " : "tv ") + sound);

    if (IsMusic)
    {
        // C#: return false (MonoGame could not stream MP3s). TS1(sims): streamed, chunk by chunk.
        std::unique_ptr<simaudio::AudioStream> stream;
        bool loop = false;
        if (Text::ToLower(sound.substr(sound.size() >= 4 ? sound.size() - 4 : 0)) == ".wav")
        {
            stream = simaudio::WavStream::Open(sound);
            loop = (Sounds.size() == 1); // Music/Modes/Load/loadloop.wav loops seamlessly
        }
        else stream = simaudio::Mp3Stream::Open(sound);
        if (stream == nullptr) return false;
        Instance = mixer->PlayStream(std::move(stream), CurrentVolume(), EverHadOwners ? Pan : 0.0f, loop, (int)VolGroup);
        return Instance != 0;
    }
    else
    {
        auto content = FSO::Content::Audio::Get();
        auto sfx = content->GetSampleByName(sound);
        if (sfx == nullptr) return false;

        Instance = mixer->Play(sfx, CurrentVolume(), Pan, false, (int)VolGroup);
        return Instance != 0;
    }
}

void HITTVOn::Pause()
{
    Paused = true;
    if (Instance != 0 && VM != nullptr && VM->Mixer != nullptr) VM->Mixer->SetPaused(Instance, true);
}

void HITTVOn::Resume()
{
    Paused = false;
    if (Instance != 0 && VM != nullptr && VM->Mixer != nullptr) VM->Mixer->SetPaused(Instance, false);
}

void HITTVOn::Dispose()
{
}

}} // namespace FSO::HIT
