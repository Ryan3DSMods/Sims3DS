/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// TS1(sims): see TS1CharacterFiles.h. The raw IFF code moved here from src/app/cas/CASFamilyWriter.cpp.
#include "content/TS1CharacterFiles.h"
#include "files/SafeFile.h"
#include "content/ContentManager.h"
#include "files/FAR1/FAR1Archive.h"
#include "files/formats/iff/chunks/NBRS.h"
#include "content/TS1NeighbourProvider.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace FSO { namespace Content { namespace TS1 {

static const char* IFF_HEADER = "IFF FILE 2.5:TYPE FOLLOWED BY SIZE\0 JAMIE DOORNBOS & MAXIS 1";

static uint32_t BE32(const uint8_t* p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
static uint16_t BE16(const uint8_t* p) { return (uint16_t)((p[0] << 8) | p[1]); }
static void PutBE32(std::vector<uint8_t>& o, uint32_t v) { o.push_back((uint8_t)(v >> 24)); o.push_back((uint8_t)(v >> 16)); o.push_back((uint8_t)(v >> 8)); o.push_back((uint8_t)v); }
static void PutBE16(std::vector<uint8_t>& o, uint16_t v) { o.push_back((uint8_t)(v >> 8)); o.push_back((uint8_t)v); }
static void PutLE32(std::vector<uint8_t>& o, uint32_t v) { o.push_back((uint8_t)v); o.push_back((uint8_t)(v >> 8)); o.push_back((uint8_t)(v >> 16)); o.push_back((uint8_t)(v >> 24)); }
static void PutLE16(std::vector<uint8_t>& o, uint16_t v) { o.push_back((uint8_t)v); o.push_back((uint8_t)(v >> 8)); }
static void SetLE32(std::vector<uint8_t>& o, size_t at, uint32_t v) { if (at + 4 <= o.size()) { o[at] = (uint8_t)v; o[at + 1] = (uint8_t)(v >> 8); o[at + 2] = (uint8_t)(v >> 16); o[at + 3] = (uint8_t)(v >> 24); } }
static std::string Lower(std::string s) { for (auto& c : s) c = (char)tolower((unsigned char)c); return s; }
static bool FileExistsPath(const std::string& p) { FILE* f = fopen(p.c_str(), "rb"); if (f) { fclose(f); return true; } return false; }

// ------------------------------------------------------------------------------------------- raw IFF

bool RawIff::ReadFile(const std::string& path, std::vector<uint8_t>& out) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    // FIX(sims) QA2: a directory opens on some systems and ftell() then says LONG_MAX: the .FAM scan of
    // TemplateFamilyUnleashed asked for a 2 GB buffer (32-bit host peak 2055 MB; out of memory on the 3DS). Character,
    // family and house files are a few MB at most.
    if (n < 0 || n > 64L * 1024 * 1024) { fclose(f); out.clear(); return false; }
    out.resize(n > 0 ? (size_t)n : 0);
    size_t got = n > 0 ? fread(out.data(), 1, (size_t)n, f) : 0;
    fclose(f);
    return got == out.size();
}

bool RawIff::WriteFileAtomic(const std::string& path, const std::vector<uint8_t>& data) {
    // FIX(sims) SAVE: was "write .tmp, REMOVE the target, rename": a crash or power cut after the remove left only
    // <path>.tmp (Neighborhood.iff from CAS / family import, User*.iff). Now the one crash-safe writer, which also
    // detaches the deferred readers (IffFile::DetachSource) and closes the pooled handle first.
    return FSO::Files::SafeFile::Write(path, data);
}

bool RawIff::Parse(const std::vector<uint8_t>& file, std::vector<RawIffChunk>& out) {
    out.clear();
    if (file.size() < 64 || memcmp(file.data(), "IFF FILE 2.", 11) != 0) return false;
    size_t off = 64;
    while (off + 76 <= file.size()) {
        const uint8_t* p = &file[off];
        uint32_t size = BE32(p + 4);
        if (size < 76 || off + size > file.size()) break; // truncated / garbage tail
        RawIffChunk c;
        c.Type.assign((const char*)p, 4);
        c.ID = BE16(p + 8);
        c.Flags = BE16(p + 10);
        const char* lab = (const char*)p + 12;
        c.Label.assign(lab, strnlen(lab, 64));
        c.Data.assign(p + 76, p + size);
        out.push_back(std::move(c));
        off += size;
    }
    return true;
}

