/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// TS1(sims): writing side of IffFieldEncode; see the header.
#include "files/formats/iff/IffFieldEncodeWriter.h"
#include <cstring>
#include <stdexcept>

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

const uint8_t IffFieldEncodeWriter::Widths16[4] = { 5, 8, 13, 16 };
const uint8_t IffFieldEncodeWriter::Widths32[4] = { 6, 11, 21, 32 };
const uint8_t IffFieldEncodeWriter::WidthsByte[4] = { 2, 4, 6, 8 };

void IffFieldEncodeWriter::WriteBit(int b)
{
    if (bitPos == 0) Data.push_back(0);
    if (b) Data.back() |= (uint8_t)(1 << (7 - bitPos));
    bitPos = (bitPos + 1) & 7;
}

void IffFieldEncodeWriter::WriteBits(uint64_t v, int n)
{
    for (int i = n - 1; i >= 0; i--) WriteBit((int)((v >> i) & 1));
}

void IffFieldEncodeWriter::WriteField(int64_t value, const uint8_t* widths)
{
    if (value == 0) { WriteBit(0); return; }
    for (int code = 0; code < 4; code++) {
        int w = widths[code];
        int64_t lo = -((int64_t)1 << (w - 1)), hi = ((int64_t)1 << (w - 1)) - 1;
        if (value >= lo && value <= hi) {
            WriteBit(1);
            WriteBits((uint64_t)code, 2);
            WriteBits((uint64_t)value & ((w >= 64) ? ~0ull : ((1ull << w) - 1)), w);
            return;
        }
    }
    throw std::runtime_error("IffFieldEncodeWriter: value does not fit the widest field");
}

void IffFieldEncodeWriter::WriteFloat(float f)
{
    uint32_t bits;
    std::memcpy(&bits, &f, sizeof(bits));
    WriteUInt32(bits);
}

void IffFieldEncodeWriter::WriteString(const std::string& s, bool nextField)
{
    (void)nextField;
    bitPos = 0; // the rest of a partial byte is skipped by the reader
    for (char c : s) Data.push_back((uint8_t)c);
    Data.push_back(0);
    if (((StartOffset + (int64_t)Data.size()) & 1) != 0) Data.push_back(StringPad); //2 byte pad
}

void IffFieldEncodeWriter::WriteBitString(const std::string& bits)
{
    for (char c : bits) {
        if (c == '0') WriteBit(0);
        else if (c == '1') WriteBit(1);
    }
}

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
