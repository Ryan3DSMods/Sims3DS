/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims audio — the software mixer: N voices of in-memory sounds or streams, cubic (Catmull-Rom) resampling
// to one output rate (32728 Hz = the 3DS DSP rate; any rate on the host), per-voice volume/pan with click-free
// ramps, four volume groups (HITVolumeGroup: FX, MUSIC, VOX, AMBIENCE), stereo s16 output.
//
// Threading: the game thread starts/stops/updates voices; Mix() runs where the platform wants it (the 3DS backend's
// audio thread, or inline on the host). Every public method takes the lock the backend installs with SetLock()
// (no lock by default = single threaded).
#pragma once
#include <cstdint>
#include <memory>
#include <vector>
#include "audio/SoundBuffer.h"

namespace simaudio {

class AudioMixer {
public:
    enum Group { GroupSFX = 0, GroupMusic = 1, GroupVox = 2, GroupAmbience = 3, GroupCount = 4 };

    explicit AudioMixer(int outputRate = 32728, int maxVoices = 32);
    ~AudioMixer();

    int OutputRate() const { return _Rate; }
    int MaxVoices() const { return (int)_Voices.size(); }

    /// Plays a decoded sound. Returns a voice handle (> 0), or 0 when no voice is free / buf is empty.
    int Play(SoundBufferPtr buf, float volume, float pan, bool loop, int group, float pitch = 1.0f);
    /// Plays a stream (decoded in chunks while mixing). Takes ownership.
    int PlayStream(std::unique_ptr<AudioStream> stream, float volume, float pan, bool loop, int group);

    void Stop(int voice);            // fades out over ~2 ms (no click) and frees the voice
    void StopGroup(int group);
    void StopAll();
    bool IsPlaying(int voice) const; // true until the sound ends or is stopped (paused voices count as playing)
    void SetVolumePan(int voice, float volume, float pan);
    void SetPaused(int voice, bool paused);
    void SetLooping(int voice, bool loop);
    /// Seconds played so far (source time), -1 if the handle is dead.
    double Position(int voice) const;

    void SetGroupVolume(int group, float v);
    float GroupVolume(int group) const;
    void SetMasterVolume(float v);
    float MasterVolume() const { return _Master; }

    /// Mixes `frames` stereo frames into out (interleaved L,R s16).
    void Mix(int16_t* out, int frames);

    int ActiveVoices() const;
    uint64_t FramesMixed() const { return _FramesMixed; }
    /// Peak |sample| of the last Mix call (0..32767), for meters/tests.
    int LastPeak() const { return _LastPeak; }

    typedef void (*LockFn)(void* ctx);
    void SetLock(LockFn lock, LockFn unlock, void* ctx) { _Lock = lock; _Unlock = unlock; _LockCtx = ctx; }

    struct Guard {
        const AudioMixer& M;
        explicit Guard(const AudioMixer& m) : M(m) { if (M._Lock) M._Lock(M._LockCtx); }
        ~Guard() { if (M._Unlock) M._Unlock(M._LockCtx); }
    };

private:
    struct Voice;
    std::vector<std::unique_ptr<Voice>> _Voices;
    std::vector<int32_t> _Acc;
    int _Rate;
    float _Groups[GroupCount] = { 1, 1, 1, 1 };
    float _Master = 1.0f;
    uint64_t _FramesMixed = 0;
    int _LastPeak = 0;
    uint16_t _NextGen = 1;
    LockFn _Lock = nullptr, _Unlock = nullptr;
    void* _LockCtx = nullptr;

    Voice* Find(int handle) const;
    int Start(Voice* v, int group, float volume, float pan, bool loop);
    void TargetGains(Voice* v) const;
    void MixVoice(Voice* v, int frames);
    bool RefillStream(Voice* v);
};

}
