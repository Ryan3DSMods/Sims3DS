/*
xxHashSharp - A pure C# implementation of xxhash
Copyright (C) 2014, Seok-Ju, Yun. (https://github.com/noricube/xxHashSharp)
Original C Implementation Copyright (C) 2012-2014, Yann Collet. (https://code.google.com/p/xxhash/)
BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

*/
// Translated from FreeSims sims.files/Hash.cs (xxHash32)
#include "files/Hash.h"
#include <cstring>

namespace FSO { namespace Files {

uint32_t Hash::CalculateHash(const std::vector<uint8_t>& buf, int32_t len, uint32_t seed)
{
    if (len == -1) len = (int32_t)buf.size();
    return CalculateHash(buf.data(), len, seed);
}

uint32_t Hash::CalculateHash(const uint8_t* buf, int32_t len, uint32_t seed)
{
    uint32_t h32;
    int32_t index = 0;

    if (len >= 16)
    {
        int32_t limit = len - 16;
        uint32_t v1 = seed + PRIME32_1 + PRIME32_2;
        uint32_t v2 = seed + PRIME32_2;
        uint32_t v3 = seed + 0;
        uint32_t v4 = seed - PRIME32_1;

        do
        {
            v1 = CalcSubHash(v1, buf, index);
            index += 4;
            v2 = CalcSubHash(v2, buf, index);
            index += 4;
            v3 = CalcSubHash(v3, buf, index);
            index += 4;
            v4 = CalcSubHash(v4, buf, index);
            index += 4;
        } while (index <= limit);

        h32 = RotateLeft(v1, 1) + RotateLeft(v2, 7) + RotateLeft(v3, 12) + RotateLeft(v4, 18);
    }
    else
    {
        h32 = seed + PRIME32_5;
    }

    h32 += (uint32_t)len;

    while (index <= len - 4)
    {
        h32 += ToUInt32(buf, index) * PRIME32_3;
        h32 = RotateLeft(h32, 17) * PRIME32_4;
        index += 4;
    }

    while (index < len)
    {
        h32 += buf[index] * PRIME32_5;
        h32 = RotateLeft(h32, 11) * PRIME32_1;
        index++;
    }

    h32 ^= h32 >> 15;
    h32 *= PRIME32_2;
    h32 ^= h32 >> 13;
    h32 *= PRIME32_3;
    h32 ^= h32 >> 16;

    return h32;
}

void Hash::Init(uint32_t seed)
{
    _state.seed = seed;
    _state.v1 = seed + PRIME32_1 + PRIME32_2;
    _state.v2 = seed + PRIME32_2;
    _state.v3 = seed + 0;
    _state.v4 = seed - PRIME32_1;
    _state.total_len = 0;
    _state.memsize = 0;
    std::memset(_state.memory, 0, 16);
}

bool Hash::Update(const uint8_t* input, int32_t len)
{
    int32_t index = 0;

    _state.total_len += (uint32_t)len;

    if (_state.memsize + len < 16) // buffer + input length under 16 bytes: just store in the buffer
    {
        std::memcpy(_state.memory + _state.memsize, input, len);
        _state.memsize += len;

        return true;
    }

    if (_state.memsize > 0) // leftover data from before: process it first.
    {
        std::memcpy(_state.memory + _state.memsize, input, 16 - _state.memsize);

        _state.v1 = CalcSubHash(_state.v1, _state.memory, index);
        index += 4;
        _state.v2 = CalcSubHash(_state.v2, _state.memory, index);
        index += 4;
        _state.v3 = CalcSubHash(_state.v3, _state.memory, index);
        index += 4;
        _state.v4 = CalcSubHash(_state.v4, _state.memory, index);
        index += 4;

        index = 0;
        _state.memsize = 0;
    }

    if (index <= len - 16)
    {
        int32_t limit = len - 16;
        uint32_t v1 = _state.v1;
        uint32_t v2 = _state.v2;
        uint32_t v3 = _state.v3;
        uint32_t v4 = _state.v4;

        do
        {
            v1 = CalcSubHash(v1, input, index);
            index += 4;
            v2 = CalcSubHash(v2, input, index);
            index += 4;
            v3 = CalcSubHash(v3, input, index);
            index += 4;
            v4 = CalcSubHash(v4, input, index);
            index += 4;
        } while (index <= limit);

        _state.v1 = v1;
        _state.v2 = v2;
        _state.v3 = v3;
        _state.v4 = v4;
    }

    if (index < len)
    {
        std::memcpy(_state.memory, input + index, len - index);
        _state.memsize = len - index;
    }
    return true;
}

uint32_t Hash::Digest()
{
    uint32_t h32;
    int32_t index = 0;
    if (_state.total_len >= 16)
    {
        h32 = RotateLeft(_state.v1, 1) + RotateLeft(_state.v2, 7) + RotateLeft(_state.v3, 12) + RotateLeft(_state.v4, 18);
    }
    else
    {
        h32 = _state.seed + PRIME32_5;
    }

    h32 += (uint32_t)_state.total_len;

    while (index <= _state.memsize - 4)
    {
        h32 += ToUInt32(_state.memory, index) * PRIME32_3;
        h32 = RotateLeft(h32, 17) * PRIME32_4;
        index += 4;
    }

    while (index < _state.memsize)
    {
        h32 += _state.memory[index] * PRIME32_5;
        h32 = RotateLeft(h32, 11) * PRIME32_1;
        index++;
    }

    h32 ^= h32 >> 15;
    h32 *= PRIME32_2;
    h32 ^= h32 >> 13;
    h32 *= PRIME32_3;
    h32 ^= h32 >> 16;

    return h32;
}

uint32_t Hash::CalcSubHash(uint32_t value, const uint8_t* buf, int32_t index)
{
    uint32_t read_value = ToUInt32(buf, index);
    value += read_value * PRIME32_2;
    value = RotateLeft(value, 13);
    value *= PRIME32_1;
    return value;
}

}} // namespace FSO::Files
