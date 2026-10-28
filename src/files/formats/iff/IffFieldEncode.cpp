/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/formats/iff/IffFieldEncode.cs
#include "files/formats/iff/IffFieldEncode.h"
#include <cstring>

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

void IffFieldEncode::setBytePos(int32_t n)
{
    io->Seek(st::SeekOrigin::Begin, n);
    curByte = io->ReadByte();
    bitPos = 0;
}

uint16_t IffFieldEncode::ReadUInt16()
{
    return (uint16_t)ReadField(false);
}

int16_t IffFieldEncode::ReadInt16()
{
    return (int16_t)ReadField(false);
}

int32_t IffFieldEncode::ReadInt32()
{
    return (int32_t)ReadField(true);
}

uint32_t IffFieldEncode::ReadUInt32()
{
    return (uint32_t)ReadField(true);
}

float IffFieldEncode::ReadFloat()
{
    // TS1(sims): FreeSO — the 32-bit field holds the IEEE float bits (FreeSims converted the integer: "this is
    // incredibly wrong").
    uint32_t data = (uint32_t)ReadField(true);
    float f;
    std::memcpy(&f, &data, sizeof(f));
    return f;
}

uint8_t IffFieldEncode::ReadByte()
{
    return (uint8_t)ReadField(widthsByte);
}

void IffFieldEncode::Interrupt()
{
    int64_t targetPos = io->Position();

    if (bitPos == 0)
    {
        targetPos--;
    }

    io->Seek(st::SeekOrigin::Begin, targetPos);
}

std::string IffFieldEncode::BitDebug(int32_t count)
{
    std::string result;

    for (int i = 0; i < count; i++)
    {
        auto bit = ReadBit();

        result += bit == 1 ? "1" : "0";

        if (bitPos == 0)
        {
            result += "|";
        }
    }

    return result;
}

std::string IffFieldEncode::BitDebugTil(int64_t skipPosition)
{
    int64_t currentPos = bitPos == 0 ? io->Position() : io->Position() - 1;

    int32_t diff = (int32_t)(skipPosition - currentPos) * 8 - bitPos;

    if (diff < 0)
    {
        return "oob";
    }

    return BitDebug(diff);
}

int64_t IffFieldEncode::ReadField(const uint8_t* widths)
{
    if (ReadBit() == 0) return 0;

    uint32_t code = ReadBits(2);
    uint8_t width = widths[code];
    int64_t value = ReadBits(width);
    value |= -(value & ((int64_t)1 << (width - 1)));

    if (value == 0)
    {
        // not valid
    }

    return value;
}

int64_t IffFieldEncode::ReadField(bool big)
{
    if (ReadBit() == 0) return 0;

    uint32_t code = ReadBits(2);
    uint8_t width = (big) ? widths2[code] : widths[code];
    int64_t value = ReadBits(width);
    value |= -(value & ((int64_t)1 << (width - 1)));

    return value;
}

uint32_t IffFieldEncode::ReadBits(int32_t n)
{
    uint32_t total = 0;
    for (int i = 0; i < n; i++)
    {
        total += (uint32_t)((uint32_t)ReadBit() << ((n - i) - 1));
    }
    return total;
}

uint8_t IffFieldEncode::ReadBit()
{
    uint8_t result = (uint8_t)((curByte & (1 << (7 - bitPos))) >> (7 - bitPos));
    if (++bitPos > 7)
    {
        bitPos = 0;
        //C#: try { curByte = io.ReadByte(); odd = !odd; } catch { curByte = 0; } (no more data, read 0)
        if (io->Position() < io->Length())
        {
            curByte = io->ReadByte();
            odd = !odd;
        }
        else
        {
            curByte = 0; //no more data, read 0
            odd = !odd; // TS1(sims): FreeSO
            StreamEnd = true;
        }
    }
    return result;
}

std::string IffFieldEncode::ReadString(bool nextField)
{
    if (bitPos == 0)
    {
        io->Seek(st::SeekOrigin::Current, -1);
        odd = !odd;
    }
    auto str = io->ReadNullTerminatedString();
    if ((str.size() % 2 == 0) == !odd) io->ReadByte(); //2 byte pad

    bitPos = 8;
    if (nextField && io->HasMore())
    {
        curByte = io->ReadByte();
        odd = true;
        bitPos = 0;
    } else
    {
        odd = false;
    }

    return str;
}

IffFieldEncode::IffFieldEncode(st::Reader* io, bool oddOffset) : IOProxy(io)
{
    curByte = io->ReadByte();
    odd = !oddOffset;
    bitPos = 0;
}

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
