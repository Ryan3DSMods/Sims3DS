/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// TS1(sims): the writing side of IffFieldEncode (files/formats/iff/IffFieldEncode.h).
// FreeSims and FreeSO only read the "field encoded" stream that The Sims 1 uses for OBJM records (and TTAB/SLOT
// v9/v10). This writer is the exact inverse of FreeSO's IffFieldEncode reader, so that every value it writes reads
// back the same way through IffFieldEncode:
//   - a value of 0 is the single bit 0;
//   - any other value is the bit 1, a 2-bit width code, then the value in that many bits (two's complement, MSB
//     first). Width tables: 16-bit fields {5, 8, 13, 16}, 32-bit fields {6, 11, 21, 32}, bytes {2, 4, 6, 8};
//   - the smallest width that holds the value is used (this reproduces the game's own files bit for bit: see
//     tools/simsave.cpp "objm" and STUBS_save.md);
//   - strings are byte aligned (the current partial byte is closed), NUL terminated, and padded with one byte so
//     that the position after them is even relative to the OBJM offset base ("2 byte pad" in the reader).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

class IffFieldEncodeWriter
{
public:
    /// startOffset: the position of the first written byte relative to the base the reader's odd/even padding rule
    /// uses (OBJM: the byte after the "MjbO" magic). Only its parity matters.
    explicit IffFieldEncodeWriter(int64_t startOffset = 0) : StartOffset(startOffset) {}

    std::vector<uint8_t> Data;
    int64_t StartOffset = 0;
    /// The pad byte written after strings that end at an odd position. The game leaves uninitialised memory there;
    /// its allocator fill is 0xA3 (the same byte pads rsmp labels and the OBJM tail).
    uint8_t StringPad = 0xA3;

    void WriteUInt16(uint16_t v) { WriteField((int64_t)(int16_t)v, Widths16); }
    void WriteInt16(int16_t v) { WriteField(v, Widths16); }
    void WriteInt32(int32_t v) { WriteField(v, Widths32); }
    void WriteUInt32(uint32_t v) { WriteField((int64_t)(int32_t)v, Widths32); }
    void WriteFloat(float f);
    void WriteByte(uint8_t v) { WriteField((int64_t)(int8_t)v, WidthsByte); }
    /// nextField has no effect on the bytes (the reader's flag only says what comes after); kept for symmetry.
    void WriteString(const std::string& s, bool nextField);

    /// Appends raw bits ("0101..." as produced by IffFieldEncode::BitDebug, '|' ignored): data the reader did not
    /// interpret, written back unchanged.
    void WriteBitString(const std::string& bits);

    /// Ends the bit stream at a byte boundary (the reader's Interrupt()).
    void Interrupt() { bitPos = 0; }

    /// Number of bits used in the last byte (0 = byte aligned).
    int BitPos() const { return bitPos; }

    static const uint8_t Widths16[4];
    static const uint8_t Widths32[4];
    static const uint8_t WidthsByte[4];

private:
    int bitPos = 0;
    void WriteBit(int b);
    void WriteBits(uint64_t v, int n);
    void WriteField(int64_t value, const uint8_t* widths);
};

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
