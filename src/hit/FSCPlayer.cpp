/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims SimsVille/HIT/FSCPlayer.cs
#include "hit/FSCPlayer.h"
#include "audio/AudioMixer.h"
#include "files/XA/XAFile.h"
#include "hit/HITVM.h"

namespace FSO { namespace HIT {

FSCPlayer::FSCPlayer(std::shared_ptr<FSO::Files::HIT::FSC> fsc, const std::string& basedir)
{
    this->fsc = fsc;
    this->BaseDir = basedir;

    BeatLength = (fsc->Tempo > 0) ? 60.0f / fsc->Tempo : 0.5f;
}

void FSCPlayer::SetManualTempo(int tempo)
{
    (void)tempo;
    BeatLength = (fsc->Tempo > 0) ? 60.0f / fsc->Tempo : 0.5f; // (sic) the C# ignores the argument
}

void FSCPlayer::SetVolume(float volume)
{
    Volume = volume;
}

void FSCPlayer::Tick(float time)
{
    auto mixer = HITVM::Get() ? HITVM::Get()->Mixer : nullptr;
    for (size_t i = 0; i < SoundEffects.size(); i++) //dispose and remove sound effect instances that are finished
    {
        if (mixer == nullptr || !mixer->IsPlaying(SoundEffects[i]))
        {
            SoundEffects.erase(SoundEffects.begin() + i);
            i--;
        }
    }

    TimeDiff += time;
    while (TimeDiff > BeatLength)
    {
        TimeDiff -= BeatLength;
        NextNote();
    }
}

simaudio::SoundBufferPtr FSCPlayer::LoadSound(const std::string& filename)
{
    auto it = SoundCache.find(filename);
    if (it != SoundCache.end()) return it->second;
    FSO::Files::XA::XAFile xa(BaseDir + filename);
    auto buf = std::make_shared<simaudio::SoundBuffer>();
    buf->Channels = xa.Channels() ? xa.Channels() : 1;
    buf->Rate = (int)xa.SampleRate();
    buf->Samples.assign(xa.Samples(), xa.Samples() + xa.SampleCount());
    SoundCache[filename] = buf;
    return buf;
}

void FSCPlayer::NextNote()
{
    if (fsc->Notes.empty()) return;
    if (LoopCount == -1)
    {
        auto note = fsc->Notes[(size_t)CurrentPosition++];
        if (CurrentPosition >= (int)fsc->Notes.size()) CurrentPosition = 0;
        if (note.Filename != "NONE")
        {
            bool play;
            if (note.Rand) play = (HITVM::NextRandom(16) < note.Prob);
            else play = true;
            (void)play; // (sic) the C# computes it and plays anyway

            float volume = (note.Volume / 1024.0f) * (fsc->MasterVolume / 1024.0f) * Volume;
            try
            {
                auto sound = LoadSound(note.Filename);
                auto mixer = HITVM::Get() ? HITVM::Get()->Mixer : nullptr;
                if (mixer != nullptr)
                {
                    int instance = mixer->Play(sound, volume, (note.LRPan / 512.0f) - 1, false, 3 /*AMBIENCE*/);
                    if (instance != 0) SoundEffects.push_back(instance);
                }
            }
            catch (std::exception&) {}
        }
        LoopCount = (int16_t)(note.Loop - 1);
    }
    else LoopCount--;
}

}} // namespace FSO::HIT
