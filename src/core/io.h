/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Portable byte-order-aware reader/writer for Sims file formats.
// Mirrors FSO.Files.Utils.IoBuffer / FSO.Files.Utils.IoWriter from the FreeSims
// reference, in C++17. The lower-case API (u8/u16/...) is the original minimal
// reader; the PascalCase API mirrors the C# IoBuffer/IoWriter method names so
// translated code lines up one-to-one.
#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <stdexcept>
#include <cstdio>
#include <cerrno>

namespace st {

// FSO.Files.Utils.ByteOrder (BIG_ENDIAN / LITTLE_ENDIAN are glibc macros, so
// the enumerators are spelled Big / Little here).
enum class Endian { Little, Big };

// System.IO.SeekOrigin
enum class SeekOrigin { Begin, Current, End };

class Reader {
public:
    Reader(const uint8_t* data, size_t size, Endian e = Endian::Little)
        : d_(data), n_(size), p_(0), e_(e), mark_(0) {}
    Reader(const std::vector<uint8_t>& v, Endian e = Endian::Little)
        : d_(v.data()), n_(v.size()), p_(0), e_(e), mark_(0) {}

    void endian(Endian e) { e_ = e; }
    Endian endian() const { return e_; }
    // C# IoBuffer.ByteOrder (property access spelled as methods)
    Endian ByteOrder() const { return e_; }
    void ByteOrder(Endian e) { e_ = e; }

    size_t pos() const { return p_; }
    size_t size() const { return n_; }
    bool has_more() const { return p_ < n_; }
    void seek(size_t off) { if (off > n_) throw std::runtime_error("seek OOB"); p_ = off; }
    void skip(size_t k) { seek(p_ + k); }

    uint8_t  u8()  { need(1); return d_[p_++]; }
    int8_t   s8()  { return (int8_t)u8(); }

    uint16_t u16() { need(2); uint16_t v = e_==Endian::Little
                         ? (uint16_t)(d_[p_] | (d_[p_+1]<<8))
                         : (uint16_t)(d_[p_+1] | (d_[p_]<<8)); p_+=2; return v; }
    int16_t  s16() { return (int16_t)u16(); }

    uint32_t u32() { need(4); uint32_t v; if (e_==Endian::Little)
                         v = (uint32_t)d_[p_] | ((uint32_t)d_[p_+1]<<8) | ((uint32_t)d_[p_+2]<<16) | ((uint32_t)d_[p_+3]<<24);
                     else
                         v = (uint32_t)d_[p_+3] | ((uint32_t)d_[p_+2]<<8) | ((uint32_t)d_[p_+1]<<16) | ((uint32_t)d_[p_]<<24);
                     p_+=4; return v; }
    int32_t  s32() { return (int32_t)u32(); }

    std::vector<uint8_t> bytes(size_t k) { need(k); std::vector<uint8_t> v(d_+p_, d_+p_+k); p_+=k; return v; }
    const uint8_t* raw(size_t k) { need(k); const uint8_t* r = d_+p_; p_+=k; return r; }

    // Fixed-length C string field; optionally trim trailing NULs.
    std::string cstr(size_t len, bool trim = true) {
        need(len);
        std::string s((const char*)(d_+p_), len);
        p_ += len;
        if (trim) { size_t z = s.find('\0'); if (z != std::string::npos) s.resize(z); }
        return s;
    }

    // ------------------------------------------------------------------
    // IoBuffer-compatible API (FSO.Files.Utils.IoBuffer)
    // ------------------------------------------------------------------

    /// More to read in this stream?  (C#: Stream.Position < Stream.Length - 1)
    bool HasMore() const { return p_ + 1 < n_; }

    /// Skips a number of bytes in the current stream, starting from the current position.
    void Skip(int64_t numBytes) { Seek(SeekOrigin::Current, numBytes); }

    /// Seeks in the current stream. Like a MemoryStream, seeking beyond the end is
    /// permitted (subsequent reads fail); seeking before the start throws.
    void Seek(SeekOrigin origin, int64_t offset) {
        int64_t base = 0;
        switch (origin) {
            case SeekOrigin::Begin:   base = 0; break;
            case SeekOrigin::Current: base = (int64_t)p_; break;
            case SeekOrigin::End:     base = (int64_t)n_; break;
        }
        int64_t np = base + offset;
        if (np < 0) throw std::runtime_error("seek before beginning");
        p_ = (size_t)np;
    }

    int64_t Position() const { return (int64_t)p_; }
    int64_t Length() const { return (int64_t)n_; }

    /// Reads a variable length unsigned integer from the current stream.
    uint32_t ReadVarLen() {
        uint32_t result = 0;
        int shift = 0;
        uint8_t read = 0x80;
        while ((read & 0x80) > 0) {
            read = ReadByte();
            result |= (uint32_t)((read & 0x7F) << shift);
            shift += 7;
        }
        return result;
    }

