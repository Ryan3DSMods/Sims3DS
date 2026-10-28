/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims audio — streaming MP3 (minimp3).
#define MINIMP3_IMPLEMENTATION
#define MINIMP3_ONLY_MP3
#include "third_party/minimp3/minimp3.h"
#include "audio/Mp3Stream.h"
#include <cstring>

namespace simaudio {

static const size_t kWindow = 16 * 1024;

struct Mp3Stream::Impl {
    mp3dec_t Dec;
};

Mp3Stream::Mp3Stream() : _I(new Impl()) {}

Mp3Stream::~Mp3Stream() { if (_F) std::fclose(_F); }

std::unique_ptr<Mp3Stream> Mp3Stream::Open(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return nullptr;
    std::unique_ptr<Mp3Stream> s(new Mp3Stream());
    s->_F = f;
    std::fseek(f, 0, SEEK_END);
    s->_FileSize = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    s->_Buf.resize(kWindow);
    s->_Pcm.resize(MINIMP3_MAX_SAMPLES_PER_FRAME);
    if (!s->Rewind()) return nullptr;
    // the first frame gives the format; it stays in _Pcm for the first Read
    if (!s->DecodeNext()) return nullptr;
    return s;
}

bool Mp3Stream::Rewind() {
    if (!_F) return false;
    std::fseek(_F, 0, SEEK_SET);
    _BufPos = _BufLen = 0;
    _Eof = false;
    _PcmFrames = _PcmPos = 0;
    mp3dec_init(&_I->Dec);
    return true;
}

bool Mp3Stream::Fill() {
    if (_BufPos > 0) {
        std::memmove(_Buf.data(), _Buf.data() + _BufPos, _BufLen - _BufPos);
        _BufLen -= _BufPos;
        _BufPos = 0;
    }
    if (!_Eof && _BufLen < _Buf.size()) {
        size_t got = std::fread(_Buf.data() + _BufLen, 1, _Buf.size() - _BufLen, _F);
        if (got == 0) _Eof = true;
        _BufLen += got;
    }
    return _BufLen > 0;
}

bool Mp3Stream::DecodeNext() {
    for (int guard = 0; guard < 4096; guard++) {
        if (_BufLen - _BufPos < 2048 && !_Eof) Fill();
        if (_BufPos >= _BufLen) return false;
        mp3dec_frame_info_t info;
        int samples = mp3dec_decode_frame(&_I->Dec, _Buf.data() + _BufPos, (int)(_BufLen - _BufPos), _Pcm.data(), &info);
        if (info.frame_bytes == 0) {
            // need more data (or garbage at the end)
            if (_Eof) return false;
            if (_BufPos == 0 && _BufLen == _Buf.size()) { _BufPos = _BufLen; continue; } // no sync in a full window
            Fill();
            continue;
        }
        _BufPos += (size_t)info.frame_bytes;
        if (samples > 0) {
            if (_Duration < 0 && info.bitrate_kbps > 0) _Duration = (double)_FileSize * 8.0 / (info.bitrate_kbps * 1000.0);
            _Channels = info.channels;
            _Rate = info.hz;
            _PcmFrames = samples;
            _PcmPos = 0;
            return true;
        }
        // frame_bytes > 0 && samples == 0: skipped ID3 / junk
    }
    return false;
}

int Mp3Stream::Read(int16_t* out, int maxFrames) {
    int done = 0;
    while (done < maxFrames) {
        if (_PcmPos >= _PcmFrames && !DecodeNext()) break;
        int n = _PcmFrames - _PcmPos;
        if (n > maxFrames - done) n = maxFrames - done;
        std::memcpy(out + (size_t)done * _Channels, _Pcm.data() + (size_t)_PcmPos * _Channels, (size_t)n * _Channels * sizeof(int16_t));
        _PcmPos += n;
        done += n;
    }
    return done;
}

std::shared_ptr<SoundBuffer> Mp3Stream::DecodeAll(const std::string& path) {
    auto s = Open(path);
    if (!s) return nullptr;
    auto buf = std::make_shared<SoundBuffer>();
    buf->Channels = s->Channels();
    buf->Rate = s->Rate();
    std::vector<int16_t> tmp(4096 * 2);
    for (;;) {
        int n = s->Read(tmp.data(), 4096);
        if (n <= 0) break;
        buf->Samples.insert(buf->Samples.end(), tmp.begin(), tmp.begin() + (size_t)n * buf->Channels);
    }
    return buf;
}

}
