/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims SimsVille/HIT/FSCPlayer.cs (namespace TSO.HIT -> FSO::HIT)
// FSC ambience sequences are a The Sims Online format (sounddata/ambience/*.fsc); The Sims 1 has none, but the
// player is kept for completeness.
#pragma once
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "audio/SoundBuffer.h"
#include "files/HIT/FSC.h"

namespace FSO { namespace HIT {

class FSCPlayer
{
    /// <summary>
    /// A Class to play FSC sequences. Bundled in with the HIT engine because it wouldn't really go anywhere else. :I
    /// </summary>
public:
    int CurrentPosition = 0;
    int16_t LoopCount = -1;
    float TimeDiff = 0;
private:
    std::shared_ptr<FSO::Files::HIT::FSC> fsc;
    std::string BaseDir;
    float BeatLength = 0.5f;
    float Volume = 1;
    std::vector<int> SoundEffects; // mixer voices

    std::map<std::string, simaudio::SoundBufferPtr> SoundCache;

public:
    FSCPlayer(std::shared_ptr<FSO::Files::HIT::FSC> fsc, const std::string& basedir);

    void SetManualTempo(int tempo);
    void SetVolume(float volume);
    void Tick(float time);

private:
    simaudio::SoundBufferPtr LoadSound(const std::string& filename);
    void NextNote();
};

}} // namespace FSO::HIT
