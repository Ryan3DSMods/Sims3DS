/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims SimsVille/HIT/HITResourceGroup.cs (namespace TSO.HIT.model -> FSO::HIT)
#pragma once
#include <memory>
#include <string>
#include <vector>
#include "files/HIT/EVT.h"
#include "files/HIT/HITFile.h"
#include "files/HIT/HSM.h"
#include "files/HIT/Hot.h"

namespace FSO { namespace HIT {

/// <summary>
/// Groups related HIT resources, like the tsov2 series or newmain.
/// </summary>
class HITResourceGroup
{
public:
    std::shared_ptr<FSO::Files::HIT::EVT> evt;
    std::shared_ptr<FSO::Files::HIT::HITFile> hit;
    std::shared_ptr<FSO::Files::HIT::HSM> hsm;

    // TS1(sims): The Sims 1 banks (SoundData/SimsGeneratedHitSource, SimsSound, each expansion pack and
    // download) describe their events/tracks/patches/hitlists in a .hot file instead of TSO's .evt + DBPF tracks.
    std::shared_ptr<FSO::Files::HIT::Hot> hot;
    std::string Name;             // e.g. "SoundData/SimsGeneratedHitSource"
    std::string Dir;              // folder of the bank (its FAR archives hold the samples)
    int LoadPriority = 0;         // [Options] LoadPriority: expansion banks (2) override the base game (1)
    std::vector<int> LocalFars;   // indices into Content::Audio's archive table: archives next to the bank

    /// TS1(sims): HIT branch/call operands are TrackData label ids; returns the code address of a label
    /// (or the value itself when it is not a label, i.e. already an address).
    uint32_t ResolveLabel(uint32_t label) const
    {
        if (hot != nullptr)
        {
            auto it = hot->TrackData.find(label);
            if (it != hot->TrackData.end()) return it->second;
        }
        return label;
    }
    bool IsLabel(uint32_t label) const { return hot != nullptr && hot->TrackData.count(label) > 0; }
};

}} // namespace FSO::HIT
