/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FAR1 ("FAR!byAZ" v1) archive reader. Reference: FreeSims sims.files/FAR1.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

namespace st {

struct FarEntry {
    std::string name;
    uint32_t offset = 0;
    uint32_t length = 0;
};

class Far1 {
public:
    // Loads the archive index. Keeps the backing file bytes in memory; small
    // enough for the global/build archives, and on 3DS we can switch to a
    // seek-on-demand reader later without changing callers.
    bool open(const std::string& path);
    bool openBytes(std::vector<uint8_t> bytes);

    const std::vector<FarEntry>& entries() const { return entries_; }
    bool has(const std::string& name) const { return index_.count(name) != 0; }

    // Returns a copy of one member's bytes, or empty vector if absent.
    std::vector<uint8_t> read(const std::string& name) const;
    std::vector<uint8_t> read(const FarEntry& e) const;

private:
    bool parse();
    std::vector<uint8_t> buf_;
    std::vector<FarEntry> entries_;
    std::unordered_map<std::string, size_t> index_;
};

} // namespace st
