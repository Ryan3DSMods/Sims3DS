/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// TS1(sims): ARRY writer; see the header.
#include "files/formats/iff/chunks/ARRYWriter.h"
#include <algorithm>

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

static void Put16(std::vector<uint8_t>& o, uint16_t v) { o.push_back((uint8_t)v); o.push_back((uint8_t)(v >> 8)); }
static void Put32(std::vector<uint8_t>& o, int32_t v) { for (int k = 0; k < 4; k++) o.push_back((uint8_t)((uint32_t)v >> (8 * k))); }

std::vector<uint8_t> WriteARRYChunk(int32_t width, int32_t height, int32_t bytesPerElement, const std::vector<uint8_t>& data,
                                    int32_t unknown)
{
    std::vector<uint8_t> o;
    Put32(o, 0);
    Put32(o, width);
    Put32(o, height);
    Put32(o, bytesPerElement);
    Put32(o, unknown);
    const size_t n = data.size();
    const size_t MinRun = 8;
    size_t i = 0;
    std::vector<uint8_t> lit;
    auto flushLiteral = [&]() {
        size_t p = 0;
        while (p < lit.size()) {
            size_t take = std::min<size_t>(lit.size() - p, 0x7FFE);
            Put16(o, (uint16_t)take);
            o.insert(o.end(), lit.begin() + (long)p, lit.begin() + (long)(p + take));
            if (take & 1) o.push_back(0);
            p += take;
        }
        lit.clear();
    };
    while (i < n) {
        size_t r = i + 1;
        while (r < n && data[r] == data[i] && r - i < 0x7FFF) r++;
        size_t len = r - i;
        if (len >= MinRun) {
            flushLiteral();
            Put16(o, (uint16_t)(0x8000 | len));
            o.push_back(data[i]);
            o.push_back(0);
            i = r;
        } else {
            lit.insert(lit.end(), data.begin() + (long)i, data.begin() + (long)r);
            i = r;
        }
    }
    flushLiteral();
    return o;
}

std::vector<uint8_t> TransposeARRY(const std::vector<uint8_t>& data, int32_t width, int32_t stride)
{
    std::vector<uint8_t> result(data.size());
    for (size_t i = 0; i + (size_t)stride <= data.size(); i += (size_t)stride) {
        int32_t divI = (int32_t)i / stride;
        int32_t x = divI % width;
        int32_t y = divI / width;
        size_t targetIndex = (size_t)y * stride + (size_t)x * stride * width;
        for (int32_t j = 0; j < stride && targetIndex < result.size(); j++) result[targetIndex++] = data[i + (size_t)j];
    }
    return result;
}

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
