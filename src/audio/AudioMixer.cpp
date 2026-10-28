/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims audio — the software mixer.
#include "audio/AudioMixer.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace simaudio {

static const int kStreamWindow = 2048; // frames decoded per stream refill
static const int kRampFrames = 64;     // volume ramp length (~2 ms) for starts of changes / stops

struct AudioMixer::Voice {
    bool Active = false;
    bool Paused = false;
    bool Loop = false;
    bool Stopping = false;  // fading out, then freed
    uint16_t Gen = 0;
    int Group = 0;
    float Volume = 1.0f, Pan = 0.0f;
    // source
    SoundBufferPtr Buf;
    std::unique_ptr<AudioStream> Stream;
    std::vector<int16_t> Window;  // stream: decoded frames [0, WinFrames)
    int WinFrames = 0;
    bool StreamEnded = false;
    const int16_t* Data = nullptr;
    size_t Frames = 0;
    int Channels = 1, SrcRate = 22050;
    uint64_t Pos = 0;   // 32.32 fixed point, in source frames (relative to the window for streams)
    uint64_t Step = 0;  // 32.32 source frames per output frame (no long-term drift)
    uint64_t StreamBase = 0; // stream: source frames dropped from the window so far
    // gains (Q14), current and target
    int32_t GL = 0, GR = 0, TL = 0, TR = 0;
    bool Fresh = true;

    void Clear() {
        Active = Paused = Loop = Stopping = false;
        Buf.reset(); Stream.reset();
        Window.clear(); Window.shrink_to_fit();
        WinFrames = 0; StreamEnded = false; Data = nullptr; Frames = 0; Pos = 0; StreamBase = 0; Fresh = true;
    }
};

AudioMixer::AudioMixer(int outputRate, int maxVoices) : _Rate(outputRate) {
    for (int i = 0; i < maxVoices; i++) _Voices.emplace_back(new Voice());
}

AudioMixer::~AudioMixer() {}

AudioMixer::Voice* AudioMixer::Find(int handle) const {
    if (handle <= 0) return nullptr;
    int idx = handle & 0xFF;
    uint16_t gen = (uint16_t)((handle >> 8) & 0xFFFF);
    if (idx >= (int)_Voices.size()) return nullptr;
    Voice* v = _Voices[idx].get();
    return (v->Active && v->Gen == gen) ? v : nullptr;
}

void AudioMixer::TargetGains(Voice* v) const {
    float g = v->Volume * _Groups[v->Group] * _Master;
    if (g < 0) g = 0;
    if (g > 4) g = 4;
    float pan = std::max(-1.0f, std::min(1.0f, v->Pan));
    // balance law: centre = unity on both sides, a hard pan silences the far side
    float l = pan > 0 ? 1.0f - pan : 1.0f;
    float r = pan < 0 ? 1.0f + pan : 1.0f;
    v->TL = (int32_t)(g * l * 16384.0f);
    v->TR = (int32_t)(g * r * 16384.0f);
    if (v->Stopping) v->TL = v->TR = 0;
}

int AudioMixer::Start(Voice* v, int group, float volume, float pan, bool loop) {
    // caller holds the lock and has filled the source fields
    int idx = -1;
    for (size_t i = 0; i < _Voices.size(); i++) if (_Voices[i].get() == v) idx = (int)i;
    v->Active = true;
    v->Paused = false;
    v->Stopping = false;
    v->Loop = loop;
    v->Group = (group >= 0 && group < GroupCount) ? group : GroupSFX;
    v->Volume = volume;
    v->Pan = pan;
    v->Pos = 0;
    v->StreamBase = 0;
    v->Step = ((uint64_t)v->SrcRate << 32) / (uint64_t)_Rate;
    v->Fresh = true;
    TargetGains(v);
    v->GL = v->TL; v->GR = v->TR; // start at the target: keeps the attack of short sounds
    if (++_NextGen == 0) _NextGen = 1;
    v->Gen = _NextGen;
    return (int)(((uint32_t)v->Gen << 8) | (uint32_t)idx);
}

int AudioMixer::Play(SoundBufferPtr buf, float volume, float pan, bool loop, int group, float pitch) {
    if (!buf || buf->Frames() == 0) return 0;
    Guard g(*this);
    for (auto& vp : _Voices) {
        Voice* v = vp.get();
        if (v->Active) continue;
        v->Clear();
        v->Buf = buf;
        v->Data = buf->Samples.data();
        v->Frames = buf->Frames();
        v->Channels = buf->Channels;
        v->SrcRate = (int)(buf->Rate * (pitch > 0 ? pitch : 1.0f));
        return Start(v, group, volume, pan, loop);
    }
    return 0;
}

