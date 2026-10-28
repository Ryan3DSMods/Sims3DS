/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims audio — streaming MP3 decoder for the game's music (Music/Modes/*, Music/Stations/*: MPEG-1 layer III,
// 128 kbps, 44.1 kHz stereo). Decodes one frame (1152 samples) at a time from a 16 KB file window, so a song never
// sits in RAM (the 3DS has none to spare: ~4 MB per minute of decoded stereo PCM). Uses minimp3 (CC0, vendored in
// src/third_party/minimp3).
#pragma once
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>
#include "audio/SoundBuffer.h"

namespace simaudio {

class Mp3Stream : public AudioStream {
public:
    static std::unique_ptr<Mp3Stream> Open(const std::string& path);
    ~Mp3Stream();
    int Channels() const override { return _Channels; }
    int Rate() const override { return _Rate; }
    int Read(int16_t* out, int maxFrames) override;
    bool Rewind() override;
    double DurationSeconds() const override { return _Duration; }

    /// Decodes a whole file into memory (host tools / tests only).
    static std::shared_ptr<SoundBuffer> DecodeAll(const std::string& path);

private:
    Mp3Stream();
    bool DecodeNext();  // decodes the next frame into _Pcm; false at the end
    bool Fill();        // tops up the file window
    struct Impl;
    std::unique_ptr<Impl> _I;
    FILE* _F = nullptr;
    int _Channels = 2, _Rate = 44100;
    long _FileSize = 0;
    double _Duration = -1;
    std::vector<uint8_t> _Buf;   // file window
    size_t _BufPos = 0, _BufLen = 0;
    bool _Eof = false;
    std::vector<int16_t> _Pcm;   // the current decoded frame
    int _PcmFrames = 0, _PcmPos = 0;
};

}
