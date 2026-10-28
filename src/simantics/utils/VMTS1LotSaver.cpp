/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// TS1(sims): writes the running lot back into its The Sims 1 house file. See VMTS1LotSaver.h.
// The conversions mirror FreeSO tso.simantics/Utils/VMTS1ActivatorNew.cs (the TS1 house reader) field by field.
#include "simantics/utils/VMTS1LotSaver.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"
#include "simantics/VMArchitecture.h"
#include "simantics/VMClock.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/entities/VMGameObject.h"
#include "simantics/entities/VMMultitileGroup.h"
#include "simantics/engine/VMThread.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMQueuedAction.h"
#include "simantics/engine/VMTS1Neighbourhood.h"
#include "simantics/engine/VMTS1Travel.h"
#include "simantics/engine/translator/VMRoutine.h"
#include "simantics/model/VMStackObjectVariable.h"
#include "simantics/model/VMPersonDataVariable.h"
#include "simantics/model/VMMotive.h"
#include "simantics/model/Routing/VMObstacle.h"
#include "content/ContentManager.h"
#include "content/TS1NeighbourProvider.h"
#include "content/TS1NeighbourhoodWriter.h"
#include "files/formats/iff/IffRawFile.h"
#include "files/formats/iff/IffFieldEncode.h"
#include "files/formats/iff/chunks/OBJMWriter.h"
#include "files/formats/iff/chunks/ARRYWriter.h"
#include "files/formats/iff/chunks/ARRY.h"
#include "files/formats/iff/chunks/OBJD.h"
#include "files/formats/iff/chunks/OBJT.h"
#include "files/formats/iff/chunks/BHAV.h"
#include "core/io.h"
#include "files/LoadProgress.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <stdexcept>
#include <unordered_map>

using namespace FSO::Files::Formats::IFF;
using namespace FSO::Files::Formats::IFF::Chunks;
using namespace FSO::SimAntics;
using namespace FSO::SimAntics::Model;
using FSO::LotView::Model::LotTilePos;
using FSO::LotView::Model::FloorTile;
using FSO::LotView::Model::WallTile;
using FSO::LotView::Model::WallSegments;

namespace {

// ------------------------------------------------------------------ small helpers

void Put16(std::vector<uint8_t>& o, uint16_t v) { o.push_back((uint8_t)v); o.push_back((uint8_t)(v >> 8)); }
void Put32(std::vector<uint8_t>& o, uint32_t v) { for (int k = 0; k < 4; k++) o.push_back((uint8_t)(v >> (8 * k))); }
uint32_t LE32(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
uint16_t LE16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

std::string Pad2(int v) { char b[8]; snprintf(b, sizeof b, "%02d", v); return b; }

// ------------------------------------------------------------------ objt (raw, lossless)

/// The objt chunk: 12-byte header (pad, version, "tjbo"), entries, then a terminator entry (GUID 0) and whatever
/// follows it. Entries are kept as their original bytes; new ones are appended before the terminator.
struct OBJTRaw {
    struct Entry {
        uint32_t GUID = 0;
        uint16_t Unknown1a = 0, InitTreeVersion = 0, Unknown2a = 0, MainTreeVersion = 0, TypeID = 0, OBJDType = 0;
        std::string Name;
        int32_t Extra = 0; // version > 2
        std::vector<uint8_t> Raw;
    };
    std::vector<uint8_t> Header;
    int32_t Version = 2;
    std::vector<Entry> Entries;
    std::vector<uint8_t> Tail;

    bool Parse(const std::vector<uint8_t>& d) {
        if (d.size() < 12) return false;
        Header.assign(d.begin(), d.begin() + 12);
        Version = (int32_t)LE32(d.data() + 4);
        size_t p = 12;
        while (p + 4 <= d.size()) {
            uint32_t guid = LE32(d.data() + p);
            if (guid == 0) break;
            size_t s = p;
            if (p + 16 > d.size()) return false;
            Entry e;
            e.GUID = guid;
            e.Unknown1a = LE16(d.data() + p + 4); e.InitTreeVersion = LE16(d.data() + p + 6);
            e.Unknown2a = LE16(d.data() + p + 8); e.MainTreeVersion = LE16(d.data() + p + 10);
            e.TypeID = LE16(d.data() + p + 12); e.OBJDType = LE16(d.data() + p + 14);
            p += 16;
            while (p < d.size() && d[p] != 0) e.Name += (char)d[p++];
            p++;
            if (e.Name.size() % 2 == 0) p++; //pad to short width
            if (Version > 2) { if (p + 4 <= d.size()) e.Extra = (int32_t)LE32(d.data() + p); p += 4; }
            if (p > d.size()) return false;
            e.Raw.assign(d.begin() + (long)s, d.begin() + (long)p);
            Entries.push_back(std::move(e));
        }
        Tail.assign(d.begin() + (long)std::min(p, d.size()), d.end());
        return true;
    }
    static std::vector<uint8_t> Encode(const Entry& e, int32_t version) {
        std::vector<uint8_t> o;
        Put32(o, e.GUID);
        Put16(o, e.Unknown1a); Put16(o, e.InitTreeVersion); Put16(o, e.Unknown2a); Put16(o, e.MainTreeVersion);
        Put16(o, e.TypeID); Put16(o, e.OBJDType);
        for (char c : e.Name) o.push_back((uint8_t)c);
        o.push_back(0);
        if (e.Name.size() % 2 == 0) o.push_back(0xA3);
        if (version > 2) Put32(o, (uint32_t)e.Extra);
        return o;
    }
    std::vector<uint8_t> Write() const {
        std::vector<uint8_t> o(Header);
        for (auto& e : Entries) o.insert(o.end(), e.Raw.begin(), e.Raw.end());
        o.insert(o.end(), Tail.begin(), Tail.end());
        return o;
    }
    const Entry* ByGUID(uint32_t guid) const { for (auto& e : Entries) if (e.GUID == guid) return &e; return nullptr; }
    const Entry* ByType(uint16_t t) const { for (auto& e : Entries) if (e.TypeID == t) return &e; return nullptr; }
};

/// ADDED(sims) save progress: the data of chunk objt #0 of a house file through its resource map (rsmp): the first
/// 2 KB (file header + rsmp, which the game and IffRawFile write right after the header; ~0.5 KB in a house), then the
/// chunk itself -- 2-3 reads instead of the whole file (~400 KB; the ~100 files took ~2.6 s of a save on the 3DS,
/// now ~0.3 s; byte-identical saves, simsave roundtrip). Plain stdio, opened and
/// closed here (never the shared FilePool: the saver rewrites one of these files right after). Returns 1 with `out`
/// set, 0 when the file has no usable rsmp / objt entry (the caller reads it whole), -1 when the file cannot be opened.
int ReadObjtChunk(const std::string& path, std::vector<uint8_t>& out) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) return -1;
    auto be32 = [](const uint8_t* p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; };
    auto le32 = [](const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); };
    auto readAt = [&](uint32_t off, uint32_t len, std::vector<uint8_t>& buf) {
        buf.resize(len);
        return len == 0 || (std::fseek(f, (long)off, SEEK_SET) == 0 && std::fread(buf.data(), 1, len, f) == len);
    };
    int result = 0;
    std::vector<uint8_t> head(2048);
    size_t n = std::fread(head.data(), 1, head.size(), f);
    head.resize(n);
    do {
        if (n < 64 + 76 || std::memcmp(head.data(), "IFF FILE 2.", 11) != 0) break;
        uint32_t rsmpOff = be32(&head[60]);
        if (rsmpOff < 64) break;
        std::vector<uint8_t> rsmp;
        if ((size_t)rsmpOff + 76 <= head.size()) rsmp.assign(head.begin() + rsmpOff, head.begin() + rsmpOff + 76);
        else if (!readAt(rsmpOff, 76, rsmp)) break;
        if (std::memcmp(rsmp.data(), "rsmp", 4) != 0) break;
        uint32_t rsmpSize = be32(&rsmp[4]);
        if (rsmpSize < 76 + 20 || rsmpSize > (1u << 20)) break;
        std::vector<uint8_t> rd;
        if ((size_t)rsmpOff + rsmpSize <= head.size()) rd.assign(head.begin() + rsmpOff + 76, head.begin() + rsmpOff + rsmpSize);
        else if (!readAt(rsmpOff + 76, rsmpSize - 76, rd)) break;
        // reserved(4) version(4) 'pmsr'(4) size(4) typeCount(4) { type(4, reversed) count(4) { offset(4) id(2) flags(2) name\0 } }
        if (std::memcmp(&rd[8], "pmsr", 4) != 0) break;
        uint32_t version = le32(&rd[4]);
        uint32_t types = le32(&rd[16]);
        size_t p = 20;
        uint32_t objtOff = 0;
        bool ok = true;
        for (uint32_t t = 0; t < types && ok && objtOff == 0; t++) {
            if (p + 8 > rd.size()) { ok = false; break; }
            bool isObjt = rd[p + 3] == 'o' && rd[p + 2] == 'b' && rd[p + 1] == 'j' && rd[p] == 't';
            uint32_t count = le32(&rd[p + 4]);
            p += 8;
            for (uint32_t i = 0; i < count; i++) {
                if (p + (version == 0 ? 8 : 10) > rd.size()) { ok = false; break; }
                uint32_t off = le32(&rd[p]);
                uint32_t id = version == 0 ? (uint32_t)(rd[p + 4] | (rd[p + 5] << 8)) : le32(&rd[p + 4]);
                p += version == 0 ? 8 : 10;
                if (version == 0) { // name: zero-terminated, padded to an even length
                    size_t z = p;
                    while (z < rd.size() && rd[z] != 0) z++;
                    if (z >= rd.size()) { ok = false; break; }
                    size_t len = z - p + 1;
                    p += (len + 1) & ~(size_t)1;
                } else { // version 1: Pascal string
                    if (p >= rd.size()) { ok = false; break; }
                    p += 1 + rd[p];
                }
                if (isObjt && id == 0) { objtOff = off; break; }
            }
        }
        if (!ok || objtOff < 64) break;
        std::vector<uint8_t> ch;
        if (!readAt(objtOff, 76, ch) || std::memcmp(ch.data(), "objt", 4) != 0) break;
        uint32_t size = be32(&ch[4]);
        if (size < 76 || ((uint32_t)ch[8] << 8 | ch[9]) != 0) break; // must be objt #0
        if (!readAt(objtOff + 76, size - 76, out)) { out.clear(); break; }
        result = 1;
    } while (false);
    std::fclose(f);
    return result;
}