int AudioMixer::PlayStream(std::unique_ptr<AudioStream> stream, float volume, float pan, bool loop, int group) {
    if (!stream) return 0;
    Guard g(*this);
    for (auto& vp : _Voices) {
        Voice* v = vp.get();
        if (v->Active) continue;
        v->Clear();
        v->Channels = stream->Channels();
        v->SrcRate = stream->Rate();
        v->Stream = std::move(stream);
        v->Window.assign((size_t)(kStreamWindow + 8) * v->Channels, 0);
        v->WinFrames = 0;
        int h = Start(v, group, volume, pan, loop);
        RefillStream(v);
        return h;
    }
    return 0;
}

bool AudioMixer::RefillStream(Voice* v) {
    // keeps the frames the interpolator still needs (idx-1 .. end) and decodes the next chunk after them
    int keep = 0;
    int ch = v->Channels;
    if (v->WinFrames > 0) {
        size_t idx = (size_t)(v->Pos >> 32);
        if (idx >= (size_t)v->WinFrames) idx = (size_t)v->WinFrames - 1;
        size_t from = idx > 0 ? idx - 1 : 0;
        keep = v->WinFrames - (int)from;
        if (keep > 0) std::memmove(v->Window.data(), v->Window.data() + from * ch, (size_t)keep * ch * sizeof(int16_t));
        v->Pos -= (uint64_t)from << 32;
        v->StreamBase += from;
    }
    int got = v->Stream->Read(v->Window.data() + (size_t)keep * ch, kStreamWindow);
    if (got <= 0 && v->Loop) {
        if (v->Stream->Rewind()) got = v->Stream->Read(v->Window.data() + (size_t)keep * ch, kStreamWindow);
    }
    if (got <= 0) { v->StreamEnded = true; v->WinFrames = keep; v->Frames = (size_t)keep; return false; }
    v->WinFrames = keep + got;
    v->Data = v->Window.data();
    v->Frames = (size_t)v->WinFrames;
    return true;
}

void AudioMixer::Stop(int voice) {
    Guard g(*this);
    Voice* v = Find(voice);
    if (v == nullptr) return;
    if (v->Paused) { v->Clear(); return; }
    v->Stopping = true;
    TargetGains(v);
}

void AudioMixer::StopGroup(int group) {
    Guard g(*this);
    for (auto& vp : _Voices) if (vp->Active && vp->Group == group) { vp->Stopping = true; TargetGains(vp.get()); }
}

void AudioMixer::StopAll() {
    Guard g(*this);
    for (auto& vp : _Voices) if (vp->Active) vp->Clear();
}

bool AudioMixer::IsPlaying(int voice) const {
    Guard g(*this);
    Voice* v = Find(voice);
    return v != nullptr && !v->Stopping;
}

void AudioMixer::SetVolumePan(int voice, float volume, float pan) {
    Guard g(*this);
    Voice* v = Find(voice);
    if (v == nullptr) return;
    v->Volume = volume;
    v->Pan = pan;
    TargetGains(v);
}

void AudioMixer::SetPaused(int voice, bool paused) {
    Guard g(*this);
    Voice* v = Find(voice);
    if (v != nullptr) v->Paused = paused;
}

void AudioMixer::SetLooping(int voice, bool loop) {
    Guard g(*this);
    Voice* v = Find(voice);
    if (v != nullptr) v->Loop = loop;
}

double AudioMixer::Position(int voice) const {
    Guard g(*this);
    Voice* v = Find(voice);
    if (v == nullptr) return -1;
    return (double)(v->StreamBase + (v->Pos >> 32)) / (v->SrcRate > 0 ? v->SrcRate : 1);
}

void AudioMixer::SetGroupVolume(int group, float vol) {
    Guard g(*this);
    if (group < 0 || group >= GroupCount) return;
    _Groups[group] = vol;
    for (auto& vp : _Voices) if (vp->Active) TargetGains(vp.get());
}

float AudioMixer::GroupVolume(int group) const {
    return (group >= 0 && group < GroupCount) ? _Groups[group] : 0.0f;
}

void AudioMixer::SetMasterVolume(float vol) {
    Guard g(*this);
    _Master = vol;
    for (auto& vp : _Voices) if (vp->Active) TargetGains(vp.get());
}

int AudioMixer::ActiveVoices() const {
    Guard g(*this);
    int n = 0;
    for (auto& vp : _Voices) if (vp->Active) n++;
    return n;
}

