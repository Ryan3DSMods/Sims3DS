/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims audio — a decoded sound held in memory (s16 interleaved PCM) and the streaming source interface.
// Platform independent; shared by the HIT engine (src/hit), the mixer and the platform backends.
#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace simaudio {

/// A decoded sound effect: 16-bit signed PCM, interleaved when stereo.
struct SoundBuffer {
    std::vector<int16_t> Samples;
    int Channels = 1;
    int Rate = 22050;
    size_t Frames() const { return Channels > 0 ? Samples.size() / (size_t)Channels : 0; }
    double Seconds() const { return Rate > 0 ? (double)Frames() / Rate : 0.0; }
    size_t Bytes() const { return Samples.size() * sizeof(int16_t) + sizeof(*this); }
};
using SoundBufferPtr = std::shared_ptr<const SoundBuffer>;

/// A sound decoded on the fly in small chunks (music MP3s, long WAVs): keeps RAM low on the 3DS.
/// Read/Rewind are called from the mixer (the audio thread on the 3DS) with the mixer lock held.
class AudioStream {
public:
    virtual ~AudioStream() {}
    virtual int Channels() const = 0;
    virtual int Rate() const = 0;
    /// Reads up to maxFrames interleaved frames into out; returns the frames read, 0 at the end of the stream.
    virtual int Read(int16_t* out, int maxFrames) = 0;
    /// Back to the start (for looping). False if not possible.
    virtual bool Rewind() = 0;
    /// Total length in seconds if known (else < 0).
    virtual double DurationSeconds() const { return -1.0; }
};

}
