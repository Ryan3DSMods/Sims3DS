/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Minimal RGBA PNG writer using zlib. Enough to dump decoded sprites for
// verification on the host build; the 3DS build never needs this.
#pragma once
#include <cstdint>
#include <cstdio>
#include <vector>
#include <string>
#include <zlib.h>

namespace st {

inline void put32(std::vector<uint8_t>& o, uint32_t v) {
    o.push_back((v>>24)&0xff); o.push_back((v>>16)&0xff);
    o.push_back((v>>8)&0xff);  o.push_back(v&0xff);
}

inline uint32_t crc32_of(const uint8_t* p, size_t n) {
    return (uint32_t)crc32(crc32(0L, Z_NULL, 0), p, (uInt)n);
}

inline void chunk(std::vector<uint8_t>& o, const char* type, const std::vector<uint8_t>& data) {
    put32(o, (uint32_t)data.size());
    size_t start = o.size();
    o.insert(o.end(), type, type+4);
    o.insert(o.end(), data.begin(), data.end());
    uint32_t c = crc32_of(&o[start], o.size()-start);
    put32(o, c);
}

// rgba: width*height*4 bytes
inline bool writePNG(const std::string& path, int w, int h, const uint8_t* rgba) {
    if (w<=0 || h<=0) return false;
    std::vector<uint8_t> raw; raw.reserve((size_t)(w*4+1)*h);
    for (int y=0;y<h;y++){ raw.push_back(0); raw.insert(raw.end(), rgba+(size_t)y*w*4, rgba+(size_t)(y+1)*w*4); }

    uLongf clen = compressBound((uLong)raw.size());
    std::vector<uint8_t> comp(clen);
    if (compress2(comp.data(), &clen, raw.data(), (uLong)raw.size(), 6) != Z_OK) return false;
    comp.resize(clen);

    std::vector<uint8_t> out;
    const uint8_t sig[8]={137,80,78,71,13,10,26,10};
    out.insert(out.end(), sig, sig+8);
    std::vector<uint8_t> ihdr;
    put32(ihdr,(uint32_t)w); put32(ihdr,(uint32_t)h);
    ihdr.push_back(8); ihdr.push_back(6); ihdr.push_back(0); ihdr.push_back(0); ihdr.push_back(0);
    chunk(out,"IHDR",ihdr);
    chunk(out,"IDAT",comp);
    chunk(out,"IEND",{});

    FILE* f=fopen(path.c_str(),"wb"); if(!f) return false;
    fwrite(out.data(),1,out.size(),f); fclose(f);
    return true;
}

} // namespace st
