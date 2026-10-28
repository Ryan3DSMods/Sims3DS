/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/. 
 */
// Translated from FreeSims sims.files/Endian.cs
#pragma once
#include <cstdint>

namespace FSO { namespace Files {

class Endian
{
public:
    static int16_t SwapInt16(int16_t v)
    {
        return (int16_t)(((v & 0xff) << 8) | ((v >> 8) & 0xff));
    }

    static uint16_t SwapUInt16(uint16_t v)
    {
        return (uint16_t)(((v & 0xff) << 8) | ((v >> 8) & 0xff));
    }

    static int32_t SwapInt32(int32_t v)
    {
        return (int32_t)(((SwapInt16((int16_t)v) & 0xffff) << 0x10) |
                         (SwapInt16((int16_t)(v >> 0x10)) & 0xffff));
    }

    static uint32_t SwapUInt32(uint32_t v)
    {
        return (uint32_t)(((SwapUInt16((uint16_t)v) & 0xffff) << 0x10) |
                          (SwapUInt16((uint16_t)(v >> 0x10)) & 0xffff));
    }

    static int64_t SwapInt64(int64_t v)
    {
        return (int64_t)(((SwapInt32((int32_t)v) & 0xffffffffLL) << 0x20) |
                         (SwapInt32((int32_t)(v >> 0x20)) & 0xffffffffLL));
    }

    static uint64_t SwapUInt64(uint64_t v)
    {
        return (uint64_t)(((SwapUInt32((uint32_t)v) & 0xffffffffLL) << 0x20) |
                          (SwapUInt32((uint32_t)(v >> 0x20)) & 0xffffffffLL));
    }

    static bool IsBigEndian() { return !IsLittleEndian(); }

    static bool IsLittleEndian()
    {
        const uint16_t one = 1;
        return *reinterpret_cast<const uint8_t*>(&one) == 1;
    }
};

}} // namespace FSO::Files
