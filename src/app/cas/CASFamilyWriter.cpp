/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims — Create-A-Family save (see CASFamilyWriter.h). TS1(sims).
#include "app/cas/CASFamilyWriter.h"
#include "content/TS1CharacterFiles.h"
#include "core/io.h"
#include "files/FAR1/FAR1Archive.h"
#include "files/formats/iff/chunks/FAMI.h"
#include "files/formats/iff/chunks/NBRS.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <memory>

namespace simapp { namespace cas {

using FSO::Files::Formats::IFF::Chunks::FAMI;
using FSO::Files::Formats::IFF::Chunks::NBRS;
using FSO::Files::Formats::IFF::Chunks::Neighbour;

static const uint32_t TEMPLATE_PERSON_GUID = 0x7FD96B54; // People\TemplatePerson.iff OBJD GUID

static void PutLE32(std::vector<uint8_t>& o, uint32_t v) { o.push_back((uint8_t)v); o.push_back((uint8_t)(v >> 8)); o.push_back((uint8_t)(v >> 16)); o.push_back((uint8_t)(v >> 24)); }
static void PutLE16(std::vector<uint8_t>& o, uint16_t v) { o.push_back((uint8_t)v); o.push_back((uint8_t)(v >> 8)); }
static void SetLE32(std::vector<uint8_t>& o, size_t at, uint32_t v) { if (at + 4 <= o.size()) { o[at] = (uint8_t)v; o[at + 1] = (uint8_t)(v >> 8); o[at + 2] = (uint8_t)(v >> 16); o[at + 3] = (uint8_t)(v >> 24); } }

CASFamilyWriter::CASFamilyWriter(const std::string& userDataDir, const std::string& objectsFar)
    : _UserData(userDataDir), _ObjectsFar(objectsFar) {}

// TS1(sims): the raw IFF helpers moved to src/content/TS1CharacterFiles.cpp (shared with the engine).
bool CASFamilyWriter::ReadFile(const std::string& path, std::vector<uint8_t>& out) { return FSO::Content::TS1::RawIff::ReadFile(path, out); }
bool CASFamilyWriter::WriteFileAtomic(const std::string& path, const std::vector<uint8_t>& data) { return FSO::Content::TS1::RawIff::WriteFileAtomic(path, data); }
bool CASFamilyWriter::ReadIff(const std::vector<uint8_t>& file, std::vector<CASRawChunk>& out) { return FSO::Content::TS1::RawIff::Parse(file, out); }
std::vector<uint8_t> CASFamilyWriter::WriteIff(const std::vector<CASRawChunk>& chunks) { return FSO::Content::TS1::RawIff::Build(chunks); }
std::vector<uint8_t> CASFamilyWriter::MakeSTRFormat3(const std::vector<std::string>& values) { return FSO::Content::TS1::RawIff::MakeSTRFormat3(values); }

std::vector<uint8_t> CASFamilyWriter::MakeBMP24(const Canvas& img) {
    std::vector<uint8_t> o;
    int w = img.Width, h = img.Height;
    int row = (w * 3 + 3) & ~3;
    uint32_t dataSize = (uint32_t)(row * h);
    o.push_back('B'); o.push_back('M');
    PutLE32(o, 54 + dataSize); PutLE32(o, 0); PutLE32(o, 54);
    PutLE32(o, 40); PutLE32(o, (uint32_t)w); PutLE32(o, (uint32_t)h);
    PutLE16(o, 1); PutLE16(o, 24); PutLE32(o, 0); PutLE32(o, dataSize);
    PutLE32(o, 2835); PutLE32(o, 2835); PutLE32(o, 0); PutLE32(o, 0);
    for (int y = h - 1; y >= 0; y--) {
        for (int x = 0; x < w; x++) {
            uint32_t c = img.Px[(size_t)y * w + x];
            o.push_back((uint8_t)(c >> 16)); o.push_back((uint8_t)(c >> 8)); o.push_back((uint8_t)c);
        }
        for (int p = w * 3; p < row; p++) o.push_back(0);
    }
    return o;
}

// ------------------------------------------------------------------------------------------- save

static bool FileExistsPath(const std::string& p) {
    FILE* f = fopen(p.c_str(), "rb");
    if (f) { fclose(f); return true; }
    return false;
}

CASSaveResult CASFamilyWriter::Save(const CASFamilyDesc& family, const std::vector<CASPortraits>& portraits) {
    CASSaveResult res;
    auto rnd = Random32 ? Random32 : []() { return ((uint32_t)rand() << 16) ^ (uint32_t)rand(); };
    if (family.Members.empty()) { res.Error = "The family has no members."; return res; }

    // ---- the template person
    std::vector<uint8_t> tplBytes;
    try {
        FSO::Files::FAR1::FAR1Archive far(_ObjectsFar, false);
        bool found = false;
        for (auto& e : far.GetAllFarEntries()) {
            std::string n = e.Filename;
            for (auto& ch : n) ch = (char)tolower((unsigned char)ch);
            if (n == "people\\templateperson.iff" || n == "people/templateperson.iff" || n == "templateperson.iff") { tplBytes = far.GetEntry(e, &found); break; }
        }
        if (!found) tplBytes.clear();
    } catch (...) { tplBytes.clear(); }
    std::vector<CASRawChunk> tpl;
    if (tplBytes.empty() || !ReadIff(tplBytes, tpl)) { res.Error = "TemplatePerson.iff not found in " + _ObjectsFar; return res; }

    // ---- the neighbourhood
    std::string nbhPath = _UserData + "/Neighborhood.iff";
    std::vector<uint8_t> nbhBytes;
    std::vector<CASRawChunk> nbh;
    if (!ReadFile(nbhPath, nbhBytes) || !ReadIff(nbhBytes, nbh)) { res.Error = "Cannot read " + nbhPath; return res; }
    NBRS nbrs;
    CASRawChunk* nbrsChunk = nullptr;
    std::set<int> famIDs;
    int32_t maxFamilyNumber = 0;
    for (auto& c : nbh) {
        if (c.Type == "NBRS" && !nbrsChunk) {
            nbrsChunk = &c;
            st::Reader r(c.Data, st::Endian::Big);
            try { nbrs.Read(nullptr, r); } catch (...) { res.Error = "Bad NBRS chunk"; return res; }
        } else if (c.Type == "FAMI") {
            famIDs.insert(c.ID);
            FAMI f;
            st::Reader r(c.Data, st::Endian::Big);
            try { f.Read(nullptr, r); maxFamilyNumber = std::max(maxFamilyNumber, f.FamilyNumber); } catch (...) {}
        }
    }
    if (!nbrsChunk) { res.Error = "Neighborhood.iff has no NBRS"; return res; }
    int16_t famID = 1;
    while (famIDs.count(famID)) famID++;
    res.FamilyID = famID;
    res.FamilyNumber = maxFamilyNumber + 1;

    // ---- per Sim: neighbour slot, user file number, GUID
    std::set<std::string> usedNames;
    std::set<uint32_t> usedGUIDs;
    for (auto& n : nbrs.Entries) if (n->Unknown1 == 1) { std::string l = n->Name; for (auto& ch : l) ch = (char)tolower((unsigned char)ch); usedNames.insert(l); usedGUIDs.insert(n->GUID); }
    int nextUser = 0;
    auto chars = _UserData + "/Characters/";
    std::vector<std::string> names;
    for (size_t i = 0; i < family.Members.size(); i++) {
        char nm[16];
        for (;; nextUser++) {
            snprintf(nm, sizeof nm, "user%05d", nextUser);
            char fn[32]; snprintf(fn, sizeof fn, "User%05d.iff", nextUser);
            if (usedNames.count(nm) || FileExistsPath(chars + fn) || FileExistsPath(chars + nm + ".iff")) continue;
            break;
        }
        names.push_back(nm);
        usedNames.insert(nm);
        nextUser++;
        uint32_t guid;
        do { guid = rnd(); } while (guid == 0 || guid == 0xFFFFFFFFu || usedGUIDs.count(guid) || (GUIDInUse && GUIDInUse(guid)));
        usedGUIDs.insert(guid);
        res.GUIDs.push_back(guid);
    }

    // ---- character files
    for (size_t i = 0; i < family.Members.size(); i++) {
        auto& sim = family.Members[i];
        std::vector<CASRawChunk> out;
        auto byType = [&](const char* type) {
            std::vector<const CASRawChunk*> v;
            for (auto& c : tpl) if (c.Type == type) v.push_back(&c);
            return v;
        };
        // OBJD, GLOB, SLOT first (the shipped order)
        for (auto t : { "OBJD", "GLOB", "SLOT" })
            for (auto c : byType(t)) {
                CASRawChunk k = *c;
                if (k.Type == "OBJD") {
                    SetLE32(k.Data, 28, res.GUIDs[i]);            // GUID1/GUID2
                    SetLE32(k.Data, 140, TEMPLATE_PERSON_GUID);   // OriginalGUID1/OriginalGUID2
                    k.Label = names[i] + " - " + sim.FirstName;
                }
                out.push_back(std::move(k));
            }
        for (auto t : { "BHAV", "TPRP", "TREE" })
            for (auto c : byType(t)) out.push_back(*c);
        for (auto c : byType("STR#")) if (c->ID != 200) out.push_back(*c);
        {
            CASRawChunk bs;
            bs.Type = "STR#"; bs.ID = 200; bs.Flags = 0x10; bs.Label = "bodystring";
            bs.Data = MakeSTRFormat3(sim.BodyStrings());
            out.push_back(std::move(bs));
        }
        // anything else the template carries (future-proof: BCON, TTAB, ...) except what we generate
        for (auto& c : tpl) {
            static const std::set<std::string> handled = { "OBJD", "GLOB", "SLOT", "BHAV", "TPRP", "TREE", "STR#", "CTSS", "BMP_", "XXXX", "rsmp" };
            if (!handled.count(c.Type)) out.push_back(c);
        }
        if (i < portraits.size()) {
            const CASPortraits& p = portraits[i];
            struct { const Canvas* Img; uint16_t ID; const char* Label; } bmps[] = {
                { &p.Faces, 2002, "faces" }, { &p.RelImages, 2003, "rel. images" }, { &p.Thumbnail, 2004, "thumbnail" },
                { &p.SpeechMed, 2005, "speech_med" }, { &p.SpeechLarge, 2006, "speech_large" } };
            for (auto& b : bmps) {
                if (b.Img->Width <= 0) continue;
                CASRawChunk k; k.Type = "BMP_"; k.ID = b.ID; k.Flags = 0x10; k.Label = b.Label;
                k.Data = MakeBMP24(*b.Img);
                out.push_back(std::move(k));
            }
        }
        {
            CASRawChunk k; k.Type = "CTSS"; k.ID = 2000; k.Flags = 0x10;
            k.Data = MakeSTRFormat3({ sim.FirstName, sim.Bio });
            out.push_back(std::move(k));
        }
        char fn[32]; snprintf(fn, sizeof fn, "User%s.iff", names[i].c_str() + 4);
        std::string path = chars + fn;
        if (!WriteFileAtomic(path, WriteIff(out))) { res.Error = "Cannot write " + path; return res; }
        res.Files.push_back(path);
    }

    // ---- neighbours
    std::set<int16_t> usedIDs;
    for (auto& kv : nbrs.NeighbourByID) usedIDs.insert(kv.first);
    for (size_t i = 0; i < family.Members.size(); i++) {
        int16_t id = 1;
        while (usedIDs.count(id)) id++;
        usedIDs.insert(id);
        auto n = new Neighbour();
        n->Unknown1 = 1;
        n->Version = 4;           // as every record of the shipped neighbourhoods (0xA0 bytes of person data)
        n->Name = names[i];
        n->MysteryZero = 0;
        n->PersonMode = 5;        // family members / townies (NPCs without data use 0)
        n->PersonData = family.Members[i].PersonData(famID);
        n->PersonData.resize(88, 0);
        n->NeighbourID = id;
        n->GUID = res.GUIDs[i];
        n->UnknownNegOne = -1;
        nbrs.Entries.emplace_back(n);
        nbrs.NeighbourByID[id] = n;
        nbrs.DefaultNeighbourByGUID[n->GUID] = id;
        res.NeighbourIDs.push_back(id);
    }
    {
        st::Writer w(st::Endian::Big);
        nbrs.Write(nullptr, w);
        nbrsChunk->Data = w.ToArray();
    }

    // ---- FAMI + FAMs
    FAMI fami;
    fami.HouseNumber = 0;          // in the family bin until moved in
    fami.FamilyNumber = res.FamilyNumber;
    fami.Budget = family.Budget;
    fami.NetWorth = 0;
    fami.FamilyFriends = 0;
    fami.Unknown = 0;              // as the shipped family-bin family (Pleasant)
    fami.FamilyGUIDs = res.GUIDs;
    // FIX(sims): the version of the neighbourhood's other families (shipped: 7), not FreeSims' 9
    fami.Version = 7;
    for (auto& c : nbh) if (c.Type == "FAMI" && c.Data.size() >= 8) { fami.Version = (uint32_t)(c.Data[4] | (c.Data[5] << 8) | (c.Data[6] << 16) | ((uint32_t)c.Data[7] << 24)); break; }
    CASRawChunk fc; fc.Type = "FAMI"; fc.ID = (uint16_t)famID; fc.Flags = 0x10;
    { st::Writer w(st::Endian::Big); fami.Write(nullptr, w); fc.Data = w.ToArray(); }
    CASRawChunk sc; sc.Type = "FAMs"; sc.ID = (uint16_t)famID; sc.Flags = 0x10;
    sc.Data = MakeSTRFormat3({ family.LastName });
    // insert after the last FAMI / FAMs
    auto insertAfterLast = [&](CASRawChunk&& c) {
        size_t at = nbh.size();
        for (size_t i = 0; i < nbh.size(); i++) if (nbh[i].Type == c.Type) at = i + 1;
        if (at == nbh.size()) { // none of this type: before the rsmp
            for (size_t i = 0; i < nbh.size(); i++) if (nbh[i].Type == "rsmp") { at = i; break; }
        }
        nbh.insert(nbh.begin() + (ptrdiff_t)at, std::move(c));
    };
    insertAfterLast(std::move(fc));
    insertAfterLast(std::move(sc));
    if (!WriteFileAtomic(nbhPath, WriteIff(nbh))) { res.Error = "Cannot write " + nbhPath; return res; }
    res.Ok = true;
    return res;
}

bool CASFamilyWriter::MoveIn(const std::string& userDataDir, int16_t familyID, int16_t houseNumber, std::string* error) {
    std::string path = userDataDir + "/Neighborhood.iff";
    std::vector<uint8_t> bytes;
    std::vector<CASRawChunk> nbh;
    if (!ReadFile(path, bytes) || !ReadIff(bytes, nbh)) { if (error) *error = "Cannot read " + path; return false; }
    bool done = false;
    for (auto& c : nbh) {
        if (c.Type != "FAMI") continue;
        FAMI f;
        st::Reader r(c.Data, st::Endian::Big);
        f.Read(nullptr, r);
        if (c.ID == (uint16_t)familyID) {
            f.HouseNumber = houseNumber;
            st::Writer w(st::Endian::Big);
            f.Write(nullptr, w);
            c.Data = w.ToArray();
            done = true;
        } else if (houseNumber > 0 && f.HouseNumber == houseNumber) {
            if (error) *error = "House is occupied";
            return false;
        }
    }
    if (!done) { if (error) *error = "No such family"; return false; }
    return WriteFileAtomic(path, WriteIff(nbh));
}

}}