// 4-point Catmull-Rom interpolation, t in Q15. Cheap on the ARM11 (SMULL) and much cleaner than linear for the
// 22 kHz -> 32.7 kHz effects and the 44.1 kHz -> 32.7 kHz music.
static inline int32_t Cubic(int32_t p0, int32_t p1, int32_t p2, int32_t p3, int32_t t) {
    int64_t a = 3 * (int64_t)(p1 - p2) + p3 - p0;
    int64_t b = 2 * (int64_t)p0 - 5 * (int64_t)p1 + 4 * (int64_t)p2 - p3;
    int64_t c = (int64_t)p2 - p0;
    int64_t y = (t * a) >> 15;
    y = (t * (b + y)) >> 15;
    y = (t * (c + y)) >> 16;
    return p1 + (int32_t)y;
}

void AudioMixer::MixVoice(Voice* v, int frames) {
    int32_t* acc = _Acc.data();
    const int ch = v->Channels;
    int rampLeft = (v->GL != v->TL || v->GR != v->TR) ? kRampFrames : 0;
    int32_t dL = rampLeft ? (v->TL - v->GL) / kRampFrames : 0;
    int32_t dR = rampLeft ? (v->TR - v->GR) / kRampFrames : 0;
    const bool wrap = v->Loop && !v->Stream;

    for (int i = 0; i < frames; i++) {
        size_t idx = (size_t)(v->Pos >> 32);
        // source exhausted / stream window used up?
        if (v->Stream) {
            if (!v->StreamEnded && idx + 2 >= (size_t)v->WinFrames) {
                RefillStream(v);
                idx = (size_t)(v->Pos >> 32);
            }
            if (v->StreamEnded && idx >= (size_t)v->WinFrames) { v->Clear(); return; }
        } else if (idx >= v->Frames) {
            if (v->Loop) { v->Pos -= (uint64_t)v->Frames << 32; idx = (size_t)(v->Pos >> 32); }
            else { v->Clear(); return; }
        }
        const int16_t* d = v->Data;
        const size_t n = v->Frames;
        // neighbours: wrap for looped sounds, hold the edge sample otherwise
        size_t i0 = idx > 0 ? idx - 1 : (wrap ? n - 1 : 0);
        size_t i2 = idx + 1 < n ? idx + 1 : (wrap ? (idx + 1) % n : n - 1);
        size_t i3 = idx + 2 < n ? idx + 2 : (wrap ? (idx + 2) % n : n - 1);
        int32_t t = (int32_t)((v->Pos >> 17) & 0x7FFF);
        int32_t l, r;
        if (ch == 1) {
            l = r = Cubic(d[i0], d[idx], d[i2], d[i3], t);
        } else {
            l = Cubic(d[i0 * 2], d[idx * 2], d[i2 * 2], d[i3 * 2], t);
            r = Cubic(d[i0 * 2 + 1], d[idx * 2 + 1], d[i2 * 2 + 1], d[i3 * 2 + 1], t);
        }
        if (rampLeft > 0) {
            v->GL += dL; v->GR += dR;
            if (--rampLeft == 0) { v->GL = v->TL; v->GR = v->TR; }
        }
        acc[i * 2] += (l * v->GL) >> 14;
        acc[i * 2 + 1] += (r * v->GR) >> 14;
        v->Pos += v->Step;
    }
    if (v->Stopping && rampLeft == 0 && v->GL == 0 && v->GR == 0) v->Clear();
}

void AudioMixer::Mix(int16_t* out, int frames) {
    Guard g(*this);
    if ((int)_Acc.size() < frames * 2) _Acc.resize((size_t)frames * 2);
    std::fill(_Acc.begin(), _Acc.begin() + (size_t)frames * 2, 0);
    for (auto& vp : _Voices) {
        Voice* v = vp.get();
        if (!v->Active || v->Paused) continue;
        MixVoice(v, frames);
    }
    int peak = 0;
    for (int i = 0; i < frames * 2; i++) {
        int32_t s = _Acc[i];
        // soft knee above ~-1.4 dBFS so stacked voices saturate gently instead of hard clipping
        if (s > 28000) s = 28000 + ((s - 28000) >> 2);
        else if (s < -28000) s = -28000 + ((s + 28000) >> 2);
        if (s > 32767) s = 32767;
        if (s < -32768) s = -32768;
        out[i] = (int16_t)s;
        int a = s < 0 ? -s : s;
        if (a > peak) peak = a;
    }
    _LastPeak = peak;
    _FramesMixed += (uint64_t)frames;
}

}
