/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/HIT/Track.cs
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "files/HIT/HITConstants.h"

namespace FSO { namespace Files { namespace HIT {

/// <summary>
/// TRK is a CSV format that defines a HIT track.
/// </summary>
class Track
{
private:
    bool TWODKT = false; //Optional encoding as Pascal string, typical Maxis...
    uint32_t ParseHexString(std::string input);
public:
    std::string MagicNumber;
    uint32_t Version = 0;
    std::string TrackName;
    uint32_t SoundID = 0;
    uint32_t TrackID = 0;
    HITArgs ArgType = HITArgs::kArgsNormal;
    HITControlGroups ControlGroup = (HITControlGroups)0;
    HITDuckingPriorities DuckingPriority = HITDuckingPriorities::duckpri_always;
    uint32_t Looped = 0;
    uint32_t Volume = 0;
    bool LoopDefined = false;

    uint32_t SubroutineID = 0;
    uint32_t HitlistID = 0;

    Track() {}
    /// <summary>
    /// Creates a new track.
    /// </summary>
    /// <param name="Filedata">The data to create the track from.</param>
    explicit Track(const std::vector<uint8_t>& Filedata);
};

}}} // namespace FSO::Files::HIT
