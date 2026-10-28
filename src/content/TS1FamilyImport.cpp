/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// TS1(sims): see TS1FamilyImport.h.
#include "content/TS1FamilyImport.h"
#include "content/ContentManager.h"
#include "content/TS1NeighbourProvider.h"
#include "content/WorldContentFiles.h"
#include "files/formats/iff/chunks/NBRS.h"
#include "core/io.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>

namespace FSO { namespace Content { namespace TS1 {

using FSO::Files::Formats::IFF::Chunks::NBRS;
using FSO::Files::Formats::IFF::Chunks::Neighbour;

static std::string Lower(std::string s) { for (auto& c : s) c = (char)tolower((unsigned char)c); return s; }
static int32_t LE32(const std::vector<uint8_t>& d, size_t at) { return at + 4 <= d.size() ? (int32_t)(d[at] | (d[at + 1] << 8) | (d[at + 2] << 16) | ((uint32_t)d[at + 3] << 24)) : 0; }
static void SetLE32(std::vector<uint8_t>& d, size_t at, int32_t v) { if (at + 4 <= d.size()) { d[at] = (uint8_t)v; d[at + 1] = (uint8_t)(v >> 8); d[at + 2] = (uint8_t)(v >> 16); d[at + 3] = (uint8_t)(v >> 24); } }
static bool Exists(const std::string& p) { FILE* f = fopen(p.c_str(), "rb"); if (f) { fclose(f); return true; } return false; }
static std::string Pad2(int32_t n) { char b[16]; snprintf(b, sizeof b, "%02d", n); return b; }

static bool ParseNBRS(const std::vector<uint8_t>& data, NBRS& out) {
    st::Reader r(data, st::Endian::Little);
    try { out.Read(nullptr, r); } catch (...) { return false; }
    return true;
}

bool TS1FamilyImport::Read(const std::string& path, TS1FamFile& fam, std::string* err) {
    std::vector<uint8_t> bytes;
    std::vector<RawIffChunk> chunks;
    if (!RawIff::ReadFile(path, bytes) || !RawIff::Parse(bytes, chunks)) { if (err) *err = "cannot read " + path; return false; }
    fam = TS1FamFile();
    fam.Path = path;
    const RawIffChunk* fami = nullptr;
    for (auto& c : chunks) if (c.Type == "FAMI") { fami = &c; break; }
    if (fami == nullptr || fami->Data.size() < 40) { if (err) *err = "no FAMI in " + path; return false; }
    fam.FamilyID = (int16_t)fami->ID;
    fam.FAMIData = fami->Data;
    fam.HouseNumber = LE32(fami->Data, 12);
    fam.Budget = LE32(fami->Data, 20);
    int32_t n = LE32(fami->Data, 36);
    for (int32_t i = 0; i < n && 40 + 4 * (size_t)i + 4 <= fami->Data.size(); i++) fam.GUIDs.push_back((uint32_t)LE32(fami->Data, 40 + 4 * (size_t)i));
    NBRS nbrs;
    bool haveNbrs = false;
    for (auto& c : chunks) {
        if (c.Type == "FAMs" && c.ID == fami->ID) { auto v = RawIff::ReadSTRValues(c.Data); if (!v.empty()) fam.Name = v[0]; }
        if ((c.Type == "FAMs" || c.Type == "FAMh") && c.ID == fami->ID) fam.FamilyChunks.push_back(c);
        if (c.Type == "NBRS" && !haveNbrs) haveNbrs = ParseNBRS(c.Data, nbrs);
    }
    if (!haveNbrs) { if (err) *err = "no NBRS in " + path; return false; }
    for (auto& e : nbrs.Entries) if (e->Unknown1 == 1) fam.OldNeighbourGUIDs.push_back({ e->NeighbourID, e->GUID });
    for (auto guid : fam.GUIDs) {
        auto it = nbrs.DefaultNeighbourByGUID.find(guid);
        if (it == nbrs.DefaultNeighbourByGUID.end()) continue;
        auto ne = nbrs.NeighbourByID[it->second];
        TS1FamFile::Member m;
        m.GUID = guid;
        m.OldNeighbourID = ne->NeighbourID;
        m.UserName = Lower(ne->Name);
        m.PersonMode = ne->PersonMode;
        m.PersonData = ne->PersonData;
        for (auto& kv : ne->Relationships) m.Relationships.push_back(kv);
        std::sort(m.Relationships.begin(), m.Relationships.end(), [](const std::pair<int32_t, std::vector<int16_t>>& a, const std::pair<int32_t, std::vector<int16_t>>& b) { return a.first < b.first; });
        uint16_t id = (uint16_t)ne->NeighbourID;
        for (auto& c : chunks) {
            if (c.Type == "uChr" && c.ID == id) m.BodyStrings = RawIff::ReadSTRValues(c.Data);
            if (c.Type == "BMP_" && c.ID == id) m.Thumbnail = c;
            if (c.Type == "CTSS" && c.ID == (uint16_t)(2000 + id)) m.CTSS = c;
        }
        // the plain CTSS #id: name, bio with the "Filename:" line
        if (auto c = RawIff::Find(chunks, "CTSS", id)) {
            auto v = RawIff::ReadSTRValues(c->Data);
            if (v.size() > 0) m.FirstName = v[0];
            if (v.size() > 1) { m.Bio = v[1]; auto f = m.Bio.find("\r\n\nFilename:"); if (f != std::string::npos) m.Bio = m.Bio.substr(0, f + 2); }
            if (m.CTSS.Type.empty()) { m.CTSS = *c; }
        }
        if (m.BodyStrings.empty()) continue;
        fam.Members.push_back(std::move(m));
    }
    if (fam.HouseNumber > 0) {
        static const std::set<std::string> family = { "FAMI", "FAMs", "FAMh", "EXPi", "NBRS", "uChr", "XXXX", "rsmp" };
        for (auto& c : chunks) {
            if (family.count(c.Type)) continue;
            if (c.Type == "CTSS" && c.ID != 0) continue;       // the members' names (the house has none of its own)
            if (c.Type == "BMP_" && c.ID < 512) continue;      // the members' thumbnails
            fam.HouseChunks.push_back(c);
        }
    }
    return !fam.Members.empty();
}

TS1FamilyImport::Result TS1FamilyImport::Import(const std::string& userDir, const TS1FamFile& fam, const Options& opts) {
    Result res;
    auto content = Content::Get();
    auto prov = content ? content->Neighborhood.get() : nullptr;
    std::string nbhPath = userDir + "/Neighborhood.iff";
    std::vector<uint8_t> nbhBytes;
    std::vector<RawIffChunk> nbh;
    if (!RawIff::ReadFile(nbhPath, nbhBytes) || !RawIff::Parse(nbhBytes, nbh)) { res.Error = "cannot read " + nbhPath; return res; }
    RawIffChunk* nbrsChunk = nullptr;
    NBRS nbrs;
    std::set<int> famIDs;
    std::set<int32_t> occupied;
    int32_t maxFamilyNumber = 0;
    for (auto& c : nbh) {
        if (c.Type == "NBRS" && nbrsChunk == nullptr) { nbrsChunk = &c; if (!ParseNBRS(c.Data, nbrs)) { res.Error = "bad NBRS"; return res; } }
        if (c.Type == "FAMI") {
            famIDs.insert(c.ID);
            maxFamilyNumber = std::max(maxFamilyNumber, LE32(c.Data, 16));
            if (LE32(c.Data, 12) > 0 && LE32(c.Data, 36) > 0) occupied.insert(LE32(c.Data, 12));
        }
    }
    if (nbrsChunk == nullptr) { res.Error = "Neighborhood.iff has no NBRS"; return res; }
    for (auto& m : fam.Members)
        if (nbrs.DefaultNeighbourByGUID.count(m.GUID)) { res.Skipped = true; res.Ok = true; res.Error = "already in the neighbourhood"; return res; }

    // ---- family id / house
    int16_t famID = fam.FamilyID;
    if (famID <= 0 || famIDs.count(famID)) { famID = 1; while (famIDs.count(famID)) famID++; }
    res.FamilyID = famID;
    int32_t house = 0;
    if (opts.MoveIntoHouse && fam.HouseNumber > 0 && !occupied.count(fam.HouseNumber)) house = fam.HouseNumber;
    res.HouseNumber = house;

    // ---- neighbour ids: lowest free per member
    std::set<int16_t> used;
    for (auto& kv : nbrs.NeighbourByID) used.insert(kv.first);
    std::map<int32_t, int16_t> idMap; // old neighbour id -> new
    for (auto& m : fam.Members) { int16_t id = 1; while (used.count(id)) id++; used.insert(id); idMap[m.OldNeighbourID] = id; }
    for (auto& og : fam.OldNeighbourGUIDs) // others: the same Sim (by GUID) in this neighbourhood
        if (!idMap.count(og.first)) { auto it = nbrs.DefaultNeighbourByGUID.find(og.second); if (it != nbrs.DefaultNeighbourByGUID.end()) idMap[og.first] = it->second; }

    // ---- character files
    std::set<std::string> names;
    for (auto& e : nbrs.Entries) if (e->Unknown1 == 1) names.insert(Lower(e->Name));
    std::set<uint32_t> guidsUsed;
    for (auto& e : nbrs.Entries) if (e->Unknown1 == 1) guidsUsed.insert(e->GUID);
    for (auto& m : fam.Members) {
        TS1CharacterFiles::NewCharacter nc;
        nc.GUID = m.GUID;
        if (guidsUsed.count(nc.GUID) || (content && content->WorldObjects && content->WorldObjects->Entries.count(nc.GUID)))
            nc.GUID = TS1CharacterFiles::NewGUID(nullptr, guidsUsed);
        guidsUsed.insert(nc.GUID);
        auto kind = m.BodyStrings[0];
        nc.TemplateGUID = (kind == "dog") ? TS1CharacterFiles::TEMPLATE_DOG_GUID : (kind == "kat" || kind == "cat") ? TS1CharacterFiles::TEMPLATE_CAT_GUID
                                                                               : TS1CharacterFiles::TEMPLATE_PERSON_GUID;
        nc.FirstName = m.FirstName;
        nc.Bio = m.Bio;
        nc.BodyStrings = m.BodyStrings;
        if (!m.CTSS.Type.empty()) { RawIffChunk c = m.CTSS; c.ID = 2000; c.Label = ""; nc.Extra.push_back(c); }
        if (!m.Thumbnail.Type.empty()) { RawIffChunk b = m.Thumbnail; b.ID = 2004; b.Label = "thumbnail"; nc.Extra.push_back(b); }
        int preferred = -1;
        if (m.UserName.size() == 9 && m.UserName.compare(0, 4, "user") == 0) preferred = atoi(m.UserName.c_str() + 4);
        auto userName = TS1CharacterFiles::NextUserName(userDir + "/Characters", names, preferred);
        names.insert(userName);
        std::string err;
        auto path = TS1CharacterFiles::WriteCharacter(userDir, nc, userName, &err);
        if (path.empty()) { res.Error = err; return res; }
        res.Files.push_back(path);
        res.GUIDs.push_back(nc.GUID);
        // the neighbour record
        auto n = new Neighbour();
        n->Unknown1 = 1;
        n->Version = 4;
        n->Name = userName;
        n->PersonMode = m.PersonMode;
        n->PersonData = m.PersonData;
        n->PersonData.resize(88, 0);
        n->PersonData[61] = famID; // 'family number'
        n->NeighbourID = idMap[m.OldNeighbourID];
        n->GUID = nc.GUID;
        n->UnknownNegOne = -1;
        for (auto& r : m.Relationships) { auto it = idMap.find(r.first); if (it != idMap.end()) n->Relationships[it->second] = r.second; }
        nbrs.Entries.emplace_back(n);
        nbrs.NeighbourByID[n->NeighbourID] = n;
        nbrs.DefaultNeighbourByGUID[n->GUID] = n->NeighbourID;
        res.NeighbourIDs.push_back(n->NeighbourID);
    }
    { st::Writer w(st::Endian::Little); nbrs.Write(nullptr, w); nbrsChunk->Data = w.ToArray(); }

    // ---- FAMI (the .FAM's own record, patched), FAMs, FAMh
    RawIffChunk fc; fc.Type = "FAMI"; fc.ID = (uint16_t)famID; fc.Data = fam.FAMIData;
    SetLE32(fc.Data, 12, house);
    SetLE32(fc.Data, 16, maxFamilyNumber + 1);
    for (size_t i = 0; i < res.GUIDs.size(); i++) SetLE32(fc.Data, 40 + 4 * i, (int32_t)res.GUIDs[i]);
    std::vector<RawIffChunk> add = { fc };
    for (auto c : fam.FamilyChunks) { c.ID = (uint16_t)famID; add.push_back(c); }
    for (auto& c : add) {
        size_t at = nbh.size();
        for (size_t i = 0; i < nbh.size(); i++) if (nbh[i].Type == c.Type) at = i + 1;
        if (at == nbh.size()) for (size_t i = 0; i < nbh.size(); i++) if (nbh[i].Type == "rsmp") { at = i; break; }
        nbh.insert(nbh.begin() + (ptrdiff_t)at, c);
    }
    if (!RawIff::WriteFileAtomic(nbhPath, RawIff::Build(nbh))) { res.Error = "cannot write " + nbhPath; return res; }

    // ---- the house
    if (house > 0 && opts.WriteHouse && !fam.HouseChunks.empty()) {
        auto rel = "Houses/House" + Pad2(house) + ".iff";
        // the neighbourhood's own saved house wins; a template house (TemplateCommunity/House56.iff ...) does not:
        // Maxis saved those with other GUIDs for the family members, the .FAM's house refers to these Sims.
        bool have = Exists(userDir + "/" + rel);
        if (!have) {
            auto path = userDir + "/" + rel;
            if (RawIff::WriteFileAtomic(path, RawIff::Build(fam.HouseChunks))) res.Files.push_back(path);
        }
    }
    res.Ok = true;
    return res;
}

std::vector<TS1FamilyImport::Result> TS1FamilyImport::InstallTemplates(const std::string& userDir, const std::string& installRoot) {
    std::vector<Result> out;
    auto dir = installRoot + "/TemplateFamilyUnleashed";
    auto files = WorldContentFiles::List(dir);
    if (files.empty()) // builds without dirent: the six families the Unleashed installer ships
        for (auto n : { "Burb_11.FAM", "Charming_13.FAM", "Goth Sr_12.FAM", "Hick_10.FAM", "Kat_6.FAM", "Strays_4000.FAM" })
            if (Exists(dir + "/" + n)) files.push_back(n);
    std::sort(files.begin(), files.end());
    for (auto& f : files) {
        if (Lower(f).size() < 4 || Lower(f).substr(Lower(f).size() - 4) != ".fam") continue;
        TS1FamFile fam;
        Result r;
        if (!Read(dir + "/" + f, fam, &r.Error)) { out.push_back(r); continue; }
        out.push_back(Import(userDir, fam, Options()));
    }
    return out;
}

}}}
