/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims audio — where the mixer's output goes. Platform backends: src/platform/3ds/AudioNdsp (libctru NDSP,
// double-buffered wave buffers fed by an audio thread) and WavCaptureBackend (host: writes what would be heard to
// a WAV file, driven by the simulated frame time, so a scripted session can be listened to and analysed).
#pragma once
#include <string>
#include "audio/AudioMixer.h"
#include "audio/WavFile.h"

namespace simaudio {

class AudioBackend {
public:
    virtual ~AudioBackend() {}
    virtual bool Start(AudioMixer& mixer) = 0;
    virtual void Stop() = 0;
    /// Called once per frame by backends that are driven from the main loop (dtMs of simulated time).
    virtual void Pump(double dtMs) { (void)dtMs; }
    /// Suspend output (HOME menu / sleep) without losing the mixer state.
    virtual void SetSuspended(bool suspended) { (void)suspended; }
    virtual const char* Name() const = 0;
    /// ADDED(sims): average delay (ms) between a mixer change and when it is heard (queued output). 0 = immediate.
    virtual double LatencyMs() const { return 0.0; }
};

/// Host: mixes exactly dtMs of audio per Pump into a WAV file (deterministic, faster or slower than real time).
class WavCaptureBackend : public AudioBackend {
public:
    explicit WavCaptureBackend(const std::string& path) : _Path(path) {}
    ~WavCaptureBackend() override { Stop(); }
    bool Start(AudioMixer& mixer) override;
    void Stop() override;
    void Pump(double dtMs) override;
    const char* Name() const override { return "wav-capture"; }
    double SecondsWritten() const;
    int Peak() const { return _Peak; }
private:
    std::string _Path;
    AudioMixer* _Mixer = nullptr;
    WavWriter _W;
    double _Frac = 0;
    int _Peak = 0;
    std::vector<int16_t> _Buf;
};

}