    uint16_t ReadUInt16() { return u16(); }
    int16_t  ReadInt16()  { return s16(); }
    int32_t  ReadInt32()  { return s32(); }
    uint32_t ReadUInt32() { return u32(); }

    /// Reads a number of ASCII characters from the current stream (INCLUDING any trailing 0s
    /// unless trimNull is set). Like BinaryReader.ReadBytes, a short read at EOF is permitted.
    std::string ReadCString(int num, bool trimNull = false) {
        auto b = ReadBytes((size_t)num);
        std::string result(b.begin(), b.end());
        if (trimNull) {
            size_t io = result.find('\0');
            if (io != std::string::npos) result = result.substr(0, io);
        }
        return result;
    }

    uint8_t ReadByte() { return u8(); }

    /// Reads a number of bytes from the current stream. Like BinaryReader.ReadBytes this
    /// returns fewer bytes (not an error) when the end of the stream is reached.
    std::vector<uint8_t> ReadBytes(size_t num) {
        size_t avail = (p_ < n_) ? (n_ - p_) : 0;
        if (num > avail) num = avail;
        std::vector<uint8_t> v(d_ + p_, d_ + p_ + num);
        p_ += num;
        return v;
    }

    /// Reads a pascal string from the current stream, which is prefixed by a 16bit short.
    std::string ReadLongPascalString() {
        int16_t length = ReadInt16();
        auto b = ReadBytes(length < 0 ? 0 : (size_t)length);
        return std::string(b.begin(), b.end());
    }

    /// Reads a C string from the current stream.
    std::string ReadNullTerminatedString() {
        std::string sb;
        while (true) {
            char ch = (char)ReadByte();
            if (ch == '\0') break;
            sb.push_back(ch);
        }
        return sb;
    }

    std::string ReadNullTerminatedUTF8() {
        std::string sb;
        while (true) {
            uint8_t b = ReadByte();
            if (b == 0) break;
            sb.push_back((char)b);
        }
        return sb; // bytes are kept as UTF-8
    }

    /// Reads a pascal string from the current stream (BinaryReader.ReadString:
    /// 7-bit encoded length prefix followed by UTF-8 bytes).
    std::string ReadVariableLengthPascalString() {
        uint32_t length = 0; int shift = 0; uint8_t b;
        do {
            b = ReadByte();
            length |= (uint32_t)(b & 0x7F) << shift;
            shift += 7;
        } while ((b & 0x80) != 0 && shift < 35);
        auto bytes = ReadBytes(length);
        return std::string(bytes.begin(), bytes.end());
    }

    /// Reads a pascal string from the current stream, prefixed by a byte.
    std::string ReadPascalString() {
        uint8_t length = ReadByte();
        auto b = ReadBytes(length);
        return std::string(b.begin(), b.end());
    }

    /// Reads a float from the current stream. NOTE: always little endian, as in the C#.
    float ReadFloat() {
        need(4);
        uint32_t tmpBuffer = (uint32_t)(d_[p_] | (d_[p_+1] << 8) | (d_[p_+2] << 16) | ((uint32_t)d_[p_+3] << 24));
        p_ += 4;
        float result;
        std::memcpy(&result, &tmpBuffer, 4);
        return result;
    }

    /// Sets a mark at the current position in the stream.
    void Mark() { mark_ = p_; }
    /// Seeks in the current stream from the current mark plus the number of bytes.
    void SeekFromMark(int64_t numBytes) { Seek(SeekOrigin::Begin, (int64_t)mark_ + numBytes); }

private:
    void need(size_t k) const { if (p_ + k > n_ || p_ > n_) throw std::runtime_error("read past end"); }
    const uint8_t* d_;
    size_t n_, p_;
    Endian e_;
    size_t mark_;
};

/// Growable byte sink mirroring FSO.Files.Utils.IoWriter over a MemoryStream.
class Writer {
public:
    explicit Writer(Endian e = Endian::Big) : p_(0), e_(e) {}

    void endian(Endian e) { e_ = e; }
    Endian endian() const { return e_; }
    void ByteOrder(Endian e) { e_ = e; }
    Endian ByteOrder() const { return e_; }

    bool HasMore() const { return p_ + 1 < buf_.size(); }
    void Skip(int64_t numBytes) { Seek(SeekOrigin::Current, numBytes); }
    void Seek(SeekOrigin origin, int64_t offset) {
        int64_t base = 0;
        switch (origin) {
            case SeekOrigin::Begin:   base = 0; break;
            case SeekOrigin::Current: base = (int64_t)p_; break;
            case SeekOrigin::End:     base = (int64_t)buf_.size(); break;
        }
        int64_t np = base + offset;
        if (np < 0) throw std::runtime_error("seek before beginning");
        p_ = (size_t)np;
        if (p_ > buf_.size()) buf_.resize(p_);
    }
    int64_t Position() const { return (int64_t)p_; }
    size_t size() const { return buf_.size(); }