std::vector<uint8_t> RawIff::Build(const std::vector<RawIffChunk>& chunks) {
    std::vector<uint8_t> o;
    o.insert(o.end(), IFF_HEADER, IFF_HEADER + 60); // 60 bytes of header text (with its embedded NUL)
    PutBE32(o, 0);                   // resource map offset (patched below)
    struct MapEntry { uint32_t Offset; uint16_t ID, Flags; std::string Label; };
    std::vector<std::pair<std::string, std::vector<MapEntry>>> map; // type order of first appearance
    for (auto& c : chunks) {
        if (c.Type == "XXXX" || c.Type == "rsmp") continue;
        uint32_t at = (uint32_t)o.size();
        std::string type = c.Type; type.resize(4, ' ');
        o.insert(o.end(), type.begin(), type.end());
        PutBE32(o, (uint32_t)(c.Data.size() + 76));
        PutBE16(o, c.ID);
        PutBE16(o, c.Flags);
        std::string label = c.Label.substr(0, 63);
        o.insert(o.end(), label.begin(), label.end());
        for (size_t i = label.size(); i < 64; i++) o.push_back(0);
        o.insert(o.end(), c.Data.begin(), c.Data.end());
        auto it = std::find_if(map.begin(), map.end(), [&](const std::pair<std::string, std::vector<MapEntry>>& e) { return e.first == type; });
        if (it == map.end()) { map.push_back({ type, {} }); it = map.end() - 1; }
        it->second.push_back({ at, c.ID, c.Flags, c.Label });
    }
    // rsmp version 0 (as every shipped file): u32 0, u32 version 0, "pmsr", u32 size, u32 type count,
    // per type: type reversed, u32 count, per chunk: u32 offset, u16 id, u16 flags, label C string padded to even.
    std::vector<uint8_t> r;
    PutLE32(r, 0); PutLE32(r, 0);
    r.push_back('p'); r.push_back('m'); r.push_back('s'); r.push_back('r');
    PutLE32(r, 0); // size (0 in the freshly installed Neighborhood.iff)
    PutLE32(r, (uint32_t)map.size());
    for (auto& t : map) {
        for (int i = 3; i >= 0; i--) r.push_back((uint8_t)t.first[(size_t)i]);
        PutLE32(r, (uint32_t)t.second.size());
        for (auto& e : t.second) {
            PutLE32(r, e.Offset);
            PutLE16(r, e.ID);
            PutLE16(r, e.Flags);
            r.insert(r.end(), e.Label.begin(), e.Label.end());
            r.push_back(0);
            if (r.size() & 1) r.push_back(0);
        }
    }
    uint32_t rsmpAt = (uint32_t)o.size();
    o.insert(o.end(), { 'r', 's', 'm', 'p' });
    PutBE32(o, (uint32_t)(r.size() + 76));
    PutBE16(o, 0);
    PutBE16(o, 0x10);
    for (int i = 0; i < 64; i++) o.push_back(0);
    o.insert(o.end(), r.begin(), r.end());
    o[60] = (uint8_t)(rsmpAt >> 24); o[61] = (uint8_t)(rsmpAt >> 16); o[62] = (uint8_t)(rsmpAt >> 8); o[63] = (uint8_t)rsmpAt;
    return o;
}

std::vector<uint8_t> RawIff::MakeSTRFormat3(const std::vector<std::string>& values) {
    std::vector<uint8_t> o;
    PutLE16(o, 0xFFFD);
    PutLE16(o, (uint16_t)values.size());
    for (auto& v : values) {
        o.push_back(1); // US English
        o.insert(o.end(), v.begin(), v.end());
        o.push_back(0);
        o.push_back(0); // empty comment
    }
    return o;
}

