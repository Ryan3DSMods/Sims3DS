/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims SimsVille/HIT/HITSound.cs (namespace TSO.HIT -> FSO::HIT)
#pragma once
#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "files/HIT/HITConstants.h"
#include "files/HIT/Track.h"

namespace FSO { namespace HIT {

class HITVM;

enum class HITVolumeGroup
{
    FX = 0,
    MUSIC = 1,
    VOX = 2,
    AMBIENCE = 3
};

class HITSound : public std::enable_shared_from_this<HITSound>
{
public:
    bool VolumeSet = false;
    float Volume = 1;
    float Pan = 0;
    float InstVolume = 1;
    float PreviousVolume = 1; //This is accessed by HitVM.Unduck()
    FSO::Files::HIT::Track* ActiveTrack = nullptr;

    HITVolumeGroup VolGroup = HITVolumeGroup::FX;

    FSO::Files::HIT::HITDuckingPriorities DuckPriority() const
    {
        if (ActiveTrack != nullptr)
            return ActiveTrack->DuckingPriority;
        else
            return FSO::Files::HIT::HITDuckingPriorities::duckpri_normal;
    }

    HITVM* VM = nullptr;

    bool EverHadOwners = false; //if we never had owners, don't kill the thread. (ui sounds)
    std::vector<int> Owners;

    bool Dead = false;

    std::string Name; // TS1(sims): the event that started this sound (logs, VMSoundEntry matching)

    HITSound() {}
    virtual ~HITSound() {}

    virtual bool Tick() = 0;

    // C#: not virtual (HITThread hides it with its own SetVolume). Virtual here so the VM's positional updates
    // through a HITSound reference reach HITThread's version, which also records PreviousVolume for Unduck.
    virtual void SetVolume(float volume, float pan)
    {
        if (VolumeSet)
        {
            if (volume > Volume)
            {
                Volume = volume;
                Pan = pan;
            }
        }
        else
        {
            Volume = volume;
            Pan = pan;
        }

        VolumeSet = true;
    }

    void AddOwner(int id)
    {
        EverHadOwners = true;
        Owners.push_back(id);
    }

    float GetVolFactor();

    void RemoveOwner(int id)
    {
        auto it = std::find(Owners.begin(), Owners.end(), id);
        if (it != Owners.end()) Owners.erase(it); // List.Remove: first occurrence
    }

    bool AlreadyOwns(int id) const
    {
        return std::find(Owners.begin(), Owners.end(), id) != Owners.end();
    }

    virtual void Pause() = 0;

    virtual void Resume() = 0;

    virtual void Dispose() = 0;
};

}} // namespace FSO::HIT
