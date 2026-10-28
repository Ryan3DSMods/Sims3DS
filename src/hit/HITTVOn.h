/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims SimsVille/HIT/HITTVOn.cs (namespace FSO.HIT.Events -> FSO::HIT)
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "hit/HITThread.h"

namespace FSO { namespace HIT {

/// <summary>
/// A special HITThread replacement to handle the kTurnOnTV event. Used by radio, TV, and (in the original game) the UI music.
///
/// Also supports streaming MP3 audio.
/// </summary>
class HITTVOn : public HITThread
{
private:
    std::string Station;
    std::vector<std::string> Sounds;

    int Instance = 0; // C#: SoundEffectInstance -> mixer voice

    bool IsMusic = false;
    bool IsTV = false;        // TS1(sims): TV clips come from the sound archives, music from Music/
    float TrackVolume = 1.0f; // TS1(sims): the station track's volume (TVs 750/1024, radios 900/1024)

    int FadeOut = -1;

public:
    void Kill();

    bool Tick() override;

    HITTVOn(uint32_t TrackID, HITVM* vm);

    HITTVOn(uint32_t id, HITVM* vm, bool IDMode);

private:
    std::string CleanPath(const std::string& path);

public:
    void Fade();
    bool Fading() const { return FadeOut != -1; }

private:
    void LoadStation(const std::string& path);
    void KillLast();
    float CurrentVolume() const;

public:
    bool PlayNext();

    void Pause() override;
    void Resume() override;
    void Dispose() override;

    // TS1(sims)
    const std::string& StationName() const { return Station; }
    std::string NowPlaying;   // the file/clip on air
    bool IsMusicStream() const { return IsMusic; }
};

}} // namespace FSO::HIT
