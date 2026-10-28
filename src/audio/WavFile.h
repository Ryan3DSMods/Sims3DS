/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims audio — RIFF/WAVE PCM reading (the game's .wav stings/UI sounds, Music/Modes/Load/loadloop.wav)
// and writing (the host backend's capture, the decode tool). Replaces XNA SoundEffect.FromStream for WAV data.
#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include "audio/SoundBuffer.h"

namespace simaudio {

struct WavInfo {
    int Channels = 0, Rate = 0, Bits = 0, Format = 0;
    size_t DataOffset = 0, DataBytes = 0; // relative to the start of the RIFF data given to ParseWavHeader
};

/// Parses a RIFF/WAVE header (PCM 8/16 bit). False if the data is not a WAV we can play.
bool ParseWavHeader(const uint8_t* data, size_t size, WavInfo& info);

/// Decodes a whole in-memory WAV (8-bit is widened to 16-bit). Null on failure.
std::shared_ptr<SoundBuffer> DecodeWav(const uint8_t* data, size_t size);

/// Streams a PCM WAV from a file region (a loose file, or an entry inside a FAR archive: offset/length).
class WavStream : public AudioStream {
public:
    /// length 0 = to the end of the file.
    static std::unique_ptr<WavStream> Open(const std::string& path, long offset = 0, long length = 0);
    ~WavStream();
    int Channels() const override { return _Info.Channels; }
    int Rate() const override { return _Info.Rate; }
    int Read(int16_t* out, int maxFrames) override;
    bool Rewind() override;
    double DurationSeconds() const override;
private:
    WavStream() {}
    FILE* _F = nullptr;
    long _Base = 0;      // file offset of the RIFF header
    WavInfo _Info;
    size_t _Pos = 0;     // bytes of sample data consumed
    std::vector<uint8_t> _Tmp;
};

/// Writes s16 interleaved PCM as a WAV file. Incremental: Begin, Append..., End (patches the sizes).
class WavWriter {
public:
    bool Begin(const std::string& path, int channels, int rate);
    void Append(const int16_t* samples, size_t frames);
    void End();
    ~WavWriter() { End(); }
    size_t FramesWritten() const { return _Frames; }
private:
    FILE* _F = nullptr;
    int _Channels = 2, _Rate = 32000;
    size_t _Frames = 0;
};

bool WriteWavFile(const std::string& path, const SoundBuffer& buf);

}