std::vector<std::string> RawIff::ReadSTRValues(const std::vector<uint8_t>& d) {
    std::vector<std::string> out;
    size_t p = 0;
    auto u8 = [&]() -> uint8_t { return p < d.size() ? d[p++] : 0; };
    auto u16 = [&]() -> uint16_t { uint16_t v = (uint16_t)(u8() | (u8() << 8)); return v; };
    auto cstr = [&]() { std::string s; while (p < d.size() && d[p] != 0) s += (char)d[p++]; if (p < d.size()) p++; return s; };
    auto pstr = [&]() { uint8_t n = u8(); std::string s; for (int i = 0; i < n && p < d.size(); i++) s += (char)d[p++]; return s; };
    auto vstr = [&]() { // 7-bit length
        uint32_t n = 0; int shift = 0; uint8_t b;
        do { b = u8(); n |= (uint32_t)(b & 0x7F) << shift; shift += 7; } while ((b & 0x80) && p < d.size());
        std::string s; for (uint32_t i = 0; i < n && p < d.size(); i++) s += (char)d[p++]; return s; };
    if (d.size() < 2) return out;
    int16_t fmt = (int16_t)u16();
    switch (fmt) {
        case 0: { uint16_t n = u16(); for (int i = 0; i < n; i++) out.push_back(pstr()); break; }
        case -1: { uint16_t n = u16(); for (int i = 0; i < n; i++) out.push_back(cstr()); break; }
        case -2: { uint16_t n = u16(); for (int i = 0; i < n; i++) { out.push_back(cstr()); cstr(); } break; }
        case -3: {
            uint16_t n = u16();
            std::vector<std::string> first;
            for (int i = 0; i < n && p < d.size(); i++) {
                uint8_t lang = u8(); auto v = cstr(); cstr();
                if (lang == 1 || lang == 0) out.push_back(v); else first.push_back(v);
            }
            if (out.empty()) out = first;
            break;
        }
        case -4: {
            uint8_t sets = u8();
            for (int s = 0; s < sets && p < d.size(); s++) {
                uint16_t n = u16();
                std::vector<std::string> vals;
                for (int i = 0; i < n && p < d.size(); i++) { u8(); vals.push_back(vstr()); vstr(); }
                if (s == 0) out = vals;
            }
            break;
        }
        default: break;
    }
    return out;
}

RawIffChunk* RawIff::Find(std::vector<RawIffChunk>& chunks, const std::string& type, uint16_t id) {
    for (auto& c : chunks) if (c.Type == type && c.ID == id) return &c;
    return nullptr;
}

// ------------------------------------------------------------------------------------------- characters

bool TS1CharacterFiles::LoadObjectIff(uint32_t guid, std::vector<uint8_t>& out) {
    auto content = Content::Get();
    if (content == nullptr || content->WorldObjects == nullptr) return false;
    auto world = content->WorldObjects.get();
    auto it = world->Entries.find(guid);
    if (it == world->Entries.end()) return false;
    auto& ref = it->second;
    if (ref.Source != GameObjectSource::Far) return RawIff::ReadFile(ref.FileName, out);
    auto name = Lower(ref.FileName + ".iff");
    auto tryArch = [&](WorldObjectProvider::Archive& a) -> bool {
        auto e = a.EntriesByName.find(name);
        if (e == a.EntriesByName.end() || a.Far == nullptr) return false;
        bool found = false;
        out = a.Far->GetEntry(e->second, &found);
        return found && !out.empty();
    };
    if (ref.FarIndex >= 0 && ref.FarIndex < (int32_t)world->Archives.size() && tryArch(world->Archives[ref.FarIndex])) return true;
    for (auto& a : world->Archives) if (tryArch(a)) return true;
    return false;
}