/// objt entries of the other house files of the folder (the game wrote those, so their unknown fields are right),
/// for types the house did not have. Read once per save, on the first new type; only the objt chunk is kept.
struct OtherHouseTypes {
    bool Loaded = false;
    std::map<uint32_t, OBJTRaw::Entry> ByGUID;
    void Load(const std::string& housesDir, const std::function<void(int)>& progress = nullptr) {
        Loaded = true;
        for (int h = 0; h < 100; h++) {
            if (progress) progress(h); // ADDED(sims) save progress: ~100 house files
            std::string path = housesDir + "/House" + Pad2(h) + ".iff";
            std::vector<uint8_t> objtData;
            int got = ReadObjtChunk(path, objtData); // ADDED(sims): only the objt chunk (was: the whole file)
            if (got < 0) continue;                    // no such house
            if (got == 0) {                           // no usable resource map: the whole file, as before
                std::vector<uint8_t> data;
                if (!st::ReadAllBytes(path, data)) continue;
                IffRawFile f;
                if (!f.Parse(data)) continue;
                auto c = f.Find("objt", 0);
                if (!c) continue;
                objtData = c->Data;
            }
            OBJTRaw t;
            if (!t.Parse(objtData)) continue;
            for (auto& e : t.Entries) ByGUID.emplace(e.GUID, e);
        }
    }
};

// ------------------------------------------------------------------ original records

struct Decoded {
    bool OK = false;
    OBJMInstance Inst;
    std::string Unhandled;
    uint16_t TypeID = 0;
};

const OBJD* ContentOBJD(uint32_t guid) {
    auto content = FSO::Content::Content::Get();
    if (content == nullptr) return nullptr;
    auto res = content->WorldObjects->Get(guid, false);
    return res ? res->OBJ : nullptr;
}

int16_t DirIndexOf(VMEntity* e) {
    if (auto ava = dynamic_cast<VMAvatar*>(e)) {
        // FreeSO ConvertAvatar: RadianDirection = ObjectData[Direction] * PI/4
        int d = (int)std::lround(ava->RadianDirection() / (M_PI / 4.0));
        return (int16_t)(((d % 8) + 8) % 8);
    }
    return e->GetValue(VMStackObjectVariable::Direction);
}

VMEntity* RootContainer(VMEntity* e) {
    int guard = 0;
    while (e->Container != nullptr && guard++ < 32) e = e->Container;
    return e;
}

struct TileKey {
    int8_t Level; int16_t X, Y;
    bool operator<(const TileKey& o) const { return std::tie(Level, Y, X) < std::tie(o.Level, o.Y, o.X); }
};

// ------------------------------------------------------------------ architecture (the loader's decode, re-run)

std::unordered_map<uint8_t, uint16_t> BuildDict(const std::vector<std::pair<std::string, uint8_t>>& entries, bool floors) {
    auto content = FSO::Content::Content::Get();
    std::unordered_map<uint8_t, uint16_t> result;
    for (auto& en : entries) {
        std::string name = en.first;
        size_t dot = name.find('.');
        name = FSO::Content::ToLowerInvariant(dot == std::string::npos ? name : name.substr(0, dot));
        if (floors) {
            auto& c = content->WorldFloors->DynamicFloorFromID;
            auto it = c.find(name);
            if (it != c.end()) result[en.second] = it->second;
        } else {
            auto& c = content->WorldWalls->DynamicWallFromID;
            auto it = c.find(name);
            if (it != c.end()) result[en.second] = it->second;
        }
    }
    return result;
}

/// FLRm/WALm payload: header (pad, version, magic), count, entries (name, pad, int32, id byte, 5 + 2*chunkid bytes).
struct WALmRaw {
    std::vector<uint8_t> Header;
    struct Entry { std::string Name; uint8_t ID = 0; std::vector<uint8_t> Raw; };
    std::vector<Entry> Entries;
    std::vector<uint8_t> Tail;
    uint16_t ChunkID = 0;
    bool Parse(const std::vector<uint8_t>& d, uint16_t chunkId) {
        ChunkID = chunkId;
        if (d.size() < 16) return false;
        Header.assign(d.begin(), d.begin() + 12);
        uint32_t count = LE32(d.data() + 12);
        size_t p = 16;
        for (uint32_t i = 0; i < count; i++) {
            size_t s = p;
            Entry e;
            while (p < d.size() && d[p] != 0) e.Name += (char)d[p++];
            p++;
            if (e.Name.size() % 2 == 0) p++;
            p += 4;
            if (p >= d.size()) return false;
            e.ID = d[p++];
            p += 5 + chunkId * 2;
            if (p > d.size()) return false;
            e.Raw.assign(d.begin() + (long)s, d.begin() + (long)p);
            Entries.push_back(std::move(e));
        }
        Tail.assign(d.begin() + (long)p, d.end());
        return true;
    }
    void Add(const std::string& name, uint8_t id) {
        Entry e; e.Name = name; e.ID = id;
        auto& o = e.Raw;
        for (char c : name) o.push_back((uint8_t)c);
        o.push_back(0);
        if (name.size() % 2 == 0) o.push_back(0xA3);
        Put32(o, 1);
        o.push_back(id);
        // shipped entries: chunk 0 "01 00 00 00 00", chunk 1 "01 01 00 00 00 00 00"
        o.push_back(1);
        for (int k = 0; k < 4 + ChunkID * 2; k++) o.push_back((ChunkID == 1 && k == 0) ? 1 : 0);
        Entries.push_back(std::move(e));
    }
    std::vector<uint8_t> Write() const {
        std::vector<uint8_t> o(Header);
        Put32(o, (uint32_t)Entries.size());
        for (auto& e : Entries) o.insert(o.end(), e.Raw.begin(), e.Raw.end());
        o.insert(o.end(), Tail.begin(), Tail.end());
        return o;
    }
};

bool DecodeARRY(IffRawFile& f, uint16_t id, ARRY& out) {
    auto c = f.Find("Arry", id);
    if (!c) return false;
    st::Reader r(c->Data, st::Endian::Little);
    out.ChunkID = id;
    out.Read(nullptr, r);
    return true;
}

} // namespace

// ================================================================== VMTS1LotSaver

std::string VMTS1LotSaver::UserHousePath(int16_t houseNumber) {
    auto content = FSO::Content::Content::Get();
    if (content == nullptr || !content->Neighborhood) return std::string();
    return content->Neighborhood->UserPath + "/Houses/House" + Pad2(houseNumber) + ".iff";
}

void VMTS1LotSaver::ThumbnailSizeOf(const std::string& housePath, int32_t& width, int32_t& height, int32_t* ax, int32_t* ay) {
    width = 264; height = 144;
    if (ax) *ax = width / 2;
    if (ay) *ay = height * 6 / 10;
    IffRawFile f;
    if (!f.Load(housePath)) return;
    // FIX(sims) FB1: THMB 512 = (x, y) of the lot's centre in the picture, base y offset, x offset, roof y offset (FreeSO THMB)
    if (auto t = f.Find("THMB", 512)) if (t->Data.size() >= 8) {
        int32_t tx = (int32_t)LE32(t->Data.data()), ty = (int32_t)LE32(t->Data.data() + 4);
        if (ax && tx > 0 && tx < 1024) *ax = tx;
        if (ay && ty > 0 && ty < 1024) *ay = ty;
    }
    auto c = f.Find("BMP_", 512);
    if (!c || c->Data.size() < 26 || c->Data[0] != 'B' || c->Data[1] != 'M') return;
    int32_t w = (int32_t)LE32(c->Data.data() + 18), h = (int32_t)LE32(c->Data.data() + 22);
    if (w > 0 && w <= 1024 && h != 0 && std::abs(h) <= 1024) { width = w; height = std::abs(h); }
    if (!f.Find("THMB", 512)) { if (ax) *ax = width / 2; if (ay) *ay = height * 6 / 10; } // FIX(sims) FB1
}

bool VMTS1LotSaver::SaveHouse(const std::string& sourcePath, const std::string& destPath) {
    std::vector<uint8_t> out;
    if (!BuildHouse(sourcePath, out)) return false;
    IffRawFile f;
    std::string err;
    if (!f.Parse(out, &err)) { Error = "internal: " + err; return false; }
    FSO::Files::LoadProgress::Progress("save: house file", 99, 100); // ADDED(sims) save progress: writing the file
    if (!f.Save(destPath, &err)) { Error = err; return false; }
    FSO::Files::LoadProgress::Progress("save: house file", 100, 100); // ADDED(sims)
    Log.push_back("wrote " + destPath + " (" + std::to_string(out.size()) + " bytes)");
    return true;
}

