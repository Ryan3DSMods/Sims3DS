/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims SimsVille/HIT/AmbiencePlayer.cs
#include "hit/AmbiencePlayer.h"
#include "audio/AudioMixer.h"
#include "files/XA/XAFile.h"
#include "hit/FSCPlayer.h"
#include "hit/HITVM.h"

namespace FSO { namespace HIT {

AmbiencePlayer::AmbiencePlayer(const Ambience& amb, const std::string& basePath)
{
    auto vm = HITVM::Get();
    if (vm == nullptr) return;
    try
    {
        if (amb.Loop)
        {
            FSO::Files::XA::XAFile xa(basePath + "/" + amb.Path);
            auto buf = std::make_shared<simaudio::SoundBuffer>();
            buf->Channels = xa.Channels() ? xa.Channels() : 1;
            buf->Rate = (int)xa.SampleRate();
            buf->Samples.assign(xa.Samples(), xa.Samples() + xa.SampleCount());

            if (vm->Mixer != nullptr) inst = vm->Mixer->Play(buf, 1.0f, 0.0f, true, 3 /*AMBIENCE*/);
            fscMode = false;
        }
        else
        {
            fsc = vm->PlayFSC(basePath + "/" + amb.Path);
            fsc->SetVolume(0.33f); //may need tweaking
            fscMode = true;
        }
    }
    catch (std::exception&)
    {
        fsc = nullptr; // missing ambience file (always the case with The Sims 1 data)
    }
}

void AmbiencePlayer::Kill()
{
    auto vm = HITVM::Get();
    if (vm == nullptr) return;
    if (fscMode) { if (fsc != nullptr) vm->StopFSC(fsc); }
    else
    {
        if (vm->Mixer != nullptr && inst != 0) vm->Mixer->Stop(inst);
    }
}

}} // namespace FSO::HIT
