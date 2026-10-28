/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// TS1(sims): complete Neighborhood.iff writer; see the header.
#include "content/TS1NeighbourhoodWriter.h"
#include "content/TS1NeighbourProvider.h"
#include "files/formats/iff/IffRawFile.h"
#include "files/formats/iff/IffFieldEncode.h"
#include "files/formats/iff/IffFieldEncodeWriter.h"
#include "files/formats/iff/chunks/NBRS.h"
#include "files/formats/iff/chunks/NGBH.h"
#include "files/formats/iff/chunks/TTAT.h"
#include "files/formats/iff/chunks/FAMI.h"
#include "core/io.h"
#include "files/LoadProgress.h"
#include <algorithm>
#include <map>
#include <set>

using namespace FSO::Files::Formats::IFF;
using namespace FSO::Files::Formats::IFF::Chunks;
using FSO::Content::TS1::TS1NeighborhoodProvider;

namespace {

void Put16(std::vector<uint8_t>& o, uint16_t v) { o.push_back((uint8_t)v); o.push_back((uint8_t)(v >> 8)); }
void Put32(std::vector<uint8_t>& o, uint32_t v) { for (int k = 0; k < 4; k++) o.push_back((uint8_t)(v >> (8 * k))); }
uint32_t LE32(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
uint16_t LE16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

/// NBRS payload kept lossless: per slot the original bytes, and for valid neighbours the parsed fields.
struct NBRSRaw {
    std::vector<uint8_t> Header; // pad, version, "SRBN"
    struct Rel { int32_t KeyCount = 1; int32_t Key = 0; std::vector<int32_t> Values; };
    struct Slot {
        bool Valid = false;
        std::vector<uint8_t> Prefix;      // unknown1 .. person mode, as saved
        int32_t Version = 0, PersonMode = 0;
        std::vector<int16_t> PersonData;  // all of them (0xA0 or 0x200 bytes)
        int16_t NeighbourID = 0;
        uint32_t GUID = 0;
        int32_t NegOne = -1;
        std::vector<Rel> Rels;
        std::vector<uint8_t> Raw;
    };
    std::vector<Slot> Slots;
    std::vector<uint8_t> Tail;

    bool Parse(const std::vector<uint8_t>& d) {
        if (d.size() < 16) return false;
        Header.assign(d.begin(), d.begin() + 12);
        uint32_t count = LE32(d.data() + 12);
        size_t p = 16;
        auto need = [&](size_t n) { if (p + n > d.size()) throw std::runtime_error("NBRS truncated"); };
        try {
            for (uint32_t i = 0; i < count && p + 4 <= d.size(); i++) {
                Slot s;
                size_t start = p;
                int32_t u1 = (int32_t)LE32(d.data() + p); p += 4;
                if (u1 != 1) { s.Raw.assign(d.begin() + (long)start, d.begin() + (long)p); Slots.push_back(s); continue; }
                s.Valid = true;
                need(4); s.Version = (int32_t)LE32(d.data() + p); p += 4;
                if (s.Version == 0xA) p += 4;
                size_t ns = p;
                while (p < d.size() && d[p] != 0) p++;
                size_t nameLen = p - ns;
                p++;
                if (nameLen % 2 == 0) p++;
                p += 4; // mystery zero
                need(4); s.PersonMode = (int32_t)LE32(d.data() + p); p += 4;
                s.Prefix.assign(d.begin() + (long)start, d.begin() + (long)p);
                if (s.PersonMode > 0) {
                    int size = (s.Version == 0x4) ? 0xa0 : 0x200;
                    need((size_t)size);
                    for (int k = 0; k < size; k += 2) { s.PersonData.push_back((int16_t)LE16(d.data() + p)); p += 2; }
                }
                need(14);
                s.NeighbourID = (int16_t)LE16(d.data() + p); p += 2;
                s.GUID = LE32(d.data() + p); p += 4;
                s.NegOne = (int32_t)LE32(d.data() + p); p += 4;
                int32_t entries = (int32_t)LE32(d.data() + p); p += 4;
                for (int k = 0; k < entries; k++) {
                    Rel r;
                    need(12);
                    r.KeyCount = (int32_t)LE32(d.data() + p); p += 4;
                    r.Key = (int32_t)LE32(d.data() + p); p += 4;
                    int32_t vc = (int32_t)LE32(d.data() + p); p += 4;
                    need((size_t)std::max<int32_t>(0, vc) * 4);
                    for (int j = 0; j < vc; j++) { r.Values.push_back((int32_t)LE32(d.data() + p)); p += 4; }
                    s.Rels.push_back(r);
                }
                s.Raw.assign(d.begin() + (long)start, d.begin() + (long)p);
                Slots.push_back(std::move(s));
            }
        } catch (...) { return false; }
        Tail.assign(d.begin() + (long)std::min(p, d.size()), d.end());
        return true;
    }
    static std::vector<uint8_t> Encode(const Slot& s) {
        std::vector<uint8_t> o(s.Prefix);
        for (auto v : s.PersonData) Put16(o, (uint16_t)v);
        Put16(o, (uint16_t)s.NeighbourID);
        Put32(o, s.GUID);
        Put32(o, (uint32_t)s.NegOne);
        Put32(o, (uint32_t)s.Rels.size());
        for (auto& r : s.Rels) {
            Put32(o, (uint32_t)r.KeyCount); Put32(o, (uint32_t)r.Key); Put32(o, (uint32_t)r.Values.size());
            for (auto v : r.Values) Put32(o, (uint32_t)v);
        }
        return o;
    }
    /// A record for a neighbour the file does not have yet (same layout as Neighbour::Save, game pad bytes).
    static Slot FromNeighbour(const Neighbour& n, int32_t fileVersion) {
        Slot s;
        s.Valid = true;
        s.Version = n.Version;
        auto& o = s.Prefix;
        Put32(o, 1);
        Put32(o, (uint32_t)n.Version);
        if (n.Version == 0xA) Put32(o, (uint32_t)n.Unknown3);
        for (char c : n.Name) o.push_back((uint8_t)c);
        o.push_back(0);
        if (n.Name.size() % 2 == 0) o.push_back(0xA3);
        Put32(o, (uint32_t)n.MysteryZero);
        s.PersonMode = n.PersonMode != 0 ? n.PersonMode : (n.PersonData.empty() ? 0 : 5);
        Put32(o, (uint32_t)s.PersonMode);
        (void)fileVersion;
        if (s.PersonMode > 0) {
            int size = (n.Version == 0x4) ? 0xa0 : 0x200;
            s.PersonData.assign((size_t)size / 2, 0);
            for (size_t i = 0; i < n.PersonData.size() && i < s.PersonData.size(); i++) s.PersonData[i] = n.PersonData[i];
        }
        s.NeighbourID = n.NeighbourID;
        s.GUID = n.GUID;
        s.NegOne = n.UnknownNegOne;
        std::vector<int32_t> keys;
        for (auto& kv : n.Relationships) keys.push_back(kv.first);
        std::sort(keys.begin(), keys.end());
        for (auto k : keys) { Rel r; r.Key = k; r.Values.assign(n.Relationships.at(k).begin(), n.Relationships.at(k).end()); s.Rels.push_back(r); }
        s.Raw = Encode(s);
        return s;
    }
    std::vector<uint8_t> Write() const {
        std::vector<uint8_t> o(Header);
        Put32(o, (uint32_t)Slots.size());
        for (auto& s : Slots) o.insert(o.end(), s.Raw.begin(), s.Raw.end());
        o.insert(o.end(), Tail.begin(), Tail.end());
        return o;
    }
};

int UpdateNBRS(IffRawFile& file, NBRS* nbrs, std::vector<std::string>* log) {
    auto c = file.Find("NBRS", 1);
    if (!c || nbrs == nullptr) return 0;
    NBRSRaw nb;
    if (!nb.Parse(c->Data)) { if (log) log->push_back("NBRS not parsed: kept as saved"); return 0; }
    int changed = 0, added = 0;
    std::set<int16_t> seen;
    for (auto& s : nb.Slots) {
        if (!s.Valid) continue;
        seen.insert(s.NeighbourID);
        auto it = nbrs->NeighbourByID.find(s.NeighbourID);
        if (it == nbrs->NeighbourByID.end() || it->second == nullptr) continue;
        Neighbour* n = it->second;
        NBRSRaw::Slot ns = s;
        // FIX(sims) QA2: a version 4 record upgraded in memory (person data 80..87 set: Superstar fame) is written in the
        // newer layout (version 0xA, the int 9 after it, 0x200 bytes of person data), keeping its other bytes.
        if (s.Version == 0x4 && n->Version == 0xA && s.PersonMode > 0 && ns.Prefix.size() >= 8) {
            std::vector<uint8_t> pre(ns.Prefix.begin(), ns.Prefix.begin() + 4);
            Put32(pre, 0xAu);
            Put32(pre, (uint32_t)n->Unknown3);
            pre.insert(pre.end(), ns.Prefix.begin() + 8, ns.Prefix.end());
            ns.Prefix = pre;
            ns.Version = 0xA;
            ns.PersonData.resize(0x200 / 2, 0);
        }
        if (s.PersonMode > 0) for (size_t i = 0; i < n->PersonData.size() && i < ns.PersonData.size(); i++) ns.PersonData[i] = n->PersonData[i];
        ns.GUID = n->GUID;
        // relationships: values of saved keys updated in place, new keys appended (ascending), dropped keys removed
        std::vector<NBRSRaw::Rel> rels;
        std::set<int32_t> done;
        for (auto& r : s.Rels) {
            auto rit = n->Relationships.find(r.Key);
            if (rit == n->Relationships.end()) continue;
            auto nr = r;
            nr.Values.assign(rit->second.begin(), rit->second.end());
            rels.push_back(nr);
            done.insert(r.Key);
        }
        std::vector<int32_t> rest;
        for (auto& kv : n->Relationships) if (!done.count(kv.first)) rest.push_back(kv.first);
        std::sort(rest.begin(), rest.end());
        for (auto k : rest) { NBRSRaw::Rel r; r.Key = k; r.Values.assign(n->Relationships[k].begin(), n->Relationships[k].end()); rels.push_back(r); }
        ns.Rels = rels;
        auto bytes = NBRSRaw::Encode(ns);
        if (bytes != s.Raw) { s.Raw = bytes; changed++; }
    }
    // neighbours created at run time: TS1 keeps neighbour id N in slot N-1
    int32_t fileVersion = (int32_t)LE32(nb.Header.data() + 4);
    for (auto& kv : nbrs->NeighbourByID) {
        if (kv.second == nullptr || seen.count(kv.first) || kv.first <= 0 || kv.second->Unknown1 != 1) continue;
        size_t slot = (size_t)kv.first - 1;
        while (nb.Slots.size() <= slot) { NBRSRaw::Slot e; Put32(e.Raw, 0); nb.Slots.push_back(e); }
        if (nb.Slots[slot].Valid) continue;
        nb.Slots[slot] = NBRSRaw::FromNeighbour(*kv.second, fileVersion);
        added++;
    }
    if (changed + added == 0) return 0;
    c->Data = nb.Write();
    if (log) log->push_back("NBRS: " + std::to_string(changed) + " neighbour records changed, " + std::to_string(added) + " added");
    return changed + added;
}

int UpdateNGBH(IffRawFile& file, NGBH* ngbh, std::vector<std::string>* log) {
    auto c = file.Find("NGBH", 1);
    if (!c || ngbh == nullptr || c->Data.size() < 44) return 0;
    auto& d = c->Data;
    std::vector<uint8_t> o(d.begin(), d.begin() + 12);
    for (int i = 0; i < 16; i++) Put16(o, (uint16_t)ngbh->NeighborhoodData[i]);
    bool hasInventory = d.size() > 44 || !ngbh->InventoryByID.empty();
    if (hasInventory) {
        // saved order of the neighbours, then new ones ascending
        std::vector<int16_t> order;
        if (d.size() >= 48) {
            size_t p = 44;
            int32_t count = (int32_t)LE32(d.data() + p); p += 4;
            for (int i = 0; i < count && p + 10 <= d.size(); i++) {
                p += 4;
                int16_t id = (int16_t)LE16(d.data() + p); p += 2;
                int32_t n = (int32_t)LE32(d.data() + p); p += 4;
                p += (size_t)std::max<int32_t>(0, n) * 10;
                order.push_back(id);
            }
        }
        std::vector<int16_t> rest;
        for (auto& kv : ngbh->InventoryByID) if (std::find(order.begin(), order.end(), kv.first) == order.end()) rest.push_back(kv.first);
        std::sort(rest.begin(), rest.end());
        order.insert(order.end(), rest.begin(), rest.end());
        std::vector<int16_t> present;
        for (auto id : order) if (ngbh->InventoryByID.count(id)) present.push_back(id);
        Put32(o, (uint32_t)present.size());
        for (auto id : present) {
            auto& inv = ngbh->InventoryByID[id];
            Put32(o, 1);
            Put16(o, (uint16_t)id);
            Put32(o, (uint32_t)inv.size());
            for (auto& it : inv) { Put32(o, (uint32_t)it.Type); Put32(o, it.GUID); Put16(o, it.Count); }
        }
    }
    if (o == d) return 0;
    d = o;
    if (log) log->push_back("NGBH: neighbourhood data / inventories written");
    return 1;
}

int UpdateTATT(IffRawFile& file, TATT* tatt, std::vector<std::string>* log) {
    auto c = file.Find("TATT", 1);
    if (!c || tatt == nullptr || c->Data.size() < 13) return 0;
    auto& d = c->Data;
    // saved order and values
    std::vector<std::pair<uint32_t, std::vector<int16_t>>> saved;
    uint8_t compression = d[12];
    try {
        st::Reader io(d, st::Endian::Little);
        io.Seek(st::SeekOrigin::Begin, 13);
        std::unique_ptr<IOProxy> iop;
        if (compression != 1) iop.reset(new TTABNormal(&io)); else iop.reset(new IffFieldEncode(&io));
        int32_t total = iop->ReadInt32();
        for (int i = 0; i < total; i++) {
            uint32_t guid = (uint32_t)iop->ReadInt32();
            int32_t count = iop->ReadInt32();
            std::vector<int16_t> v((size_t)std::max<int32_t>(0, count));
            for (auto& x : v) x = iop->ReadInt16();
            saved.push_back(std::make_pair(guid, v));
        }
    } catch (...) { saved.clear(); }
    bool same = saved.size() == tatt->TypeAttributesByGUID.size();
    for (auto& kv : saved) {
        auto it = tatt->TypeAttributesByGUID.find(kv.first);
        if (it == tatt->TypeAttributesByGUID.end() || it->second != kv.second) { same = false; break; }
    }
    if (same) return 0;
    std::vector<uint32_t> order;
    for (auto& kv : saved) if (tatt->TypeAttributesByGUID.count(kv.first)) order.push_back(kv.first);
    std::vector<uint32_t> rest;
    for (auto& kv : tatt->TypeAttributesByGUID) if (std::find(order.begin(), order.end(), kv.first) == order.end()) rest.push_back(kv.first);
    std::sort(rest.begin(), rest.end());
    order.insert(order.end(), rest.begin(), rest.end());
    std::vector<uint8_t> o(d.begin(), d.begin() + 13);
    if (compression == 1) {
        IffFieldEncodeWriter w(1); // the stream starts 1 byte after the 12-byte header (no strings: parity unused)
        w.WriteInt32((int32_t)order.size());
        for (auto g : order) {
            auto& v = tatt->TypeAttributesByGUID[g];
            w.WriteInt32((int32_t)g);
            w.WriteInt32((int32_t)v.size());
            for (auto x : v) w.WriteInt16(x);
        }
        o.insert(o.end(), w.Data.begin(), w.Data.end());
    } else {
        Put32(o, (uint32_t)order.size());
        for (auto g : order) {
            auto& v = tatt->TypeAttributesByGUID[g];
            Put32(o, g); Put32(o, (uint32_t)v.size());
            for (auto x : v) Put16(o, (uint16_t)x);
        }
    }
    d = o;
    if (log) log->push_back("TATT: " + std::to_string(order.size()) + " type attribute sets written");
    return 1;
}

int UpdateFAMI(IffRawFile& file, TS1NeighborhoodProvider* prov, const std::vector<int16_t>& deleted, std::vector<std::string>* log) {
    int changed = 0;
    for (auto id : deleted) {
        if (file.Find("FAMI", (uint16_t)id)) { file.Remove("FAMI", (uint16_t)id); changed++; }
        if (file.Find("FAMs", (uint16_t)id)) file.Remove("FAMs", (uint16_t)id);
    }
    for (auto fami : prov->ListFamilies()) {
        if (fami == nullptr) continue;
        if (std::find(deleted.begin(), deleted.end(), (int16_t)fami->ChunkID) != deleted.end()) continue;
        auto c = file.Find("FAMI", fami->ChunkID);
        std::vector<uint8_t> o;
        if (c && c->Data.size() >= 40) {
            auto& d = c->Data;
            uint32_t n = LE32(d.data() + 36);
            size_t after = 40 + (size_t)n * 4;
            o.assign(d.begin(), d.begin() + 12); // pad, version (kept), "IMAF"
            Put32(o, (uint32_t)fami->HouseNumber); Put32(o, (uint32_t)fami->FamilyNumber); Put32(o, (uint32_t)fami->Budget);
            Put32(o, (uint32_t)fami->NetWorth); Put32(o, (uint32_t)fami->FamilyFriends); Put32(o, (uint32_t)fami->Unknown);
            Put32(o, (uint32_t)fami->FamilyGUIDs.size());
            for (auto g : fami->FamilyGUIDs) Put32(o, g);
            if (after <= d.size()) o.insert(o.end(), d.begin() + (long)after, d.end());
            if (o == d) continue;
            d = o;
        } else {
            // FIX(sims): a new family takes the version of the file's other families (FAMI::Write: 7 if unknown)
            if (fami->Version == 0 || fami->Version >= 9) {
                for (auto other : prov->ListFamilies()) {
                    if (other == nullptr || other == fami) continue;
                    auto oc = file.Find("FAMI", other->ChunkID);
                    if (oc && oc->Data.size() >= 8) { fami->Version = LE32(oc->Data.data() + 4); break; }
                }
            }
            st::Writer w(st::Endian::Little);
            fami->Write(prov->MainResource.get(), w);
            file.Set("FAMI", fami->ChunkID, w.ToArray(), fami->ChunkLabel);
        }
        changed++;
    }
    if (changed && log) log->push_back("FAMI: " + std::to_string(changed) + " family records written");
    return changed;
}

// FIX(sims): Makin' Magic family spells (TS1GameState::FamilySpells: the Magic Controller's attributes 0..5 per
// family, see VMTS1Travel::LoadFamilySpells). Where the original game keeps them is not known (Sims.exe is
// SafeDisc-encrypted; no Makin' Magic save with spells in the data; FreeSO leaves Generic Sims Call 43 empty). They are
// written as a private chunk "FSPL" (id = family number; u32 version 1, u16 count, count x s16) which the original's
// IFF loader skips like any unknown chunk type; the TS1 chunks stay untouched.
int UpdateFamilySpells(IffRawFile& file, TS1NeighborhoodProvider* prov, const std::vector<int16_t>& deleted, std::vector<std::string>* log) {
    int changed = 0;
    for (auto id : deleted) if (file.Find("FSPL", (uint16_t)id)) { file.Remove("FSPL", (uint16_t)id); changed++; }
    for (auto& kv : prov->GameState.FamilySpells) {
        if (kv.first <= 0 || std::find(deleted.begin(), deleted.end(), kv.first) != deleted.end()) continue;
        std::vector<uint8_t> o;
        Put32(o, 1);
        Put16(o, (uint16_t)kv.second.size());
        for (auto v : kv.second) Put16(o, (uint16_t)v);
        auto c = file.Find("FSPL", (uint16_t)kv.first);
        if (c && c->Data == o) continue;
        file.Set("FSPL", (uint16_t)kv.first, o, "Family Spells");
        changed++;
    }
    if (changed && log) log->push_back("FSPL: " + std::to_string(changed) + " family spell sets written");
    return changed;
}

} // namespace

namespace FSO { namespace Content { namespace TS1 {

void TS1NeighbourhoodWriter::LoadFamilySpells(TS1NeighborhoodProvider* prov, const std::string& path) {
    if (prov == nullptr) return;
    IffRawFile file;
    if (!file.Load(path, nullptr)) return;
    for (auto& c : file.Chunks) {
        if (c.Type != "FSPL" || c.Data.size() < 6 || LE32(c.Data.data()) != 1) continue;
        size_t n = LE16(c.Data.data() + 4);
        std::vector<int16_t> v;
        for (size_t i = 0; i < n && 6 + i * 2 + 2 <= c.Data.size(); i++) v.push_back((int16_t)LE16(c.Data.data() + 6 + i * 2));
        prov->GameState.FamilySpells[(int16_t)c.ID] = v;
    }
}

bool TS1NeighbourhoodWriter::Save(TS1NeighborhoodProvider* prov, const std::string& path, const std::vector<int16_t>& deletedFamilies,
                                  std::string* error, std::vector<std::string>* log) {
    if (prov == nullptr || prov->MainResource == nullptr) { if (error) *error = "no neighbourhood"; return false; }
    // ADDED(sims) save progress (files/LoadProgress.h): the app's "Saving..." overlay follows the stages
    auto progress = [](size_t done) { FSO::Files::LoadProgress::Progress("save: neighbourhood", done, 7); };
    progress(0);
    IffRawFile file;
    std::string err;
    if (!file.Load(path, &err)) { if (error) *error = err; return false; }
    progress(1);
    int changed = 0;
    changed += UpdateNBRS(file, prov->Neighbors, log);
    progress(2);
    changed += UpdateNGBH(file, prov->Neighborhood, log);
    progress(3);
    changed += UpdateTATT(file, prov->TypeAttributes, log);
    progress(4);
    changed += UpdateFAMI(file, prov, deletedFamilies, log);
    changed += UpdateFamilySpells(file, prov, deletedFamilies, log); // FIX(sims)
    progress(5);
    if (changed == 0) { if (log) log->push_back("neighbourhood unchanged"); progress(7); return true; }
    if (!file.Save(path, &err)) { if (error) *error = err; return false; }
    progress(7);
    return true;
}

}}}
