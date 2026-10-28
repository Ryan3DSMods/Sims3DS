/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// TS1(sims): raw IFF chunk list with rsmp regeneration; see the header.
#include "files/formats/iff/IffRawFile.h"
#include "core/io.h"
#include "files/SafeFile.h"
#include <cstdio>
#include <cstring>
#include <map>
#include <set>

namespace FSO { namespace Files { namespace Formats { namespace IFF {

static uint32_t BE32(const uint8_t* p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
static uint16_t BE16(const uint8_t* p) { return (uint16_t)((p[0] << 8) | p[1]); }
static uint32_t LE32(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint16_t LE16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

bool IffRawFile::Load(const std::string& path, std::string* error)
{
    std::vector<uint8_t> data;
    if (!st::ReadAllBytes(path, data)) { if (error) *error = "cannot open " + path; return false; }
    return Parse(data, error);
}

bool IffRawFile::Parse(const std::vector<uint8_t>& data, std::string* error)
{
    Chunks.clear(); RsmpOrder.clear(); TrailingBytes.clear();
    HadRsmp = false; RsmpIndex = -1;
    if (data.size() < 64 || std::memcmp(data.data(), "IFF FILE 2.", 11) != 0) { if (error) *error = "not an IFF file"; return false; }
    Header.assign(data.begin(), data.begin() + 64);
    size_t off = 64;
    std::vector<uint8_t> rsmpData;
    while (off + 76 <= data.size()) {
        const uint8_t* h = data.data() + off;
        uint32_t size = BE32(h + 4);
        if (size < 76 || off + size > data.size()) break;
        Chunk c;
        c.Type.assign((const char*)h, 4);
        c.ID = BE16(h + 8);
        c.Flags = BE16(h + 10);
        c.LabelRaw.assign(h + 12, h + 76);
        c.Label.assign((const char*)h + 12, strnlen((const char*)h + 12, 64));
        if (c.Type == "rsmp") {
            HadRsmp = true;
            RsmpIndex = (int)Chunks.size();
            RsmpID = c.ID; RsmpFlags = c.Flags; RsmpLabelRaw = c.LabelRaw;
            rsmpData.assign(h + 76, h + size);
        } else {
            c.Data.assign(h + 76, h + size);
            Chunks.push_back(std::move(c));
        }
        off += size;
    }
    TrailingBytes.assign(data.begin() + off, data.end());

    // the original map's order (types and entries) so the regenerated map lists them the same way
    if (rsmpData.size() >= 20 && std::memcmp(rsmpData.data() + 8, "pmsr", 4) == 0) {
        RsmpVersion = LE32(rsmpData.data() + 4);
        RsmpSizeField = LE32(rsmpData.data() + 12);
        uint32_t types = LE32(rsmpData.data() + 16);
        size_t p = 20;
        for (uint32_t t = 0; t < types && p + 8 <= rsmpData.size(); t++) {
            std::string type(4, ' ');
            for (int k = 0; k < 4; k++) type[k] = (char)rsmpData[p + 3 - k];
            uint32_t count = LE32(rsmpData.data() + p + 4);
            p += 8;
            std::vector<uint16_t> ids;
            for (uint32_t i = 0; i < count && p < rsmpData.size(); i++) {
                p += 4; // offset
                uint32_t id;
                if (RsmpVersion == 0) { id = LE16(rsmpData.data() + p); p += 2; }
                else { id = LE32(rsmpData.data() + p); p += 4; }
                p += 2; // flags
                if (RsmpVersion == 0) {
                    size_t s = p;
                    while (p < rsmpData.size() && rsmpData[p] != 0) p++;
                    p++;
                    if (((p - s) & 1) != 0) p++;
                } else {
                    uint8_t len = p < rsmpData.size() ? rsmpData[p] : 0;
                    p += 1 + len;
                }
                ids.push_back((uint16_t)id);
            }
            RsmpOrder.push_back(std::make_pair(type, ids));
        }
    }
    return true;
}

IffRawFile::Chunk* IffRawFile::Find(const std::string& type, uint16_t id)
{
    for (auto& c : Chunks) if (c.Type == type && c.ID == id) return &c;
    return nullptr;
}

IffRawFile::Chunk* IffRawFile::Set(const std::string& type, uint16_t id, const std::vector<uint8_t>& data, const std::string& label,
                                   uint16_t flags)
{
    if (auto c = Find(type, id)) { c->Data = data; return c; }
    Chunk c;
    c.Type = type; c.ID = id; c.Flags = flags; c.Label = label; c.Data = data;
    if (RsmpIndex >= 0 && RsmpIndex <= (int)Chunks.size()) {
        Chunks.insert(Chunks.begin() + RsmpIndex, std::move(c));
        return &Chunks[(size_t)RsmpIndex++];
    }
    Chunks.push_back(std::move(c));
    return &Chunks.back();
}

void IffRawFile::Remove(const std::string& type, uint16_t id)
{
    for (size_t i = 0; i < Chunks.size(); i++) {
        if (Chunks[i].Type == type && Chunks[i].ID == id) {
            Chunks.erase(Chunks.begin() + (long)i);
            if (RsmpIndex > (int)i) RsmpIndex--;
            return;
        }
    }
}

static void PutBE32(std::vector<uint8_t>& o, uint32_t v) { for (int k = 3; k >= 0; k--) o.push_back((uint8_t)(v >> (8 * k))); }
static void PutBE16(std::vector<uint8_t>& o, uint16_t v) { o.push_back((uint8_t)(v >> 8)); o.push_back((uint8_t)v); }
static void PutLE32(std::vector<uint8_t>& o, uint32_t v) { for (int k = 0; k < 4; k++) o.push_back((uint8_t)(v >> (8 * k))); }
static void PutLE16(std::vector<uint8_t>& o, uint16_t v) { o.push_back((uint8_t)v); o.push_back((uint8_t)(v >> 8)); }

static void PutChunkHeader(std::vector<uint8_t>& out, const IffRawFile::Chunk& c, size_t dataSize)
{
    for (int k = 0; k < 4; k++) out.push_back(k < (int)c.Type.size() ? (uint8_t)c.Type[k] : (uint8_t)' ');
    PutBE32(out, (uint32_t)(dataSize + 76));
    PutBE16(out, c.ID);
    PutBE16(out, c.Flags);
    std::string cur((const char*)c.LabelRaw.data(), c.LabelRaw.size() == 64 ? strnlen((const char*)c.LabelRaw.data(), 64) : 0);
    if (c.LabelRaw.size() == 64 && cur == c.Label) {
        out.insert(out.end(), c.LabelRaw.begin(), c.LabelRaw.end());
    } else {
        for (int k = 0; k < 64; k++) out.push_back(k < (int)c.Label.size() ? (uint8_t)c.Label[k] : (uint8_t)0);
    }
}

std::vector<uint8_t> IffRawFile::Write() const
{
    std::vector<uint8_t> out(Header);
    if (out.size() != 64) {
        out.assign(64, 0);
        const char* sig = "IFF FILE 2.5:TYPE FOLLOWED BY SIZE\0 JAMIE DOORNBOS & MAXIS 1";
        std::memcpy(out.data(), sig, 60);
    }
    std::vector<size_t> offsets(Chunks.size());
    size_t rsmpOffset = 0;

    // First pass: chunk offsets (the rsmp payload size depends only on labels, not on offsets).
    auto buildRsmp = [&](const std::vector<size_t>& offs) {
        std::vector<uint8_t> r;
        PutLE32(r, 0);
        PutLE32(r, RsmpVersion);
        r.push_back('p'); r.push_back('m'); r.push_back('s'); r.push_back('r');
        PutLE32(r, RsmpSizeField);
        // type order: the original map's, then new types in file order
        std::vector<std::string> types;
        std::set<std::string> seen;
        for (auto& t : RsmpOrder) if (!seen.count(t.first)) { types.push_back(t.first); seen.insert(t.first); }
        for (auto& c : Chunks) if (!seen.count(c.Type)) { types.push_back(c.Type); seen.insert(c.Type); }
        std::vector<std::pair<std::string, std::vector<size_t>>> lists;
        for (auto& t : types) {
            std::vector<size_t> idx;
            std::set<size_t> used;
            for (auto& o : RsmpOrder) if (o.first == t) {
                for (auto id : o.second) {
                    for (size_t i = 0; i < Chunks.size(); i++)
                        if (!used.count(i) && Chunks[i].Type == t && Chunks[i].ID == id) { idx.push_back(i); used.insert(i); break; }
                }
            }
            for (size_t i = 0; i < Chunks.size(); i++) if (Chunks[i].Type == t && !used.count(i)) { idx.push_back(i); used.insert(i); }
            if (!idx.empty()) lists.push_back(std::make_pair(t, idx));
        }
        PutLE32(r, (uint32_t)lists.size());
        for (auto& l : lists) {
            for (int k = 3; k >= 0; k--) r.push_back((uint8_t)l.first[(size_t)k]);
            PutLE32(r, (uint32_t)l.second.size());
            for (auto i : l.second) {
                auto& c = Chunks[i];
                PutLE32(r, (uint32_t)offs[i]);
                if (RsmpVersion == 0) PutLE16(r, c.ID); else PutLE32(r, c.ID);
                PutLE16(r, c.Flags);
                if (RsmpVersion == 0) {
                    size_t s = r.size();
                    for (char ch : c.Label) r.push_back((uint8_t)ch);
                    r.push_back(0);
                    if (((r.size() - s) & 1) != 0) r.push_back(0xA3);
                } else {
                    r.push_back((uint8_t)c.Label.size());
                    for (char ch : c.Label) r.push_back((uint8_t)ch);
                }
            }
        }
        return r;
    };

    size_t rsmpSize = 0;
    if (HadRsmp) rsmpSize = 76 + buildRsmp(std::vector<size_t>(Chunks.size(), 0)).size();
    size_t pos = 64;
    for (size_t i = 0; i <= Chunks.size(); i++) {
        if (HadRsmp && (int)i == RsmpIndex) { rsmpOffset = pos; pos += rsmpSize; }
        if (i == Chunks.size()) break;
        offsets[i] = pos;
        pos += 76 + Chunks[i].Data.size();
    }
    if (HadRsmp && RsmpIndex < 0) { rsmpOffset = pos; pos += rsmpSize; }

    if (HadRsmp) {
        out[60] = (uint8_t)(rsmpOffset >> 24); out[61] = (uint8_t)(rsmpOffset >> 16);
        out[62] = (uint8_t)(rsmpOffset >> 8); out[63] = (uint8_t)rsmpOffset;
    }
    std::vector<uint8_t> rsmp = HadRsmp ? buildRsmp(offsets) : std::vector<uint8_t>();
    out.reserve(pos + TrailingBytes.size());
    for (size_t i = 0; i <= Chunks.size(); i++) {
        if (HadRsmp && (int)i == RsmpIndex) {
            Chunk rc; rc.Type = "rsmp"; rc.ID = RsmpID; rc.Flags = RsmpFlags; rc.LabelRaw = RsmpLabelRaw;
            rc.Label.assign((const char*)RsmpLabelRaw.data(), RsmpLabelRaw.size() == 64 ? strnlen((const char*)RsmpLabelRaw.data(), 64) : 0);
            PutChunkHeader(out, rc, rsmp.size());
            out.insert(out.end(), rsmp.begin(), rsmp.end());
        }
        if (i == Chunks.size()) break;
        PutChunkHeader(out, Chunks[i], Chunks[i].Data.size());
        out.insert(out.end(), Chunks[i].Data.begin(), Chunks[i].Data.end());
    }
    out.insert(out.end(), TrailingBytes.begin(), TrailingBytes.end());
    return out;
}

bool IffRawFile::Save(const std::string& path, std::string* error) const
{
    // FIX(sims) SAVE: the one crash-safe writer (files/SafeFile.h). The old code (tmp, old -> .bak, tmp -> path) left
    // only .tmp / .bak on the card when the console died between the renames, and nothing read those back.
    return FSO::Files::SafeFile::Write(path, Write(), error);
}

}}}} // namespace FSO::Files::Formats::IFF