bool VMTS1LotSaver::BuildHouse(const std::string& sourcePath, std::vector<uint8_t>& out) {
    Error.clear();
    LastStats = Stats();
    // ADDED(sims) save progress (files/LoadProgress.h, the app's "Saving..." overlay): one scale for the whole house
    // file: 4 units for loading the source, one per original record decoded and per entity written, 100 for the
    // other houses' type tables (read on the first new type only), 4 for building the file. Reported often enough
    // that no stretch between two reports takes more than ~0.1 s on the 3DS.
    size_t pDone = 0, pTotal = 0;
    auto progress = [&](size_t units) { pDone += units; FSO::Files::LoadProgress::Progress("save: house file", std::min(pDone, pTotal), pTotal); };
    IffRawFile file;
    std::string err;
    if (!file.Load(sourcePath, &err)) { Error = err; return false; }
    auto ctx = VM->Context;
    auto arch = ctx->Architecture();
    if (arch == nullptr) { Error = "no lot loaded"; return false; }

    // ---------------------------------------------------------------- original object table and records
    auto objtChunk = file.Find("objt", 0);
    auto objmChunk = file.Find("ObjM", 1);
    if (!objtChunk || !objmChunk) { Error = "house file has no objt#0/ObjM#1"; return false; }
    OBJTRaw objt;
    if (!objt.Parse(objtChunk->Data)) { Error = "cannot parse objt"; return false; }
    OBJMRawChunk objm;
    if (!objm.Parse(objmChunk->Data, err)) { Error = "cannot parse ObjM: " + err; return false; }

    // OBJTEntry objects for the field decoder (OBJDType decides portal/person records)
    std::map<uint16_t, OBJTEntry*> typeEntries;
    std::vector<std::unique_ptr<OBJTEntry>> ownedEntries;
    auto entryFor = [&](uint16_t typeId) -> OBJTEntry* {
        auto it = typeEntries.find(typeId);
        if (it != typeEntries.end()) return it->second;
        auto raw = objt.ByType(typeId);
        if (!raw) return nullptr;
        std::vector<uint8_t> bytes(raw->Raw);
        bytes.resize(bytes.size() + 8, 0);
        st::Reader r(bytes, st::Endian::Little);
        ownedEntries.emplace_back(new OBJTEntry(r, objt.Version));
        typeEntries[typeId] = ownedEntries.back().get();
        return ownedEntries.back().get();
    };

    std::map<int16_t, Decoded> orig; // by object id
    std::vector<int16_t> origOrder;
    pTotal = 4 + objm.Records.size() + VM->Entities.size() + 100 + 4; // ADDED(sims) save progress
    progress(4);
    for (size_t i = 0; i < objm.Records.size(); i++) {
        if ((i & 7) == 0) progress(i == 0 ? 0 : 8); // ADDED(sims)
        auto& rec = objm.Records[i];
        Decoded d;
        d.TypeID = objm.Table[i].second;
        try {
            st::Reader io(rec.Data.data(), rec.Data.size(), st::Endian::Little);
            IffFieldEncode iop(&io, (rec.Offset & 1) == 1);
            d.Inst.Read(objm.Version, iop, [&](int16_t objID) {
                OBJMResource r;
                uint16_t type = 0;
                for (auto& p : objm.Table) if (p.first == (uint16_t)objID) { type = p.second; break; }
                r.OBJT = entryFor(type);
                if (r.OBJT) r.OBJD = const_cast<OBJD*>(ContentOBJD(r.OBJT->GUID));
                return r;
            });
            std::string bits = iop.BitDebugTil((int64_t)rec.Data.size());
            for (char c : bits) if (c == '0' || c == '1') d.Unhandled += c;
            // trailing zero bits that only pad the record's last byte are not data
            while (!d.Unhandled.empty() && d.Unhandled.size() < 8 && d.Unhandled.find('1') == std::string::npos) d.Unhandled.clear();
            d.OK = true;
        } catch (std::exception& e) {
            Log.push_back(std::string("record ") + std::to_string(objm.Table[i].first) + " not decoded: " + e.what());
        }
        orig[(int16_t)objm.Table[i].first] = std::move(d);
        origOrder.push_back((int16_t)objm.Table[i].first);
    }
    // templates by GUID (any decoded record of that type)
    std::map<uint32_t, const Decoded*> templByGUID;
    for (auto& kv : orig) if (kv.second.OK && kv.second.Inst.OBJT) templByGUID.emplace(kv.second.Inst.OBJT->GUID, &kv.second);
    const Decoded* personTemplate = nullptr;
    for (auto& kv : orig) if (kv.second.OK && kv.second.Inst.HasPersonData) { personTemplate = &kv.second; break; }

    // ---------------------------------------------------------------- entities to save
    std::vector<VMEntity*> ents;
    for (auto e : VM->Entities) {
        if (e == nullptr || e->Dead || e->GhostImage || e->Object == nullptr || e->Object->OBJ == nullptr) continue;
        ents.push_back(e);
    }
    std::set<int16_t> liveIds;
    for (auto e : ents) liveIds.insert(e->ObjectID);
    pDone = 4 + objm.Records.size(); // ADDED(sims) save progress
    pTotal = pDone + ents.size() + 100 + 4;

    // ---------------------------------------------------------------- object types
    std::string housesDir = sourcePath.substr(0, sourcePath.find_last_of("/\\"));
    OtherHouseTypes others;
    uint16_t nextType = 1;
    for (auto& e : objt.Entries) nextType = std::max<uint16_t>(nextType, (uint16_t)(e.TypeID + 1));
    auto typeOf = [&](uint32_t guid, const OBJD* objd) -> uint16_t {
        if (auto e = objt.ByGUID(guid)) return e->TypeID;
        OBJTRaw::Entry en;
        if (!others.Loaded) others.Load(housesDir, [&](int h) { progress(h == 0 ? 0 : 1); }); // ADDED(sims) progress
        auto oit2 = others.ByGUID.find(guid);
        bool found = oit2 != others.ByGUID.end();
        if (found) en = oit2->second;
        if (!found) {
            en = OBJTRaw::Entry();
            en.GUID = guid;
            if (objd) {
                en.OBJDType = (uint16_t)objd->ObjectType;
                // single tile objects are named, multitile parts are not (OBJT.cs)
                if (!objd->IsMultiTile()) en.Name = objd->ChunkLabel;
                auto content = FSO::Content::Content::Get();
                auto res = content->WorldObjects->Get(guid, false);
                if (res && res->Resource) {
                    if (auto b = res->Resource->Get<BHAV>(objd->BHAV_Init)) en.InitTreeVersion = b->Flags;
                    if (auto b = res->Resource->Get<BHAV>(objd->BHAV_MainID)) en.MainTreeVersion = b->Flags;
                }
            }
        }
        en.TypeID = nextType++;
        en.Raw = OBJTRaw::Encode(en, objt.Version);
        objt.Entries.push_back(en);
        LastStats.NewTypes++;
        if (getenv("SAVER_DEBUG")) Log.push_back("new object type " + std::to_string(en.TypeID) + " '" + en.Name + "'" + (found ? " (fields from another house)" : ""));
        return en.TypeID;
    };

    // ---------------------------------------------------------------- tile lists (ARRY#3/#103 + OBJM linked slots)
    // Original chains: head from the object grid, then each record's LinkedSlot.
    std::map<int16_t, std::pair<TileKey, int>> origTile; // object id -> (tile, rank in the tile's list)
    {
        for (int lvl = 0; lvl < 2; lvl++) {
            ARRY a;
            if (!DecodeARRY(file, lvl == 0 ? 3 : 103, a)) continue;
            auto grid = a.TransposeData();
            for (size_t i = 0; i + 1 < grid.size(); i += 2) {
                int16_t head = (int16_t)(grid[i] | (grid[i + 1] << 8));
                if (head == 0) continue;
                size_t idx = i / 2;
                TileKey k{ (int8_t)(lvl + 1), (int16_t)(idx % 64), (int16_t)(idx / 64) };
                int rank = 0, guard = 0;
                int16_t cur = head;
                while (cur != 0 && guard++ < 256) {
                    origTile[cur] = std::make_pair(k, rank++);
                    auto it = orig.find(cur);
                    if (it == orig.end() || !it->second.OK) break;
                    cur = it->second.Inst.LinkedSlot.ObjectID;
                }
            }
        }
    }
    // Saved position for objects the VM holds out of the world although the house had them on the lot: the loader
    // could not place them (two objects on one tile the port's placement rejects, e.g. a flower bed under a rose
    // bush in House07). Nothing in TS1 takes a lot object out of the world without deleting it (sold objects are
    // gone from the VM), so these keep their saved position instead of being written as lost.
    std::unordered_map<int16_t, LotTilePos> keptPos;
    for (auto e : ents) {
        if (e->Container != nullptr || dynamic_cast<VMAvatar*>(e) != nullptr) continue;
        if (e->Position() != LotTilePos::OUT_OF_WORLD) continue;
        auto oit = orig.find(e->ObjectID);
        if (oit == orig.end() || !oit->second.OK || !oit->second.Inst.OBJT || oit->second.Inst.OBJT->GUID != e->Object->OBJ->GUID) continue;
        auto& bi = oit->second.Inst;
        if (bi.X == -16 && bi.Y == -16) continue;
        if (bi.ObjectData.size() > 2 && bi.ObjectData[2] != 0) continue; // was contained
        keptPos[e->ObjectID] = LotTilePos((int16_t)bi.X, (int16_t)bi.Y, (int8_t)bi.Level);
    }
    if (!keptPos.empty()) Log.push_back(std::to_string(keptPos.size()) + " objects the loader could not place keep their saved position");
    auto effPos = [&](VMEntity* e) -> LotTilePos {
        auto it = keptPos.find(e->ObjectID);
        return it != keptPos.end() ? it->second : e->Position();
    };
    std::map<TileKey, std::vector<VMEntity*>> tiles;
    for (auto e : ents) {
        if (e->Container != nullptr) continue;
        auto pos = effPos(e);
        if (pos == LotTilePos::OUT_OF_WORLD || pos.x < 0 || pos.y < 0) continue;
        TileKey k{ pos.Level, (int16_t)(pos.x / 16), (int16_t)(pos.y / 16) };
        tiles[k].push_back(e);
    }
    std::unordered_map<int16_t, int16_t> nextOnTile, prevOnTile;
    for (auto& kv : tiles) {
        auto& list = kv.second;
        auto rankOf = [&](VMEntity* e) -> int {
            auto it = origTile.find(e->ObjectID);
            auto oit = orig.find(e->ObjectID);
            bool same = it != origTile.end() && !(it->second.first < kv.first) && !(kv.first < it->second.first) &&
                        oit != orig.end() && oit->second.OK && oit->second.Inst.OBJT && oit->second.Inst.OBJT->GUID == e->Object->OBJ->GUID;
            return same ? it->second.second : -1;
        };
        std::stable_sort(list.begin(), list.end(), [&](VMEntity* a, VMEntity* b) {
            int ra = rankOf(a), rb = rankOf(b);
            if ((ra < 0) != (rb < 0)) return ra < 0; // objects that arrived on the tile since go first (TS1 prepends)
            if (ra < 0) return a->ObjectID > b->ObjectID;
            return ra < rb;
        });
        for (size_t i = 0; i < list.size(); i++) {
            nextOnTile[list[i]->ObjectID] = (i + 1 < list.size()) ? list[i + 1]->ObjectID : 0;
            prevOnTile[list[i]->ObjectID] = (i > 0) ? list[i - 1]->ObjectID : 0;
        }
    }

    // ---------------------------------------------------------------- records
    std::map<int16_t, OBJMRecordOut> records;
    size_t recordsDone = 0; // ADDED(sims) save progress
    for (auto e : ents) {
        if ((++recordsDone & 7) == 0) progress(8); // ADDED(sims)
        const OBJD* objd = e->Object->OBJ;
        uint32_t guid = objd->GUID;
        uint16_t typeId = typeOf(guid, objd);
        auto oit = orig.find(e->ObjectID);
        const Decoded* base = (oit != orig.end() && oit->second.OK && oit->second.Inst.OBJT && oit->second.Inst.OBJT->GUID == guid) ? &oit->second : nullptr;
        const Decoded* templ = base;
        if (!templ) { auto t = templByGUID.find(guid); if (t != templByGUID.end()) templ = t->second; }
        auto ava = dynamic_cast<VMAvatar*>(e);

        OBJMRecordOut rec;
        rec.ObjectID = (uint16_t)e->ObjectID;
        rec.TypeID = typeId;
        OBJMInstance& inst = rec.Instance;
        if (templ) { inst = templ->Inst; rec.UnhandledBits = templ->Unhandled; }
        else {
            inst = OBJMInstance();
            inst.UnknownData = 1;
            inst.StackFlags = 512; //512 usually
            inst.ObjectDataExtra.assign(OBJMInstance::ExtraDataCount, 0);
            inst.Footprint = OBJMFootprint();
        }
        if (base) LastStats.Kept++; else LastStats.Added++;
        inst.OBJD = const_cast<OBJD*>(objd);
        inst.OBJT = nullptr; // set after the objt table is final (pointer into ownedEntries below)

        // position (FreeSO: Position = X,Y,Level or OUT_OF_WORLD for -16,-16); contained objects carry the root
        // container's position, as in the shipped houses (a lamp on a table has the table's X/Y).
        VMEntity* root = RootContainer(e);
        LotTilePos pos = effPos(root);
        int16_t dir = DirIndexOf(e);
        if (keptPos.count(e->ObjectID) && base && base->Inst.ObjectData.size() > 1) dir = base->Inst.ObjectData[1];
        if (pos == LotTilePos::OUT_OF_WORLD) {
            inst.X = -16; inst.Y = -16; inst.Level = 1;
            inst.Footprint.MinY = -24; inst.Footprint.MinX = -24; inst.Footprint.MaxY = -8; inst.Footprint.MaxX = -8;
        } else {
            bool unmoved = base && base->Inst.X == pos.x && base->Inst.Y == pos.y && base->Inst.Level == pos.Level &&
                           base->Inst.ObjectData.size() > 1 && base->Inst.ObjectData[1] == dir;
            inst.X = pos.x; inst.Y = pos.y; inst.Level = std::max<int32_t>(1, pos.Level);
            // the loader places objects at their tile's centre (LotTilePos::FromBigTile); TS1 keeps exact sub-tile
            // positions for some (call markers on tile edges): the saved position stays while the tile is the same
            if (!ava && base && base->Inst.X != -16 && base->Inst.X / 16 == pos.x / 16 && base->Inst.Y / 16 == pos.y / 16 && base->Inst.Level == pos.Level) {
                inst.X = base->Inst.X; inst.Y = base->Inst.Y;
                unmoved = base->Inst.ObjectData.size() > 1 && base->Inst.ObjectData[1] == dir;
            }
            if (!unmoved) {
                if (e->Footprint && e->Container == nullptr) {
                    inst.Footprint.MinX = e->Footprint->x1; inst.Footprint.MinY = e->Footprint->y1;
                    inst.Footprint.MaxX = e->Footprint->x2; inst.Footprint.MaxY = e->Footprint->y2;
                } else if (templ && templ->Inst.X != -16) {
                    // the template's footprint shape around the new position (rotated by quarter turns)
                    int ex0 = templ->Inst.X - templ->Inst.Footprint.MinX, ex1 = templ->Inst.Footprint.MaxX - templ->Inst.X;
                    int ey0 = templ->Inst.Y - templ->Inst.Footprint.MinY, ey1 = templ->Inst.Footprint.MaxY - templ->Inst.Y;
                    int turns = (((dir - (templ->Inst.ObjectData.size() > 1 ? templ->Inst.ObjectData[1] : 0)) / 2) % 4 + 4) % 4;
                    for (int t = 0; t < turns; t++) { int nx0 = ey1, nx1 = ey0, ny0 = ex0, ny1 = ex1; ex0 = nx0; ex1 = nx1; ey0 = ny0; ey1 = ny1; }
                    inst.Footprint.MinX = pos.x - ex0; inst.Footprint.MaxX = pos.x + ex1;
                    inst.Footprint.MinY = pos.y - ey0; inst.Footprint.MaxY = pos.y + ey1;
                } else {
                    inst.Footprint.MinX = pos.x - 8; inst.Footprint.MaxX = pos.x + 8;
                    inst.Footprint.MinY = pos.y - 8; inst.Footprint.MaxY = pos.y + 8;
                }
            }
        }

        // attributes / temps / object data (FreeSO: ent.Attributes, thread.TempRegisters (TS1 has 8), ent.ObjectData)
        {
            // TS1 attribute arrays grow on demand (VMEntity::GetAttribute); the record keeps at least the OBJD's count
            // and the saved one (trailing zeros included)
            auto& at = e->AttributeData();
            size_t n = base ? std::max(at.size(), base->Inst.Attributes.size()) : std::max<size_t>(at.size(), objd->NumAttributes);
            inst.Attributes.assign(n, 0);
            for (size_t i = 0; i < at.size(); i++) inst.Attributes[i] = at[i];
        }
        inst.TempRegisters.assign(OBJMInstance::TempCount, 0);
        if (e->Thread) for (int i = 0; i < OBJMInstance::TempCount; i++) inst.TempRegisters[i] = e->Thread->TempRegisters[(size_t)i];
        inst.ObjectData.assign(OBJMInstance::ObjectDataCount, 0);
        for (int i = 0; i < OBJMInstance::ObjectDataCount && i < (int)e->ObjectData.size(); i++) inst.ObjectData[i] = e->ObjectData[(size_t)i];
        inst.ObjectData[(int)VMStackObjectVariable::Direction] = dir;
        inst.ObjectData[(int)VMStackObjectVariable::ObjectId] = e->ObjectID;
        inst.ObjectData[(int)VMStackObjectVariable::ContainerId] = e->Container ? e->Container->ObjectID : 0;
        inst.ObjectData[(int)VMStackObjectVariable::SlotNumber] = e->Container ? e->ContainerSlot : (int16_t)-1;
        // ParentId: the previous object in the tile's list, or the container (shipped houses)
        inst.ObjectData[(int)VMStackObjectVariable::ParentId] = e->Container ? e->Container->ObjectID
            : (prevOnTile.count(e->ObjectID) ? prevOnTile[e->ObjectID] : (int16_t)0);
        if (inst.ObjectDataExtra.size() != (size_t)OBJMInstance::ExtraDataCount) inst.ObjectDataExtra.assign(OBJMInstance::ExtraDataCount, 0);
        {
            // Object data the engine computes, or that this port initialises differently from TS1 (measured with
            // `simsave diffobjm` on an unmodified load + save): kept from the saved record while the object has not
            // moved; for new objects the VM's value (flags: without the port-only bits unless the type's saved record
            // has them).
            bool sameSpot = base && base->Inst.X == inst.X && base->Inst.Y == inst.Y && base->Inst.Level == inst.Level &&
                            base->Inst.ObjectData.size() > 1 && base->Inst.ObjectData[1] == dir;
            const uint16_t portFlagBits = 0x1 | 0x10 | 0x100 | 0x400 | 0x2000; // ShowGhost, AllowPersonIntersection, ChairFacing, HideForCutaway, NeedsMaintinance
            int16_t vmFlags = inst.ObjectData[(int)VMStackObjectVariable::Flags];
            if (base) {
                int16_t bf = base->Inst.ObjectData[(int)VMStackObjectVariable::Flags];
                inst.ObjectData[(int)VMStackObjectVariable::Flags] = (int16_t)((bf & portFlagBits) | (vmFlags & ~portFlagBits));
                inst.ObjectData[(int)VMStackObjectVariable::FlagField2] = base->Inst.ObjectData[(int)VMStackObjectVariable::FlagField2];
                inst.ObjectData[(int)VMStackObjectVariable::SlotCount] = base->Inst.ObjectData[(int)VMStackObjectVariable::SlotCount];
                if (sameSpot) {
                    inst.ObjectData[(int)VMStackObjectVariable::WallAdjacencyFlags] = base->Inst.ObjectData[(int)VMStackObjectVariable::WallAdjacencyFlags];
                    inst.ObjectData[(int)VMStackObjectVariable::Room] = base->Inst.ObjectData[(int)VMStackObjectVariable::Room];
                }
            } else {
                int16_t tf = (templ && templ->Inst.ObjectData.size() > 8) ? templ->Inst.ObjectData[8] : (int16_t)0;
                inst.ObjectData[(int)VMStackObjectVariable::Flags] = (int16_t)((tf & portFlagBits) | (vmFlags & ~portFlagBits));
                inst.ObjectData[(int)VMStackObjectVariable::SlotCount] = (int16_t)e->TotalSlots();
            }
        }

        // ---- stack (inverse of FreeSO ConvertThread): BHAV frames only; routing frames have no TS1 record (the
        // routing primitive below them reruns, FreeSO "SpecialResult.Retry").
        inst.Stack.clear();
        std::vector<FSO::SimAntics::Engine::VMStackFrame*> frames;
        if (e->Thread) for (auto& f : e->Thread->Stack) if (f && !f->IsRoutingFrame() && f->Routine) frames.push_back(f.get());
        for (auto f : frames) {
            OBJMStackFrame sf;
            sf.StackObjectID = f->StackObjectID();
            sf.TreeID = (int16_t)f->Routine->ID;
            sf.NodeID = (int16_t)f->InstructionPointer;
            size_t np = f->Routine->Arguments, nl = f->Routine->Locals;
            if (f->Routine->Chunk) { np = f->Routine->Chunk->Args; nl = f->Routine->Chunk->Locals; }
            sf.Parameters.assign(np, 0);
            for (size_t k = 0; k < np && k < f->Args.size(); k++) sf.Parameters[k] = f->Args[k];
            sf.Locals.assign(nl, 0);
            for (size_t k = 0; k < nl && k < f->Locals.size(); k++) sf.Locals[k] = f->Locals[k];
            sf.PrimitiveState = 0;
            // FreeSO: CodeOwnerGUID = objt.Entries[frame.CodeOwnerObjType - 1].GUID
            const OBJD* owner = (f->CodeOwner && f->CodeOwner->OBJ) ? f->CodeOwner->OBJ : objd;
            sf.CodeOwnerObjType = (int16_t)typeOf(owner->GUID, owner);
            inst.Stack.push_back(sf);
        }
        LastStats.StackFrames += (int)inst.Stack.size();

        // ---- relationships (FreeSO: MeToObject = Relationships{TargetID, Values}); original order kept
        {
            std::vector<OBJMRelationshipEntry> rels;
            std::set<uint16_t> done;
            if (base) for (auto& r : base->Inst.Relationships) {
                auto it = e->MeToObject.find((uint16_t)r.TargetID);
                if (it == e->MeToObject.end()) continue;
                OBJMRelationshipEntry n = r;
                n.Values.assign(it->second.begin(), it->second.end());
                rels.push_back(n);
                done.insert((uint16_t)r.TargetID);
            }
            std::vector<uint16_t> rest;
            for (auto& kv : e->MeToObject) if (!done.count(kv.first)) rest.push_back(kv.first);
            std::sort(rest.begin(), rest.end());
            for (auto t : rest) {
                OBJMRelationshipEntry n;
                n.IsPresent = 1; n.TargetID = t;
                auto& v = e->MeToObject[t];
                n.Values.assign(v.begin(), v.end());
                rels.push_back(n);
            }
            inst.Relationships = rels;
        }

        // ---- tile list and container slots (FreeSO: ent.Contained = Slots[].ObjectID)
        inst.LinkedSlot.Unknown = 0;
        inst.LinkedSlot.ObjectID = (e->Container == nullptr && nextOnTile.count(e->ObjectID)) ? nextOnTile[e->ObjectID] : 0;
        {
            size_t count = base ? base->Inst.Slots.size() : (size_t)std::max<int32_t>(0, e->TotalSlots());
            if (count < (size_t)std::max<int32_t>(0, e->TotalSlots()) && base) count = base->Inst.Slots.size();
            std::vector<OBJMSlot> slots(count);
            for (size_t i = 0; i < count; i++) {
                if (base && i < base->Inst.Slots.size()) slots[i].Unknown = base->Inst.Slots[i].Unknown;
                VMEntity* in = (int)i < e->TotalSlots() ? e->GetSlot((int32_t)i) : nullptr;
                slots[i].ObjectID = in ? in->ObjectID : 0;
                if (in) LastStats.Contained++;
            }
            inst.Slots = slots;
        }

        // ---- dynamic sprite flags (FreeSO: DynamicSpriteFlags bit i <- record flag i != 0)
        {
            size_t count = base ? base->Inst.DynamicSpriteFlags.size() : (size_t)objd->NumDynamicSprites;
            std::vector<int16_t> dsf(count, 0);
            for (size_t i = 0; i < count; i++) {
                bool on = (i < 64) ? ((e->DynamicSpriteFlags >> i) & 1) != 0 : (i < 128 ? ((e->DynamicSpriteFlags2 >> (i - 64)) & 1) != 0 : false);
                int16_t old = (base && i < base->Inst.DynamicSpriteFlags.size()) ? base->Inst.DynamicSpriteFlags[i] : (int16_t)0;
                dsf[i] = on ? (old != 0 ? old : (int16_t)1) : (int16_t)0;
            }
            inst.DynamicSpriteFlags = dsf;
        }

        // ---- multitile (FreeSO: group offsets = FromBigTile(GroupX, GroupY, GroupLevel); lead first)
        inst.HasMultitileData = objd->IsMultiTile();
        if (inst.HasMultitileData) {
            auto group = e->MultitileGroup;
            if (group && !group->Objects.empty()) {
                VMEntity* lead = nullptr;
                // keep the saved lead when the group is the one that was saved
                if (base && base->Inst.HasMultitileData) {
                    int16_t savedLead = base->Inst.MultitileData.MultitileParentID == 0 ? e->ObjectID : base->Inst.MultitileData.MultitileParentID;
                    for (auto o : group->Objects) if (o->ObjectID == savedLead) lead = o;
                }
                if (!lead) lead = group->BaseObject();
                size_t idx = 0;
                for (size_t i = 0; i < group->Objects.size(); i++) if (group->Objects[i] == e) idx = i;
                LotTilePos off = idx < group->Offsets.size() ? group->Offsets[idx] : LotTilePos();
                auto fdiv = [](int v) { return (int)std::floor(v / 16.0); };
                inst.MultitileData.GroupX = fdiv(off.x);
                inst.MultitileData.GroupY = fdiv(off.y);
                inst.MultitileData.GroupLevel = off.Level;
                inst.MultitileData.MultitileParentID = (lead == e) ? 0 : lead->ObjectID;
                LotTilePos lp = RootContainer(lead)->Position();
                if (pos != LotTilePos::OUT_OF_WORLD && lp != LotTilePos::OUT_OF_WORLD) {
                    inst.MultitileData.OffsetX = fdiv(pos.x) - fdiv(lp.x);
                    inst.MultitileData.OffsetY = fdiv(pos.y) - fdiv(lp.y);
                    inst.MultitileData.OffsetLevel = pos.Level - lp.Level;
                } else if (!templ) {
                    inst.MultitileData.OffsetX = inst.MultitileData.GroupX;
                    inst.MultitileData.OffsetY = inst.MultitileData.GroupY;
                    inst.MultitileData.OffsetLevel = 0;
                }
            }
        }

        // ---- portal data (8 floats in every shipped portal record: 10001)
        {
            auto te = objt.ByType(typeId);
            bool portal = te ? te->OBJDType == (uint16_t)OBJDType::Portal : objd->ObjectType == OBJDType::Portal;
            // shipped records: doors carry 8 floats (10001), windows none; a type without a saved record of its own
            // gets the door form when its OBJD has a portal tree
            if (portal) { if (!templ) inst.PortalData.assign(objd->BHAV_Portal != 0 ? 8 : 0, 10001.0f); }
            else inst.PortalData.clear();
        }

        // ---- persons (inverse of FreeSO ConvertAvatar / ConvertThread's queue part)
        inst.HasPersonData = (ava != nullptr);
        if (ava) {
            LastStats.Persons++;
            OBJMPerson& p = inst.PersonData;
            if (!(templ && templ->Inst.HasPersonData)) {
                p = personTemplate ? personTemplate->Inst.PersonData : OBJMPerson();
                auto& ap = ava->TS1Appearance;
                p.Body = ap.Body.Mesh; p.BodyTex = ap.Body.Binding.empty() ? ap.Body.Texture : ap.Body.Binding + "=" + ap.Body.Texture;
                p.Head = ap.Head.Mesh; p.HeadTex = ap.Head.Binding.empty() ? ap.Head.Texture : ap.Head.Binding + "=" + ap.Head.Texture;
                p.LeftHand = ap.Hands[0].Mesh; p.LeftHandTex = ap.Hands[0].Binding + (ap.Hands[0].Texture.empty() ? "" : "=" + ap.Hands[0].Texture);
                p.RightHand = ap.Hands[1].Mesh; p.RightHandTex = ap.Hands[1].Binding + (ap.Hands[1].Texture.empty() ? "" : "=" + ap.Hands[1].Texture);
                p.Unk1.clear(); p.Unk2.clear();
                p.Accessories.clear();
                bool child = ap.AgeType == "child";
                p.Animation.clear(); p.CarryAnimation.clear();
                p.BaseAnimation = child ? "c2o-standing-loop;-10;1000;0;1000;1;1;1" : "a2o-standing-loop;-10;1000;0;1000;1;1;1";
                p.AnimEventCount = 0; p.Engaged = 0; p.RoutingState = OBJMRoutingState::None; p.RoutingFrameCount = 0;
                p.LastInteraction = OBJMInteraction();
            }
            // motives: keep the saved float when the VM value is its rounding (FreeSO: (short)Math.Round(motive))
            for (int i = 0; i < 16; i++) {
                int16_t v = ava->GetMotiveData((VMMotive)i);
                if (std::lround(p.MotiveData[i]) != v) {
                    p.MotiveDataOld[i] = p.MotiveData[i];
                    p.MotiveData[i] = (float)v;
                }
            }
            size_t pdCount = objm.Version > 0x45 ? 0x100 : 0x50;
            if (p.PersonData.size() < pdCount) p.PersonData.resize(pdCount, 0);
            for (size_t i = 0; i < pdCount && i <= 100; i++) p.PersonData[i] = ava->GetPersonData((VMPersonDataVariable)i);

            // queue: FreeSO ActiveQueueBlock = active ? 0 : -1; Queue[0] = ActiveInteraction, then InteractionQueue
            auto conv = [&](const std::shared_ptr<FSO::SimAntics::Engine::VMQueuedAction>& a) {
                OBJMInteraction r;
                r.UID = a->UID;
                r.CallerID = e->ObjectID;
                r.TargetID = a->Callee ? a->Callee->ObjectID : 0;
                r.Icon = a->IconOwner() ? a->IconOwner()->ObjectID : 0;
                r.TTAIndex = a->InteractionNumber;
                for (int k = 0; k < 4; k++) r.Args[k] = (k < (int)a->Args.size()) ? a->Args[(size_t)k] : 0;
                r.Priority = a->Priority;
                r.ActionTreeID = a->ActionRoutine ? (int16_t)a->ActionRoutine->ID : 0;
                r.Attenuation = 0;
                int32_t fl = 0;
                if ((a->Flags & TTABFlags::FSOPushHead) > 0) fl |= (int32_t)OBJMInteractionFlags::PushHeadContinuation;
                if (a->Priority >= (int16_t)FSO::SimAntics::Engine::VMQueuePriority::UserDriven) fl |= (int32_t)OBJMInteractionFlags::UserInitiated;
                if (a->Cancelled) fl |= (int32_t)OBJMInteractionFlags::UserInterrupted; // FreeSO: NotifyIdle <- UserInterrupted
                r.Flags = fl;
                return r;
            };
            p.ActiveInteraction = OBJMInteraction();
            p.InteractionQueue.clear();
            if (e->Thread) {
                auto& q = e->Thread->Queue;
                int active = e->Thread->ActiveQueueBlock;
                for (int i = 0; i < (int)q.size(); i++) {
                    if (!q[(size_t)i] || q[(size_t)i]->Mode != FSO::SimAntics::Engine::VMQueueMode::Normal) continue; // idle / parent entries are engine made
                    if (i <= active && i == active) p.ActiveInteraction = conv(q[(size_t)i]);
                    else if (i > active) p.InteractionQueue.push_back(conv(q[(size_t)i]));
                }
            }
            // object uses: FreeSO sets Stack[i].Callee = use.TargetID for i >= StackLength - 1
            p.ObjectUses.clear();
            {
                int16_t prev = e->ObjectID;
                for (size_t i = 0; i < frames.size(); i++) {
                    int16_t callee = frames[i]->Callee ? frames[i]->Callee->ObjectID : e->ObjectID;
                    if (callee != prev) {
                        OBJMObjectUse u; u.TargetID = callee; u.StackLength = (int32_t)i + 1; u.Unknown2 = 0;
                        p.ObjectUses.push_back(u);
                        prev = callee;
                    }
                }
            }
            // motive deltas: FreeSO MaxValue = StopAt, PerHourChange = round(TickDelta * 1800)
            p.MotiveDeltas.clear();
            for (int i = 0; i < 16; i++) {
                auto& mc = ava->GetMotiveChange(i);
                if (mc.PerHourChange == 0) continue;
                OBJMMotiveDelta d; d.Motive = i; d.TickDelta = mc.PerHourChange / 1800.0f; d.StopAt = mc.MaxValue;
                p.MotiveDeltas.push_back(d);
            }
        }
        if (base && (base->Inst.OBJT->OBJDType == OBJDType::Internal || base->Inst.OBJT->Name == "*Social Interactions" || base->Inst.OBJT->Name == "*comeandsee")) {
            // VMWorldActivator::RestoreSavedState leaves these interaction-only objects in their fresh init state (their
            // saved state names interaction steps that no longer run there), so the VM does not own their saved state:
            // the saved record is written back, with the tile list / position this save computed.
            OBJMInstance keep = base->Inst;
            keep.X = inst.X; keep.Y = inst.Y; keep.Level = inst.Level; keep.Footprint = inst.Footprint;
            keep.LinkedSlot = inst.LinkedSlot;
            for (int k : { (int)VMStackObjectVariable::ParentId, (int)VMStackObjectVariable::ContainerId, (int)VMStackObjectVariable::SlotNumber, (int)VMStackObjectVariable::Direction })
                keep.ObjectData[k] = inst.ObjectData[k];
            keep.OBJD = inst.OBJD;
            inst = keep;
            rec.UnhandledBits = base->Unhandled;
        }
        LastStats.Objects++;
        records[e->ObjectID] = std::move(rec);
    }
    for (auto& kv : orig) if (!liveIds.count(kv.first)) LastStats.Removed++;
    pDone = pTotal - 4; progress(0); // ADDED(sims) save progress: the other houses were not needed / are done

    // final objt (OBJTEntry views for the writer: portal / person record layout)
    {
        std::vector<uint8_t> objtData = objt.Write();
        objtChunk = file.Set("objt", 0, objtData);
        for (auto& kv : records) {
            auto& inst = kv.second.Instance;
            inst.OBJT = entryFor(kv.second.TypeID);
            if (inst.OBJT == nullptr) { Error = "internal: no objt entry for type " + std::to_string(kv.second.TypeID); return false; }
            // the record layout the reader expects for this type (person record iff OBJT type is Person)
            if (inst.HasPersonData != (inst.OBJT->OBJDType == OBJDType::Person)) {
                Error = "object " + std::to_string(kv.first) + ": person record / objt type mismatch";
                return false;
            }
        }
    }

    // record order: objects that are new since the load first (newest first), then the saved order
    std::vector<OBJMRecordOut> outRecs;
    {
        std::vector<int16_t> fresh;
        for (auto& kv : records) {
            auto it = orig.find(kv.first);
            bool kept = it != orig.end() && it->second.OK && it->second.Inst.OBJT && it->second.Inst.OBJT->GUID == kv.second.Instance.OBJD->GUID;
            if (!kept) fresh.push_back(kv.first);
        }
        std::sort(fresh.begin(), fresh.end(), [](int16_t a, int16_t b) { return a > b; });
        for (auto id : fresh) outRecs.push_back(records[id]);
        for (auto id : origOrder) {
            auto it = records.find(id);
            if (it == records.end()) continue;
            if (std::find(fresh.begin(), fresh.end(), id) != fresh.end()) continue;
            outRecs.push_back(it->second);
        }
    }
    file.Set("ObjM", 1, WriteOBJMChunk(objm.Pad, objm.Version, outRecs, objm.Tail));

    // ---------------------------------------------------------------- object grids ARRY#3 / #103
    for (int lvl = 0; lvl < 2; lvl++) {
        uint16_t id = lvl == 0 ? 3 : 103;
        ARRY a;
        int32_t w = 64, h = 64;
        if (DecodeARRY(file, id, a)) { w = a.Width; h = a.Height; }
        std::vector<uint8_t> grid((size_t)w * h * 2, 0); // loader order (y * w + x)
        for (auto& kv : tiles) {
            if (kv.first.Level != lvl + 1 || kv.second.empty()) continue;
            if (kv.first.X >= w || kv.first.Y >= h) continue;
            size_t idx = (size_t)kv.first.Y * w + kv.first.X;
            uint16_t head = (uint16_t)kv.second[0]->ObjectID;
            grid[idx * 2] = (uint8_t)head; grid[idx * 2 + 1] = (uint8_t)(head >> 8);
        }
        auto fileOrder = TransposeARRY(grid, w, 2);
        if (!(a.Data == fileOrder)) file.Set("Arry", id, WriteARRYChunk(w, h, 2, fileOrder), lvl == 0 ? "objects" : "objects2");
    }

    // ---------------------------------------------------------------- floors and walls (changed tiles only)
    {
        auto simiC = file.Find("SIMI", 1);
        int16_t size = 64;
        if (simiC && simiC->Data.size() >= 12 + 2 * 24) size = (int16_t)LE16(simiC->Data.data() + 12 + 2 * 23);
        auto hous = file.Find("HOUS", 0);
        bool flipRoad = false;
        if (hous && hous->Data.size() >= 30) flipRoad = (LE16(hous->Data.data() + 28) & 1) != 0; // HOUS.CameraDir
        WALmRaw flrm, walm;
        uint16_t flrmId = file.Find("FLRm", 1) ? 1 : 0, walmId = file.Find("WALm", 1) ? 1 : 0;
        bool hasFlrm = file.Find("FLRm", flrmId) && flrm.Parse(file.Find("FLRm", flrmId)->Data, flrmId);
        bool hasWalm = file.Find("WALm", walmId) && walm.Parse(file.Find("WALm", walmId)->Data, walmId);
        std::vector<std::pair<std::string, uint8_t>> fl, wl;
        for (auto& e : flrm.Entries) fl.push_back(std::make_pair(e.Name, e.ID));
        for (auto& e : walm.Entries) wl.push_back(std::make_pair(e.Name, e.ID));
        auto floorDict = BuildDict(fl, true);
        auto wallDict = BuildDict(wl, false);
        bool adv = file.Find("Arry", 11) != nullptr;
        ARRY flagsA;
        std::vector<uint8_t> flags;
        if (DecodeARRY(file, 8, flagsA)) flags = flagsA.TransposeData();
        ARRY poolA, waterA;
        std::vector<uint8_t> pools, water;
        if (DecodeARRY(file, 9, poolA)) pools = poolA.TransposeData();
        if (DecodeARRY(file, 10, waterA)) water = waterA.TransposeData();
        auto content = FSO::Content::Content::Get();

        // local id for a content floor/wall id: the existing map entry, or a new FLRm/WALm entry
        auto usedBytes = [&](const std::vector<uint8_t>& raw, int stride, int offset, int count) {
            std::set<int> s;
            for (size_t i = 0; i + (size_t)stride <= raw.size(); i += (size_t)stride) for (int k = 0; k < count; k++) s.insert(raw[i + (size_t)offset + (size_t)k]);
            return s;
        };
        bool flrmChanged = false, walmChanged = false;
        auto localId = [&](uint16_t contentId, bool floor, const std::set<int>& used) -> int {
            if (contentId < 256 && !(floor ? floorDict.count((uint8_t)contentId) : wallDict.count((uint8_t)contentId))) return contentId;
            auto& dict = floor ? floorDict : wallDict;
            for (auto& kv : dict) if (kv.second == contentId) return kv.first;
            std::string name;
            // the file name the loader maps back (VMWorldActivator::BuildFloorDict/BuildWallDict: name before the dot)
            if (floor) { for (auto& kv : content->WorldFloors->DynamicFloorFromID) if (kv.second == contentId) { name = kv.first; break; } }
            else { for (auto& kv : content->WorldWalls->DynamicWallFromID) if (kv.second == contentId) { name = kv.first; break; } }
            if (name.empty() || !(floor ? hasFlrm : hasWalm)) return -1;
            if (name.find('.') == std::string::npos) name += floor ? ".flr" : ".wll";
            for (int id = 255; id > 30; id--) {
                if (used.count(id) || dict.count((uint8_t)id)) continue;
                if (floor) { flrm.Add(name, (uint8_t)id); flrmChanged = true; }
                else { walm.Add(name, (uint8_t)id); walmChanged = true; }
                dict[(uint8_t)id] = contentId;
                return id;
            }
            return -1;
        };

        for (int lvl = 0; lvl < 2 && lvl < (int)arch->Floors.size(); lvl++) {
            uint16_t fid = adv ? (lvl ? 111 : 11) : (lvl ? 101 : 1);
            uint16_t wid = adv ? (lvl ? 112 : 12) : (lvl ? 102 : 2);
            ARRY fa, wa;
            if (!DecodeARRY(file, fid, fa) || !DecodeARRY(file, wid, wa)) continue;
            int fstride = adv ? 2 : 1, wstride = adv ? 14 : 8;
            auto fraw = fa.TransposeData(); // loader order, 64 x 64
            auto wraw = wa.TransposeData();
            int W = fa.Width;
            // the loader's decode of the saved tiles (VMWorldActivator::LoadFromIff)
            std::vector<FloorTile> f0(fraw.size() / fstride);
            for (size_t i = 0; i < f0.size(); i++) f0[i].Pattern = adv ? (uint16_t)(fraw[i * 2] | (fraw[i * 2 + 1] << 8)) : fraw[i];
            for (size_t i = 0; i < f0.size(); i++) {
                auto& p = f0[i].Pattern;
                if (flipRoad) { if (p == 10) p = 11; else if (p == 11) p = 10; }
                uint8_t flag = i < flags.size() ? flags[i] : 0;
                if ((p != 0 && (flag & 0x20) == 0) || p > 30) { auto it = floorDict.find((uint8_t)p); if (it != floorDict.end()) p = it->second; }
                if (lvl == 0 && i < pools.size() && pools[i] != 0xFF && pools[i] != 0) p = 65535;
                if (lvl == 0 && i < water.size() && water[i] != 0xFF && water[i] != 0) p = 65534;
            }
            std::vector<WallTile> w0(wraw.size() / wstride);
            for (size_t i = 0; i < w0.size(); i++) {
                const uint8_t* d = &wraw[i * wstride];
                WallTile t;
                t.Segments = (WallSegments)d[0];
                if (adv) {
                    t.TopLeftStyle = LE16(d + 2); t.TopRightStyle = LE16(d + 4); t.TopLeftPattern = LE16(d + 6);
                    t.TopRightPattern = LE16(d + 8); t.BottomLeftPattern = LE16(d + 10); t.BottomRightPattern = LE16(d + 12);
                } else {
                    t.TopLeftStyle = d[2]; t.TopRightStyle = d[3]; t.TopLeftPattern = d[4]; t.TopRightPattern = d[5];
                    t.BottomLeftPattern = d[6]; t.BottomRightPattern = d[7];
                }
                if ((t.Segments & WallSegments::AnyDiag) == 0) {
                    static const std::set<uint16_t> valid = { 2, 12, 14, 13 };
                    if (!valid.count(t.TopLeftStyle)) t.TopLeftStyle = 1;
                    if (!valid.count(t.TopRightStyle)) t.TopRightStyle = 1;
                    if ((t.Segments & WallSegments::TopLeft) == 0) t.TopLeftStyle = 0;
                    if ((t.Segments & WallSegments::TopRight) == 0) t.TopRightStyle = 0;
                }
                auto remap = [](const std::unordered_map<uint8_t, uint16_t>& dct, uint16_t& v) { if (v == 0) return; auto it = dct.find((uint8_t)v); if (it != dct.end()) v = it->second; };
                remap(wallDict, t.BottomLeftPattern); remap(wallDict, t.BottomRightPattern);
                if ((t.Segments & WallSegments::AnyDiag) > 0) { remap(floorDict, t.TopLeftPattern); remap(floorDict, t.TopLeftStyle); }
                else { remap(wallDict, t.TopLeftPattern); remap(wallDict, t.TopRightPattern); }
                w0[i] = t;
            }
            auto usedF = usedBytes(fraw, fstride, 0, 1);
            auto usedW = usedBytes(wraw, wstride, 4, 4);
            bool fChanged = false, wChanged = false;
            bool poolsDirty = false, waterDirty = false; // TS1(sims)
            auto& curF = arch->Floors[(size_t)lvl];
            auto& curW = arch->Walls[(size_t)lvl];
            for (int y = 0; y < size && y < W; y++) for (int x = 0; x < size && x < W; x++) {
                size_t ai = (size_t)y * arch->Width + x, fi = (size_t)y * W + x;
                if (ai >= curF.size() || fi >= f0.size()) continue;
                // floors
                uint16_t now = curF[ai].Pattern, was = f0[fi].Pattern;
                if (now != was) {
                    if (lvl == 0 && (now >= 65534 || was >= 65534)) {
                        // pool / water tiles (ARRY 9/10): 0xFF = none
                        if (now == 65535 || was == 65535) poolsDirty = true;
                        if (now == 65534 || was == 65534) waterDirty = true;
                    }
                    if (now < 65534) {
                        uint16_t p = now;
                        int lid = localId(p, true, usedF);
                        if (lid >= 0) {
                            uint16_t b = (uint16_t)lid;
                            if (flipRoad) { if (b == 10) b = 11; else if (b == 11) b = 10; }
                            if (adv) { fraw[fi * 2] = (uint8_t)b; fraw[fi * 2 + 1] = (uint8_t)(b >> 8); }
                            else fraw[fi] = (uint8_t)b;
                            fChanged = true;
                            LastStats.FloorTiles++;
                        }
                    }
                }
                // walls
                if (fi >= w0.size() || ai >= curW.size()) continue;
                const WallTile& a = curW[ai];
                const WallTile& b = w0[fi];
                if ((int)a.Segments == 0 && (int)b.Segments == 0) continue; // paint left on removed walls: the engine drops it, the file keeps it
                if (a.Segments == b.Segments && a.TopLeftPattern == b.TopLeftPattern && a.TopRightPattern == b.TopRightPattern &&
                    a.BottomLeftPattern == b.BottomLeftPattern && a.BottomRightPattern == b.BottomRightPattern &&
                    a.TopLeftStyle == b.TopLeftStyle && a.TopRightStyle == b.TopRightStyle) continue;
                if (getenv("SAVER_DEBUG")) printf("saver: wall L%d (%d,%d) seg %d/%d tl %d/%d tr %d/%d bl %d/%d br %d/%d tls %d/%d trs %d/%d\n", lvl + 1, x, y,
                    (int)a.Segments, (int)b.Segments, a.TopLeftPattern, b.TopLeftPattern, a.TopRightPattern, b.TopRightPattern, a.BottomLeftPattern, b.BottomLeftPattern,
                    a.BottomRightPattern, b.BottomRightPattern, a.TopLeftStyle, b.TopLeftStyle, a.TopRightStyle, b.TopRightStyle);
                uint8_t* d = &wraw[fi * wstride];
                bool diag = (a.Segments & WallSegments::AnyDiag) > 0;
                auto wl = [&](uint16_t v) -> uint16_t { int l = (v == 0) ? 0 : localId(v, false, usedW); return (uint16_t)std::max(0, l); };
                auto flid = [&](uint16_t v) -> uint16_t { int l = (v == 0) ? 0 : localId(v, true, usedF); return (uint16_t)std::max(0, l); };
                uint16_t vals[6] = { diag ? flid(a.TopLeftStyle) : a.TopLeftStyle, a.TopRightStyle,
                                     diag ? flid(a.TopLeftPattern) : wl(a.TopLeftPattern), wl(a.TopRightPattern),
                                     wl(a.BottomLeftPattern), wl(a.BottomRightPattern) };
                d[0] = (uint8_t)a.Segments;
                for (int k = 0; k < 6; k++) {
                    if (adv) { d[2 + k * 2] = (uint8_t)vals[k]; d[3 + k * 2] = (uint8_t)(vals[k] >> 8); }
                    else d[2 + k] = (uint8_t)vals[k];
                }
                wChanged = true;
                LastStats.WallTiles++;
            }
            if (fChanged) file.Set("Arry", fid, WriteARRYChunk(fa.Width, fa.Height, fstride, TransposeARRY(fraw, fa.Width, fstride)));
            if (wChanged) file.Set("Arry", wid, WriteARRYChunk(wa.Width, wa.Height, wstride, TransposeARRY(wraw, wa.Width, wstride)));
            if (lvl == 0 && (poolsDirty || waterDirty)) {
                // TS1(sims): ARRY 9 (pools) / 10 (water) hold per tile the mask of the 4 neighbours of the same
                // kind (0xFF = none; bits in loader order: 1 = (x, y-1), 2 = (x-1, y), 4 = (x, y+1), 8 = (x+1, y)),
                // verified on every shipped pool/pond; regenerated when the pool changed
                auto regen = [&](std::vector<uint8_t>& grid, uint16_t pat) {
                    for (int y = 0; y < size && y < W; y++) for (int x = 0; x < size && x < W; x++) {
                        size_t fi = (size_t)y * W + x;
                        if (fi >= grid.size()) continue;
                        auto is = [&](int xx, int yy) { return xx >= 0 && yy >= 0 && xx < size && yy < size && curF[(size_t)yy * arch->Width + xx].Pattern == pat; };
                        if (!is(x, y)) { grid[fi] = 0xFF; continue; }
                        grid[fi] = (uint8_t)((is(x, y - 1) ? 1 : 0) | (is(x - 1, y) ? 2 : 0) | (is(x, y + 1) ? 4 : 0) | (is(x + 1, y) ? 8 : 0));
                    }
                };
                if (poolsDirty) regen(pools, 65535);
                if (waterDirty) regen(water, 65534);
                if (!pools.empty() && !(poolA.TransposeData() == pools)) file.Set("Arry", 9, WriteARRYChunk(poolA.Width, poolA.Height, 1, TransposeARRY(pools, poolA.Width, 1)));
                if (!water.empty() && !(waterA.TransposeData() == water)) file.Set("Arry", 10, WriteARRYChunk(waterA.Width, waterA.Height, 1, TransposeARRY(water, waterA.Width, 1)));
            }
        }
        if (flrmChanged) {
            auto data = flrm.Write();
            file.Set("FLRm", flrmId, data);
            if (flrmId == 1 && file.Find("FLRm", 0)) { WALmRaw f0; if (f0.Parse(file.Find("FLRm", 0)->Data, 0)) { for (auto& e : flrm.Entries) { bool has = false; for (auto& o : f0.Entries) if (o.ID == e.ID) has = true; if (!has) f0.Add(e.Name, e.ID); } file.Set("FLRm", 0, f0.Write()); } }
        }
        if (walmChanged) {
            auto data = walm.Write();
            file.Set("WALm", walmId, data);
            if (walmId == 1 && file.Find("WALm", 0)) { WALmRaw w0; if (w0.Parse(file.Find("WALm", 0)->Data, 0)) { for (auto& e : walm.Entries) { bool has = false; for (auto& o : w0.Entries) if (o.ID == e.ID) has = true; if (!has) w0.Add(e.Name, e.ID); } file.Set("WALm", 0, w0.Write()); } }
        }
    }

    // ---------------------------------------------------------------- terrain (TS1(sims)): ARRY 0 altitude, 6 lawn, 8 flags
    if (arch->Terrain) {
        auto& T = *arch->Terrain;
        int AW = arch->Width;
        ARRY altA;
        if (DecodeARRY(file, 0, altA) && (int)altA.Type == 4) {
            auto raw = altA.TransposeData(); // loader order: tile (x, y) at (y * W + x) * 4, byte 1 = vertex (x, y)
            int W = altA.Width;
            auto savedB1 = [&](int vx, int vy) -> int { if (vx >= W || vy >= W) return -1; return raw[(size_t)(vy * W + vx) * 4 + 1]; };
            // vertices beyond the lot size are not in the engine (the loader crops to SIMI size): keep their saved value
            auto V = [&](int x, int y) -> int {
                if (x >= AW || y >= (int)arch->Height) { int sv = savedB1(x, y); return sv >= 0 ? sv : T.Heights[(size_t)(std::min<int>(y, (int)arch->Height - 1) * AW + std::min<int>(x, AW - 1))] / 10; }
                return T.Heights[(size_t)(y * AW + x)] / 10;
            };
            bool changed = false;
            ARRY flagsA; std::vector<uint8_t> flags; bool flagsChanged = false;
            if (DecodeARRY(file, 8, flagsA)) flags = flagsA.TransposeData();
            for (int y = 0; y < arch->Height && y < W; y++) for (int x = 0; x < AW && x < W; x++) {
                uint8_t* d = &raw[(size_t)(y * W + x) * 4];
                int saved = d[1];
                // the loader read byte 1 only; tiles whose vertex did not change keep all their bytes
                bool tileChanged = saved != V(x, y) || d[2] != (uint8_t)V(x + 1, y) || d[0] != (uint8_t)V(x, y + 1) || d[3] != (uint8_t)V(x + 1, y + 1);
                if (!tileChanged) continue;
                // only rewrite when some corner of this tile actually moved in the engine (the shipped files have
                // a few tiles whose corner bytes disagree with their neighbours: keep them)
                int c[4] = { V(x, y), V(x + 1, y), V(x, y + 1), V(x + 1, y + 1) };
                int s0 = d[1], s1 = d[2], s2 = d[0], s3 = d[3];
                bool engineMoved = false;
                {
                    // the engine heights of the four corners came from byte 1 of four tiles; compare with those saved values
                    auto savedV = [&](int vx, int vy) -> int { if (vx >= W || vy >= W) return -1; return raw[(size_t)(vy * W + vx) * 4 + 1]; };
                    engineMoved = savedV(x, y) != c[0] || (savedV(x + 1, y) >= 0 && savedV(x + 1, y) != c[1]) ||
                                  (savedV(x, y + 1) >= 0 && savedV(x, y + 1) != c[2]) || (savedV(x + 1, y + 1) >= 0 && savedV(x + 1, y + 1) != c[3]);
                }
                (void)s0; (void)s1; (void)s2; (void)s3;
                if (!engineMoved) continue;
                d[1] = (uint8_t)c[0]; d[2] = (uint8_t)c[1]; d[0] = (uint8_t)c[2]; d[3] = (uint8_t)c[3];
                changed = true;
                size_t fi = (size_t)(y * W + x);
                if (fi < flags.size()) {
                    bool sloped = c[0] != c[1] || c[0] != c[2] || c[0] != c[3];
                    uint8_t nf = (uint8_t)(sloped ? (flags[fi] & ~1) : (flags[fi] | 1)); // flag 1 = level (FreeSO "not sloped")
                    if (nf != flags[fi]) { flags[fi] = nf; flagsChanged = true; }
                }
            }
            // byte 1 holds the vertex: rewrite pass 2 over the ones changed above already happened per tile; tiles whose
            // byte 1 vertex moved are covered by engineMoved
            if (changed) { file.Set("Arry", 0, WriteARRYChunk(altA.Width, altA.Height, 4, TransposeARRY(raw, altA.Width, 4))); }
            if (flagsChanged) file.Set("Arry", 8, WriteARRYChunk(flagsA.Width, flagsA.Height, 1, TransposeARRY(flags, flagsA.Width, 1)));
        }
        ARRY grassA;
        if (DecodeARRY(file, 6, grassA) && (int)grassA.Type == 1 && !T.GrassState.empty()) {
            auto raw = grassA.TransposeData();
            int W = grassA.Width; bool changed = false;
            for (int y = 0; y < arch->Height && y < W; y++) for (int x = 0; x < AW && x < W; x++) {
                uint8_t v = (uint8_t)(127 - T.GrassState[(size_t)(y * AW + x)]); // loader: GrassState = 127 - x
                if (raw[(size_t)(y * W + x)] != v) { raw[(size_t)(y * W + x)] = v; changed = true; }
            }
            if (changed) file.Set("Arry", 6, WriteARRYChunk(grassA.Width, grassA.Height, 1, TransposeARRY(raw, grassA.Width, 1)));
        }
    }

    // ---------------------------------------------------------------- HOUS roof (TS1(sims)): style name + pitch
    if (auto hous = file.Find("HOUS", 0)) {
        auto& d = hous->Data;
        if (d.size() >= 39) {
            auto roofs = FSO::Content::Content::Get()->WorldRoofs.get();
            std::string saved((const char*)d.data() + 38, strnlen((const char*)d.data() + 38, d.size() - 38));
            float pitch; uint32_t bits = LE32(d.data() + 34); std::memcpy(&pitch, &bits, sizeof pitch);
            bool styleSame = roofs != nullptr && (uint32_t)roofs->NameToID(FSO::Content::ToLowerInvariant(saved) + ".bmp") == arch->RoofStyle;
            bool pitchSame = std::isfinite(pitch) && std::fabs(pitch - arch->RoofPitch) < 1e-4f;
            if (arch->RoofUserSet && (!styleSame || !pitchSame)) {
                std::vector<uint8_t> nd(d.begin(), d.begin() + 34);
                uint32_t pb; std::memcpy(&pb, &arch->RoofPitch, sizeof pb);
                for (int k = 0; k < 4; k++) nd.push_back((uint8_t)(pb >> (8 * k)));
                std::string name = saved;
                if (!styleSame && roofs != nullptr) {
                    name = roofs->IDToName((int32_t)arch->RoofStyle);
                    if (name.size() > 4 && name.substr(name.size() - 4) == ".bmp") name.resize(name.size() - 4);
                }
                size_t tailAt = 38 + saved.size() + 1; // bytes after the name (padding) are kept
                nd.insert(nd.end(), name.begin(), name.end());
                nd.push_back(0);
                if (tailAt < d.size()) nd.insert(nd.end(), d.begin() + (long)tailAt, d.end());
                if (nd.size() & 1) nd.push_back(0);
                d = nd;
            }
        }
    }

    // ---------------------------------------------------------------- SIMI globals (patched in place)
    if (auto simi = file.Find("SIMI", 1)) {
        auto& d = simi->Data;
        if (d.size() >= 12) {
            uint32_t version = LE32(d.data() + 4);
            int items = (version > 0x3F) ? 0x40 : 0x20; // SIMI.cs
            // globals the loader overrides for this engine (VMWorldActivator / FreeSO VMTS1ActivatorNew: 17 runtime code
            // version, 20 game edition, 25 idle setting, 32, 33 machine level) keep the values the game saved
            static const std::set<int> keepSaved = { 17, 20, 25, 32, 33 };
            for (int i = 0; i < items && i < (int)VM->GlobalState.size(); i++) {
                if (keepSaved.count(i)) continue;
                size_t at = 12 + (size_t)i * 2;
                if (at + 2 > d.size()) break;
                int16_t v = VM->GlobalState[(size_t)i];
                d[at] = (uint8_t)v; d[at + 1] = (uint8_t)((uint16_t)v >> 8);
            }
            // TS1(sims): LotValue / ObjectsValue / ArchitectureValue after the globals, a short and five ints
            size_t lv = 12 + (size_t)items * 2 + 2 + 4 * 5;
            if (lv + 12 <= d.size() && (ArchitectureDelta != 0 || ObjectsDelta != 0 || SetArchitectureValue >= 0 || SetObjectsValue >= 0)) {
                auto put = [&](size_t at, int32_t v) { for (int k = 0; k < 4; k++) d[at + (size_t)k] = (uint8_t)((uint32_t)v >> (8 * k)); };
                int32_t objects = (int32_t)LE32(d.data() + lv + 4), archv = (int32_t)LE32(d.data() + lv + 8);
                objects = SetObjectsValue >= 0 ? SetObjectsValue : std::max<int32_t>(0, objects + ObjectsDelta);
                archv = SetArchitectureValue >= 0 ? SetArchitectureValue : std::max<int32_t>(0, archv + ArchitectureDelta);
                put(lv + 4, objects); put(lv + 8, archv);
            }
        }
    }

    // ---------------------------------------------------------------- thumbnail
    for (auto& kv : Pictures) {
        const Picture& pic = kv.second;
        if (pic.W <= 0 || pic.H <= 0 || pic.RGB.size() < (size_t)pic.W * pic.H * 3) continue;
        auto writeBmp = [&](int id, int32_t w, int32_t h, const std::vector<uint8_t>& rgb, const char* label) {
            std::vector<uint8_t> bmp;
            uint32_t rowSize = ((uint32_t)w * 3 + 3) & ~3u;
            // header as the game writes it: sizes left 0, 24 bpp, uncompressed
            bmp.push_back('B'); bmp.push_back('M');
            Put32(bmp, 0); Put32(bmp, 0); Put32(bmp, 54);
            Put32(bmp, 40); Put32(bmp, (uint32_t)w); Put32(bmp, (uint32_t)h);
            Put16(bmp, 1); Put16(bmp, 24);
            for (int k = 0; k < 6; k++) Put32(bmp, 0);
            for (int y = h - 1; y >= 0; y--) {
                const uint8_t* row = &rgb[(size_t)y * w * 3];
                for (int x = 0; x < w; x++) { bmp.push_back(row[x * 3 + 2]); bmp.push_back(row[x * 3 + 1]); bmp.push_back(row[x * 3]); }
                for (uint32_t k = (uint32_t)w * 3; k < rowSize; k++) bmp.push_back(0);
            }
            if (auto c = file.Find("BMP_", (int16_t)id)) c->Data = bmp;
            else file.Set("BMP_", (int16_t)id, bmp, label, 0);
        };
        writeBmp(kv.first, pic.W, pic.H, pic.RGB, kv.first == 512 ? "Thumbnail" : "Thumbnail");
        // FIX(sims) FB1: the half-size copy (BMP_ 1024 / 1025) when the file has one: 2x2 average, magenta stays magenta
        if (auto half = file.Find("BMP_", (int16_t)(kv.first + 512))) {
            int32_t hw = std::max<int32_t>(1, pic.W / 2), hh = std::max<int32_t>(1, pic.H / 2);
            std::vector<uint8_t> small((size_t)hw * hh * 3);
            for (int y = 0; y < hh; y++) for (int x = 0; x < hw; x++) {
                int acc[3] = { 0, 0, 0 }, n = 0;
                for (int k = 0; k < 4; k++) {
                    int sx = (int)std::min<int32_t>(pic.W - 1, x * 2 + (k & 1)), sy = (int)std::min<int32_t>(pic.H - 1, y * 2 + (k >> 1));
                    const uint8_t* q = &pic.RGB[((size_t)sy * pic.W + sx) * 3];
                    if (q[0] == 248 && q[1] == 0 && q[2] == 248) continue;
                    acc[0] += q[0]; acc[1] += q[1]; acc[2] += q[2]; n++;
                }
                uint8_t* d = &small[((size_t)y * hw + x) * 3];
                if (n < 2) { d[0] = 248; d[1] = 0; d[2] = 248; }
                else { d[0] = (uint8_t)(acc[0] / n); d[1] = (uint8_t)(acc[1] / n); d[2] = (uint8_t)(acc[2] / n); if (d[0] == 248 && d[1] == 0 && d[2] == 248) d[1] = 1; }
            }
            (void)half;
            writeBmp(kv.first + 512, hw, hh, small, "Thumbnail");
        }
    }

    progress(2); // ADDED(sims) save progress
    out = file.Write();
    char buf[256];
    snprintf(buf, sizeof buf, "house: %d objects (%d persons; %d kept, %d new, %d removed, %d new types), %d stack frames, %d contained, %d floor / %d wall tiles changed",
             LastStats.Objects, LastStats.Persons, LastStats.Kept, LastStats.Added, LastStats.Removed, LastStats.NewTypes,
             LastStats.StackFrames, LastStats.Contained, LastStats.FloorTiles, LastStats.WallTiles);
    Log.push_back(buf);
    return true;
}

