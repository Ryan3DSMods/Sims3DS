/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
#include "far1.h"
#include "../core/io.h"
#include <cstdio>

namespace st {

static std::vector<uint8_t> slurp(const std::string& path) {
    std::vector<uint8_t> v;
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return v;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n > 0) { v.resize((size_t)n); if (fread(v.data(), 1, (size_t)n, f) != (size_t)n) v.clear(); }
    fclose(f);
    return v;
}

bool Far1::open(const std::string& path) {
    buf_ = slurp(path);
    if (buf_.empty()) return false;
    return parse();
}

bool Far1::openBytes(std::vector<uint8_t> bytes) {
    buf_ = std::move(bytes);
    return parse();
}

bool Far1::parse() {
    entries_.clear(); index_.clear();
    try {
        Reader r(buf_, Endian::Little);
        std::string magic = r.cstr(8, false);
        uint32_t version = r.u32();
        if (magic != "FAR!byAZ" || version != 1) return false;

        uint32_t manifestOffset = r.u32();
        r.seek(manifestOffset);
        uint32_t numFiles = r.u32();
        entries_.reserve(numFiles);

        for (uint32_t i = 0; i < numFiles; i++) {
            FarEntry e;
            uint32_t dataLength  = r.u32();   // DataLength
            (void) r.u32();                   // DataLength2 (duplicate)
            uint32_t dataOffset  = r.u32();   // DataOffset
            // The Sims uses FAR 1a: 4-byte filename length.
            uint32_t nameLen     = r.u32();
            std::string name     = r.cstr(nameLen, false);
            e.name = name; e.length = dataLength; e.offset = dataOffset;
            index_[name] = entries_.size();
            entries_.push_back(std::move(e));
        }
        return true;
    } catch (...) {
        entries_.clear(); index_.clear();
        return false;
    }
}

std::vector<uint8_t> Far1::read(const FarEntry& e) const {
    std::vector<uint8_t> out;
    if ((size_t)e.offset + e.length > buf_.size()) return out;
    out.assign(buf_.begin() + e.offset, buf_.begin() + e.offset + e.length);
    return out;
}

std::vector<uint8_t> Far1::read(const std::string& name) const {
    auto it = index_.find(name);
    if (it == index_.end()) return {};
    return read(entries_[it->second]);
}

} // namespace st