    /// Writes a variable length unsigned integer to the current stream
    void WriteVarLen(uint32_t value) {
        bool first = true;
        while (value > 0 || first) {
            WriteByte((uint8_t)(((value > 127) ? (uint32_t)128 : 0) | (value & 127)));
            value >>= 7;
            first = false;
        }
    }

    void WriteUInt16(uint16_t value) {
        if (e_ == Endian::Big) { put((uint8_t)(value >> 8)); put((uint8_t)value); }
        else { put((uint8_t)value); put((uint8_t)(value >> 8)); }
    }
    void WriteInt16(int16_t value) { WriteUInt16((uint16_t)value); }
    void WriteUInt32(uint32_t value) {
        if (e_ == Endian::Big) { put((uint8_t)(value >> 24)); put((uint8_t)(value >> 16)); put((uint8_t)(value >> 8)); put((uint8_t)value); }
        else { put((uint8_t)value); put((uint8_t)(value >> 8)); put((uint8_t)(value >> 16)); put((uint8_t)(value >> 24)); }
    }
    void WriteInt32(int32_t value) { WriteUInt32((uint32_t)value); }

    /// Writes a number of ASCII characters to the current stream, padded with 0 to num
    /// (like string.PadRight, a longer string is not truncated).
    void WriteCString(const std::string& value, int num) {
        for (char c : value) put((uint8_t)c);
        for (size_t i = value.size(); i < (size_t)num; i++) put(0);
    }
    void WriteCString(const std::string& value) { WriteCString(value, (int)value.size() + 1); }

    void WriteByte(uint8_t value) { put(value); }
    void WriteBytes(const std::vector<uint8_t>& bytes) { for (uint8_t b : bytes) put(b); }
    void WriteBytes(const uint8_t* bytes, size_t n) { for (size_t i = 0; i < n; i++) put(bytes[i]); }

    /// Writes a pascal string to the current stream, which is prefixed by a 16bit short.
    void WriteLongPascalString(const std::string& value) {
        WriteInt16((int16_t)value.size());
        for (char c : value) put((uint8_t)c);
    }
    /// Writes a C string to the current stream.
    void WriteNullTerminatedString(const std::string& value) {
        for (char c : value) put((uint8_t)c);
        WriteByte(0);
    }
    /// Writes a pascal string to the current stream (BinaryWriter.Write(string)).
    void WriteVariableLengthPascalString(const std::string& value) {
        uint32_t v = (uint32_t)value.size();
        while (v >= 0x80) { put((uint8_t)(v | 0x80)); v >>= 7; }
        put((uint8_t)v);
        for (char c : value) put((uint8_t)c);
    }
    /// Writes a pascal string to the current stream, prefixed by a byte.
    void WritePascalString(const std::string& value) {
        WriteByte((uint8_t)value.size());
        for (char c : value) put((uint8_t)c);
    }
    /// Writes a float to the current stream (honours the byte order, as the C# does).
    void WriteFloat(float value) {
        uint32_t bits; std::memcpy(&bits, &value, 4);
        WriteUInt32(bits);
    }

    /// MemoryStream.ToArray()
    const std::vector<uint8_t>& ToArray() const { return buf_; }
    std::vector<uint8_t> TakeArray() { std::vector<uint8_t> r; r.swap(buf_); p_ = 0; return r; }

private:
    void put(uint8_t b) {
        if (p_ < buf_.size()) buf_[p_] = b;
        else buf_.push_back(b);
        p_++;
    }
    std::vector<uint8_t> buf_;
    size_t p_;
    Endian e_;
};

/// TS1(sims): set by src/files/FilePool.cpp: closes the archive handles the pool keeps open (used when an
/// fopen fails, e.g. out of handles on the 3DS), and the reason of the last ReadAllBytes failure ("path: strerror").
inline void (*ReleaseFileHandles)() = nullptr;
inline std::string LastFileError;

/// File.ReadAllBytes equivalent (fopen works on the 3DS sdmc). Returns false when the
/// file cannot be opened (st::LastFileError says why).
inline bool ReadAllBytes(const std::string& path, std::vector<uint8_t>& out) {
    errno = 0;
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f && ReleaseFileHandles) { ReleaseFileHandles(); errno = 0; f = std::fopen(path.c_str(), "rb"); }
    if (!f) { LastFileError = path + ": " + (errno ? std::strerror(errno) : "cannot open"); return false; }
    std::setvbuf(f, nullptr, _IONBF, 0); // one big fread
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (n < 0) { LastFileError = path + ": cannot get the size"; std::fclose(f); return false; }
    try { out.resize((size_t)n); }
    catch (...) { std::fclose(f); throw std::runtime_error(path + ": out of memory reading " + std::to_string(n) + " bytes"); }
    size_t got = n ? std::fread(out.data(), 1, (size_t)n, f) : 0;
    std::fclose(f);
    out.resize(got);
    return true;
}

} // namespace st
