/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/HIT/HITConstants.cs
#pragma once
#include <cstdint>
namespace FSO { namespace Files { namespace HIT {

enum class HITArgs : uint32_t
{
    kArgsNormal = 0,
    kArgsVolPan = 1,
    kArgsIdVolPan = 2,
    kArgsXYZ = 3
};

enum class HITControlGroups : uint32_t
{
    kGroupSFX = 1,
    kGroupMusic = 2,
    kGroupVox = 3
};

enum class HITDuckingPriorities : uint32_t
{
    duckpri_unknown1 = 32,
    duckpri_unknown2 = 5000,
    duckpri_always = 0x0,
    duckpri_low = 0x1,
    duckpri_normal = 0x14,
    duckpri_high = 0x1e,
    duckpri_higher = 0x28,
    duckpri_evenhigher = 0x32,
    duckpri_never = 0x64
};

enum class HITEvents : uint32_t
{
    kSoundobPlay = 1,
    kSoundobStop = 2,
    kSoundobKill = 3,
    kSoundobUpdate = 4,
    kSoundobSetVolume = 5,
    kSoundobSetPitch = 6,
    kSoundobSetPan = 7,
    kSoundobSetPosition = 8,
    kSoundobSetFxType = 9,
    kSoundobSetFxLevel = 10,
    kSoundobPause = 11,
    kSoundobUnpause = 12,
    kSoundobLoad = 13,
    kSoundobUnload = 14,
    kSoundobCache = 15,
    kSoundobUncache = 16,
    kSoundobCancelNote = 19,
    kKillAll = 20,
    kPause = 21,
    kUnpause = 22,
    kKillInstance = 23,
    kTurnOnTV = 30,
    kTurnOffTV = 31,
    kUpdateSourceVolPan = 32,
    kSetMusicMode = 36,
    kPlayPiano = 43,
    debugeventson = 44,
    debugeventsoff = 45,
    debugsampleson = 46,
    debugsamplesoff = 47,
    debugtrackson = 48,
    debugtracksoff = 49,
    // TS1(sims): event types only the TS1 expansion packs use (SimsGlobals.txt / *_userdata.txt)
    kSetAmbienceMode = 52,
    kSequenceTrackHitList = 53
};

enum class HITPerson : uint32_t
{
    Instance = 0x0,
    Gender = 0x1
};

}}} // namespace FSO::Files::HIT
