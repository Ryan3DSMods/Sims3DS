/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Model.Sound.VMSoundEntry / VMSoundTransfer
// (FreeSims SimsVille/SimsAntics/model/Sound/VMSoundEntry.cs, VMSoundTransfer.cs)
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include "hit/HITSound.h"
namespace FSO { namespace SimAntics { namespace Model { namespace Sound {

using HITSound = FSO::HIT::HITSound; // C#: TSO.HIT.HITSound

class VMSoundEntry {
public:
    // C# reference; shared with the HIT VM (which drops finished threads while an entity may still hold one)
    std::shared_ptr<HITSound> Sound;
    bool Pan = false;
    bool Zoom = false;
    bool Loop = false;
    std::string Name;
};

class VMSoundTransfer {
public:
    int16_t SourceID; //only copy on ID+GUID match. In future, match on persist too?
    uint32_t SourceGUID;
    VMSoundEntry SFX;
    VMSoundTransfer(int16_t id, uint32_t guid, const VMSoundEntry& sfx) : SourceID(id), SourceGUID(guid), SFX(sfx) {}
};
}}}}
