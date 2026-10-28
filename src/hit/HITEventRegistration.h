/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims SimsVille/HIT/HITEventRegistration.cs (namespace TSO.HIT.model -> FSO::HIT)
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include "files/HIT/HITConstants.h"
#include "hit/HITResourceGroup.h"

namespace FSO { namespace HIT {

class HITEventRegistration
{
public:
    std::string Name;
    FSO::Files::HIT::HITEvents EventType = FSO::Files::HIT::HITEvents::kSoundobPlay;
    uint32_t TrackID = 0;
    std::shared_ptr<HITResourceGroup> ResGroup; //used to access this event's hit code
    uint32_t Extra = 0; // TS1(sims): 4th EventMapping column (kSequenceTrackHitList: the hitlist)
};

}} // namespace FSO::HIT
