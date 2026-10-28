/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/formats/iff/IffFieldEncode.cs
#pragma once
#include "files/formats/iff/chunks/TTAB.h"

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

/// <summary>
/// Used to read values from field encoded stream.
/// </summary>
class IffFieldEncode : public IOProxy
{
private:
    uint8_t bitPos = 0;
    uint8_t curByte = 0;
    bool odd = false;
public:
    uint8_t widths[4] = { 5, 8, 13, 16 };
    uint8_t widths2[4] = { 6, 11, 21, 32 };
    uint8_t widthsByte[4] = { 2, 4, 6, 8 };
    bool StreamEnd = false; // TS1(sims): FreeSO

    void setBytePos(int32_t n);

    uint16_t ReadUInt16() override;
    int16_t ReadInt16() override;
    int32_t ReadInt32() override;
    uint32_t ReadUInt32() override;
    float ReadFloat() override;
    uint8_t ReadByte();

    void Interrupt();

    std::string BitDebug(int32_t count);
    std::string BitDebugTil(int64_t skipPosition);

private:
    int64_t ReadField(const uint8_t* widths);
    int64_t ReadField(bool big);
    uint32_t ReadBits(int32_t n);
    uint8_t ReadBit();

public:
    std::string ReadString(bool nextField);

    // TS1(sims): FreeSO adds oddOffset (field encoded data starting at an odd offset from the chunk base,
    // which flips the 2-byte string padding rule). FreeSims always started even.
    explicit IffFieldEncode(st::Reader* io, bool oddOffset = false);
};

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
