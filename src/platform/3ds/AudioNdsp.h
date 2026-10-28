/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims — 3DS audio backend: the software mixer (src/audio/AudioMixer) feeds one stereo NDSP channel through
// a ring of wave buffers (linear memory), refilled by a dedicated audio thread that wakes on the DSP callback.
// MP3 music is decoded on that thread in 1152-sample frames, so nothing large is ever resident.
//
// Needs the DSP firmware dump at sdmc:/3ds/dspfirm.cdc (made by DSP1 / Luma's "dump DSP firmware"); without it
// ndspInit fails and the game runs silently.
#pragma once
#include "audio/AudioBackend.h"

namespace simaudio {

class NdspBackend : public AudioBackend {
public:
    NdspBackend() {}
    ~NdspBackend() override { Stop(); }
    bool Start(AudioMixer& mixer) override;
    void Stop() override;
    void SetSuspended(bool suspended) override;
    const char* Name() const override { return "ndsp"; }
    /// ADDED(sims): a new voice goes into the next buffer freed (0..1 buffer away) behind the 3 still queued.
    double LatencyMs() const override { return (kBuffers - 0.5) * kFrames * 1000.0 / (_Rate > 0 ? _Rate : 32728); }

    static const int kBuffers = 4;       // 4 x 1024 frames = 125 ms of queued audio at 32728 Hz
    static const int kFrames = 1024;
    // last measured cost of mixing one buffer (ms), for the stats overlay
    static volatile float MixMs;
    static volatile int Underruns;
private:
    int _Rate = 32728; // ADDED(sims): the mixer's output rate (LatencyMs)
};

}
