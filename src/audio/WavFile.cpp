/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims audio — RIFF/WAVE reading and writing.
#include "audio/WavFile.h"
#include <cstring>

namespace simaudio {

static uint32_t RdU32(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint16_t RdU16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

bool ParseWavHeader(const uint8_t* data, size_t size, WavInfo& info) {
    if (size < 12 || std::memcmp(data, "RIFF", 4) != 0 || std::memcmp(data + 8, "WAVE", 4) != 0) return false;
    size_t p = 12;
    bool haveFmt = false;
    while (p + 8 <= size) {
        uint32_t len = RdU32(data + p + 4);
        if (std::memcmp(data + p, "fmt ", 4) == 0 && p + 8 + 16 <= size) {
            info.Format = RdU16(data + p + 8);
            info.Channels = RdU16(data + p + 10);
            info.Rate = (int)RdU32(data + p + 12);
            info.Bits = RdU16(data + p + 22);
            haveFmt = true;
        } else if (std::memcmp(data + p, "data", 4) == 0) {
            info.DataOffset = p + 8;
            info.DataBytes = len;
            // some Maxis WAVs (and XA-converted ones) declare more data than present; the caller clamps
            return haveFmt && info.Format == 1 && (info.Bits == 16 || info.Bits == 8) && info.Channels >= 1 && info.Channels <= 2 && info.Rate > 0;
        }
        p += 8 + len + (len & 1);
    }
    return false;
}

std::shared_ptr<SoundBuffer> DecodeWav(const uint8_t* data, size_t size) {
    WavInfo info;
    if (!ParseWavHeader(data, size, info)) return nullptr;
    size_t avail = size > info.DataOffset ? size - info.DataOffset : 0;
    size_t bytes = info.DataBytes < avail ? info.DataBytes : avail;
    auto buf = std::make_shared<SoundBuffer>();
    buf->Channels = info.Channels;
    buf->Rate = info.Rate;
    const uint8_t* s = data + info.DataOffset;
    if (info.Bits == 16) {
        size_t n = bytes / 2;
        n -= n % (size_t)info.Channels;
        buf->Samples.resize(n);
        for (size_t i = 0; i < n; i++) buf->Samples[i] = (int16_t)RdU16(s + i * 2);
    } else {
        size_t n = bytes - bytes % (size_t)info.Channels;
        buf->Samples.resize(n);
        for (size_t i = 0; i < n; i++) buf->Samples[i] = (int16_t)(((int)s[i] - 128) << 8);
    }
    return buf;
}

std::unique_ptr<WavStream> WavStream::Open(const std::string& path, long offset, long length) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return nullptr;
    uint8_t hdr[512];
    std::fseek(f, offset, SEEK_SET);
    size_t got = std::fread(hdr, 1, sizeof hdr, f);
    std::unique_ptr<WavStream> s(new WavStream());
    if (!ParseWavHeader(hdr, got, s->_Info)) { std::fclose(f); return nullptr; }
    if (length > 0 && s->_Info.DataOffset + s->_Info.DataBytes > (size_t)length)
        s->_Info.DataBytes = (size_t)length > s->_Info.DataOffset ? (size_t)length - s->_Info.DataOffset : 0;
    if (length <= 0) {
        std::fseek(f, 0, SEEK_END);
        long end = std::ftell(f);
        size_t avail = (size_t)(end - offset) > s->_Info.DataOffset ? (size_t)(end - offset) - s->_Info.DataOffset : 0;
        if (s->_Info.DataBytes > avail) s->_Info.DataBytes = avail;
    }
    s->_F = f;
    s->_Base = offset;
    s->Rewind();
    return s;
}

WavStream::~WavStream() { if (_F) std::fclose(_F); }

bool WavStream::Rewind() {
    _Pos = 0;
    return _F && std::fseek(_F, _Base + (long)_Info.DataOffset, SEEK_SET) == 0;
}

double WavStream::DurationSeconds() const {
    int fb = _Info.Channels * (_Info.Bits / 8);
    return (fb > 0 && _Info.Rate > 0) ? (double)(_Info.DataBytes / (size_t)fb) / _Info.Rate : -1.0;
}

int WavStream::Read(int16_t* out, int maxFrames) {
    if (!_F || maxFrames <= 0) return 0;
    int bps = _Info.Bits / 8;
    size_t frameBytes = (size_t)_Info.Channels * (size_t)bps;
    size_t left = _Info.DataBytes - _Pos;
    size_t want = (size_t)maxFrames * frameBytes;
    if (want > left) want = left - left % frameBytes;
    if (want == 0) return 0;
    _Tmp.resize(want);
    size_t got = std::fread(_Tmp.data(), 1, want, _F);
    got -= got % frameBytes;
    _Pos += got;
    size_t n = got / (size_t)bps;
    if (bps == 2) for (size_t i = 0; i < n; i++) out[i] = (int16_t)RdU16(&_Tmp[i * 2]);
    else for (size_t i = 0; i < n; i++) out[i] = (int16_t)(((int)_Tmp[i] - 128) << 8);
    return (int)(got / frameBytes);
}

static void WrU32(FILE* f, uint32_t v) { uint8_t b[4] = { (uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24) }; std::fwrite(b, 1, 4, f); }
static void WrU16(FILE* f, uint16_t v) { uint8_t b[2] = { (uint8_t)v, (uint8_t)(v >> 8) }; std::fwrite(b, 1, 2, f); }

bool WavWriter::Begin(const std::string& path, int channels, int rate) {
    End();
    _F = std::fopen(path.c_str(), "wb");
    if (!_F) return false;
    _Channels = channels; _Rate = rate; _Frames = 0;
    std::fwrite("RIFF", 1, 4, _F); WrU32(_F, 36);
    std::fwrite("WAVEfmt ", 1, 8, _F); WrU32(_F, 16);
    WrU16(_F, 1); WrU16(_F, (uint16_t)channels); WrU32(_F, (uint32_t)rate);
    WrU32(_F, (uint32_t)(rate * channels * 2)); WrU16(_F, (uint16_t)(channels * 2)); WrU16(_F, 16);
    std::fwrite("data", 1, 4, _F); WrU32(_F, 0);
    return true;
}

void WavWriter::Append(const int16_t* samples, size_t frames) {
    if (!_F) return;
    size_t n = frames * (size_t)_Channels;
    for (size_t i = 0; i < n; i++) WrU16(_F, (uint16_t)samples[i]); // explicit little endian
    _Frames += frames;
}

void WavWriter::End() {
    if (!_F) return;
    uint32_t data = (uint32_t)(_Frames * (size_t)_Channels * 2);
    std::fseek(_F, 4, SEEK_SET); WrU32(_F, 36 + data);
    std::fseek(_F, 40, SEEK_SET); WrU32(_F, data);
    std::fclose(_F);
    _F = nullptr;
}

bool WriteWavFile(const std::string& path, const SoundBuffer& buf) {
    WavWriter w;
    if (!w.Begin(path, buf.Channels, buf.Rate)) return false;
    w.Append(buf.Samples.data(), buf.Frames());
    w.End();
    return true;
}

}