// ================================================================== neighbourhood

bool VMTS1LotSaver::UpdateNeighbourhood() {
    auto content = FSO::Content::Content::Get();
    auto prov = content ? content->Neighborhood.get() : nullptr;
    if (prov == nullptr || prov->Neighbors == nullptr) { Error = "no neighbourhood"; return false; }
    // TS1 Generic Sims Call 25 "save this Sim's persistent data" for every person with a neighbour record (on-lot
    // relationships between neighbours already live in their NBRS records, see VMRelationship)
    int saved = 0;
    for (auto e : VM->Entities) {
        auto ava = dynamic_cast<VMAvatar*>(e);
        if (!ava || ava->Dead) continue;
        if (ava->GetPersonData(VMPersonDataVariable::NeighborId) <= 0) continue;
        if (FSO::SimAntics::Engine::Utils::VMTS1Neighbourhood::SavePersistentData(ava)) saved++;
    }
    // the lot family's money (globals 13/14 = budget mod/div 10000) into its FAMI
    using FSO::SimAntics::Engine::Utils::VMTS1Neighbourhood;
    int16_t famNum = VM->GetGlobalValue(VMTS1Neighbourhood::GLOBAL_CURRENT_FAMILY);
    int32_t budget = VM->GetGlobalValue(VMTS1Neighbourhood::GLOBAL_BUDGET_DIV) * 10000 + VM->GetGlobalValue(VMTS1Neighbourhood::GLOBAL_BUDGET_MOD);
    auto fami = famNum > 0 ? prov->GetFamily(famNum) : nullptr;
    if (fami != nullptr && budget >= 0 && fami->Budget != budget) {
        Log.push_back("family " + std::to_string(famNum) + " budget " + std::to_string(fami->Budget) + " -> " + std::to_string(budget));
        fami->Budget = budget;
    }
    // FIX(sims): the family's Makin' Magic spells (Magic Controller attributes) into the neighbourhood, which writes them
    // (TS1NeighbourhoodWriter, chunk FSPL) - they were kept in memory only and lost on a neighbourhood reload.
    FSO::SimAntics::Engine::Utils::VMTS1Travel::SaveFamilySpells(VM);
    LastStats.Neighbours = saved;
    Log.push_back("neighbourhood: persistent data of " + std::to_string(saved) + " Sims updated in memory");
    return true;
}

bool VMTS1LotSaver::SaveNeighbours(const std::string& neighborhoodPath) {
    Error.clear();
    if (!UpdateNeighbourhood()) return false;
    auto prov = FSO::Content::Content::Get()->Neighborhood.get();
    std::string err;
    if (!FSO::Content::TS1::TS1NeighbourhoodWriter::Save(prov, neighborhoodPath, std::vector<int16_t>(), &err, &Log)) { Error = err; return false; }
    return true;
}