std::vector<RawIffChunk> TS1CharacterFiles::Clone(const std::vector<RawIffChunk>& tpl, const NewCharacter& c, const std::string& userName) {
    std::vector<RawIffChunk> out;
    auto byType = [&](const char* type) {
        std::vector<const RawIffChunk*> v;
        for (auto& k : tpl) if (k.Type == type) v.push_back(&k);
        return v;
    };
    auto overridden = [&](const RawIffChunk& k) {
        for (auto& e : c.Extra) if (e.Type == k.Type && e.ID == k.ID) return true;
        return false;
    };
    // OBJD, GLOB, SLOT first (the shipped order)
    for (auto t : { "OBJD", "GLOB", "SLOT" })
        for (auto k0 : byType(t)) {
            RawIffChunk k = *k0;
            if (k.Type == "OBJD") {
                SetLE32(k.Data, 28, c.GUID);            // GUID1/GUID2
                SetLE32(k.Data, 140, c.TemplateGUID);   // OriginalGUID1/OriginalGUID2
                k.Label = userName + " - " + c.FirstName;
                if (c.ForcePerson && k.Data.size() > 19) { k.Data[18] = 2; k.Data[19] = 0; } // FIX(sims) QA3: ObjectType = Person
            }
            out.push_back(std::move(k));
        }
    for (auto t : { "BHAV", "TPRP", "TREE" })
        for (auto k : byType(t)) if (!overridden(*k)) out.push_back(*k);
    for (auto k : byType("STR#")) if (k->ID != 200 && !overridden(*k)) out.push_back(*k);
    {
        RawIffChunk bs;
        bs.Type = "STR#"; bs.ID = 200; bs.Flags = 0x10; bs.Label = "bodystring";
        bs.Data = RawIff::MakeSTRFormat3(c.BodyStrings);
        out.push_back(std::move(bs));
    }
    for (auto& k : tpl) {
        static const std::set<std::string> handled = { "OBJD", "GLOB", "SLOT", "BHAV", "TPRP", "TREE", "STR#", "CTSS", "XXXX", "rsmp" };
        if (handled.count(k.Type) || overridden(k)) continue;
        if (k.Type == "BMP_" && k.ID >= 2002 && k.ID <= 2006) continue; // the template's own portraits
        out.push_back(k);
    }
    for (auto& e : c.Extra) if (!(e.Type == "CTSS" && e.ID == 2000)) out.push_back(e);
    {
        RawIffChunk k; k.Type = "CTSS"; k.ID = 2000; k.Flags = 0x10;
        k.Data = RawIff::MakeSTRFormat3({ c.FirstName, c.Bio });
        for (auto& e : c.Extra) if (e.Type == "CTSS" && e.ID == 2000) k = e; // a multi-language table (.FAM)
        out.push_back(std::move(k));
    }
    return out;
}

std::string TS1CharacterFiles::FileNameFor(const std::string& userName) {
    if (userName.size() > 4) return "User" + userName.substr(4) + ".iff";
    return userName + ".iff";
}

std::string TS1CharacterFiles::NextUserName(const std::string& charactersDir, const std::set<std::string>& usedNames,
                                            int preferred, int* number) {
    auto ok = [&](int n, std::string& nm) {
        char b[16]; snprintf(b, sizeof b, "user%05d", n);
        nm = b;
        if (usedNames.count(nm)) return false;
        return !FileExistsPath(charactersDir + "/" + FileNameFor(nm)) && !FileExistsPath(charactersDir + "/" + nm + ".iff");
    };
    std::string nm;
    if (preferred >= 0 && ok(preferred, nm)) { if (number) *number = preferred; return nm; }
    for (int n = 0; n < 100000; n++) if (ok(n, nm)) { if (number) *number = n; return nm; }
    return "";
}

uint32_t TS1CharacterFiles::NewGUID(const std::function<uint32_t()>& rnd, const std::set<uint32_t>& avoid) {
    auto content = Content::Get();
    auto world = content ? content->WorldObjects.get() : nullptr;
    for (int tries = 0; tries < 1000000; tries++) {
        uint32_t g = rnd ? rnd() : (((uint32_t)rand() << 16) ^ (uint32_t)rand());
        if (g == 0 || g == 0xFFFFFFFFu || avoid.count(g)) continue;
        if (world && world->Entries.find(g) != world->Entries.end()) continue;
        return g;
    }
    return 0;
}

