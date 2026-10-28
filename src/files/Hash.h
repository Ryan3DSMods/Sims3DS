/*
xxHashSharp - A pure C# implementation of xxhash
Copyright (C) 2014, Seok-Ju, Yun. (https://github.com/noricube/xxHashSharp)
Original C Implementation Copyright (C) 2012-2014, Yann Collet. (https://code.google.com/p/xxhash/)
BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

*/
// Translated from FreeSims sims.files/Hash.cs
#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>

namespace FSO { namespace Files {

class Hash
{
public:
    struct XXH_State
    {
        uint64_t total_len = 0;
        uint32_t seed = 0;
        uint32_t v1 = 0;
        uint32_t v2 = 0;
        uint32_t v3 = 0;
        uint32_t v4 = 0;
        int32_t memsize = 0;
        uint8_t memory[16] = {0};
    };

    static constexpr uint32_t PRIME32_1 = 2654435761U;
    static constexpr uint32_t PRIME32_2 = 2246822519U;
    static constexpr uint32_t PRIME32_3 = 3266489917U;
    static constexpr uint32_t PRIME32_4 = 668265263U;
    static constexpr uint32_t PRIME32_5 = 374761393U;

protected:
    XXH_State _state;

public:
    Hash() {}

    static uint32_t CalculateHash(const uint8_t* buf, int32_t len, uint32_t seed = 0);
    static uint32_t CalculateHash(const std::vector<uint8_t>& buf, int32_t len = -1, uint32_t seed = 0);

    void Init(uint32_t seed = 0);
    bool Update(const uint8_t* input, int32_t len);
    bool Update(const std::vector<uint8_t>& input, int32_t len) { return Update(input.data(), len); }
    uint32_t Digest();

private:
    static uint32_t ToUInt32(const uint8_t* buf, int32_t index)
    {
        return (uint32_t)buf[index] | ((uint32_t)buf[index + 1] << 8) | ((uint32_t)buf[index + 2] << 16) | ((uint32_t)buf[index + 3] << 24);
    }
    static uint32_t CalcSubHash(uint32_t value, const uint8_t* buf, int32_t index);
    static uint32_t RotateLeft(uint32_t value, int count)
    {
        return (value << count) | (value >> (32 - count));
    }
};

}} // namespace FSO::Files