std::string TS1CharacterFiles::WriteCharacter(const std::string& userDir, const NewCharacter& c, const std::string& userName,
                                              std::string* err) {
    std::vector<uint8_t> tplBytes;
    std::vector<RawIffChunk> tpl;
    if (!LoadObjectIff(c.TemplateGUID, tplBytes) || !RawIff::Parse(tplBytes, tpl)) {
        char b[64]; snprintf(b, sizeof b, "template %08X not found", c.TemplateGUID);
        if (err) *err = b;
        return "";
    }
    auto path = userDir + "/Characters/" + FileNameFor(userName);
    if (!RawIff::WriteFileAtomic(path, RawIff::Build(Clone(tpl, c, userName)))) {
        if (err) *err = "cannot write " + path;
        return "";
    }
    auto content = Content::Get();
    if (content && content->WorldObjects) {
        content->WorldObjects->AddStandaloneIff(path, true);
        content->WorldObjects->PersonGUIDs.insert(c.GUID);
    }
    return path;
}

bool TS1CharacterFiles::ReplaceBodyStrings(const std::string& path, const std::vector<std::string>& bodyStrings) {
    std::vector<uint8_t> bytes;
    std::vector<RawIffChunk> chunks;
    if (!RawIff::ReadFile(path, bytes) || !RawIff::Parse(bytes, chunks)) return false;
    auto bs = RawIff::Find(chunks, "STR#", 200);
    if (bs == nullptr) {
        chunks.push_back(RawIffChunk());
        bs = &chunks.back();
        bs->Type = "STR#"; bs->ID = 200; bs->Label = "bodystring";
    }
    bs->Data = RawIff::MakeSTRFormat3(bodyStrings);
    return RawIff::WriteFileAtomic(path, RawIff::Build(chunks));
}

std::string TS1CharacterFiles::CharacterFileOf(uint32_t guid) {
    auto content = Content::Get();
    if (content == nullptr || content->WorldObjects == nullptr) return "";
    auto it = content->WorldObjects->Entries.find(guid);
    if (it == content->WorldObjects->Entries.end() || it->second.Source == GameObjectSource::Far) return "";
    auto l = Lower(it->second.FileName);
    // only the neighbourhood's own characters (never TemplateNPCs or the install tree's People folder)
    if (l.find("/characters/") == std::string::npos && l.find("\\characters\\") == std::string::npos) return "";
    return it->second.FileName;
}

FSO::Files::Formats::IFF::Chunks::Neighbour* TS1CharacterFiles::AddNeighbour(TS1NeighborhoodProvider* prov, uint32_t guid,
                                                                             const std::string& userName, const std::vector<int16_t>& personData) {
    using FSO::Files::Formats::IFF::Chunks::Neighbour;
    if (prov == nullptr || prov->Neighbors == nullptr) return nullptr;
    auto nbrs = prov->Neighbors;
    int16_t id = 1;
    while (nbrs->NeighbourByID.count(id)) id++;
    auto n = new Neighbour();
    n->Unknown1 = 1;
    n->Version = 4;           // as every record of the shipped neighbourhoods (0xA0 bytes of person data)
    n->Name = userName;
    n->MysteryZero = 0;
    n->PersonMode = 5;
    n->PersonData = personData;
    n->PersonData.resize(88, 0);
    n->NeighbourID = id;
    n->GUID = guid;
    n->UnknownNegOne = -1;
    nbrs->Entries.emplace_back(n);
    nbrs->NeighbourByID[id] = n;
    nbrs->DefaultNeighbourByGUID[guid] = id;
    prov->DirtyAvatars.insert(guid);
    return n;
}

std::set<std::string> TS1CharacterFiles::NeighbourNames(TS1NeighborhoodProvider* prov) {
    std::set<std::string> out;
    if (prov == nullptr || prov->Neighbors == nullptr) return out;
    for (auto& n : prov->Neighbors->Entries) if (n->Unknown1 == 1) out.insert(Lower(n->Name));
    return out;
}

}}}
