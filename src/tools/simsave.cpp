/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// simsave — TS1(sims) lot saving checks (see src/simantics/STUBS_save.md).
//   simsave raw  <file.iff>...                        IffRawFile parse + write == original bytes (rsmp regenerated)
//   simsave objm <gameDataDir> <cacheDir> <House.iff>... decode every OBJM record and re-encode it == original bytes
#include "content/ContentManager.h"
#include "files/formats/iff/IffFile.h"
#include "files/formats/iff/IffRawFile.h"
#include "files/formats/iff/IffFieldEncode.h"
#include "files/formats/iff/chunks/OBJM.h"
#include "files/formats/iff/chunks/OBJT.h"
#include "files/formats/iff/chunks/OBJMWriter.h"
#include "files/formats/iff/chunks/BHAV.h"

#include "simantics/VM.h"
#include "simantics/VMContext.h"
#include "simantics/VMArchitecture.h"
#include "simantics/VMClock.h"
#include "simantics/utils/VMWorldActivator.h"
#include "simantics/utils/VMTS1FamilyLoader.h"
#include "simantics/utils/VMArchitectureTools.h"
#include "simantics/utils/VMTS1LotSaver.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/entities/VMMultitileGroup.h"
#include "simantics/VMPlacementResult.h"
#include "simantics/model/VMStackObjectVariable.h"
#include "simantics/model/VMPersonDataVariable.h"
#include "simantics/model/VMMotive.h"
#include "content/TS1NeighbourProvider.h"
#include "content/TS1NeighbourhoodWriter.h"
#include "files/formats/iff/chunks/ARRY.h"
#include "files/formats/iff/chunks/ARRYWriter.h"
#include "world/World.h"
#include "core/png.h"
#include <map>
#include <set>
#include <memory>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace FSO::Files::Formats::IFF;
using namespace FSO::Files::Formats::IFF::Chunks;

static int CmdRaw(int argc, char** argv) {
    int bad = 0;
    for (int i = 2; i < argc; i++) {
        std::vector<uint8_t> orig;
        if (!st::ReadAllBytes(argv[i], orig)) { printf("%s: cannot read\n", argv[i]); bad++; continue; }
        IffRawFile raw;
        std::string err;
        if (!raw.Parse(orig, &err)) { printf("%s: %s\n", argv[i], err.c_str()); bad++; continue; }
        auto out = raw.Write();
        size_t diff = 0, first = (size_t)-1;
        for (size_t k = 0; k < std::min(out.size(), orig.size()); k++) if (out[k] != orig[k]) { diff++; if (first == (size_t)-1) first = k; }
        bool same = diff == 0 && out.size() == orig.size();
        if (!same) bad++;
        printf("%-50s %zu chunks, rsmp %s: %s (%zu vs %zu bytes, %zu differ, first at %zd)\n", argv[i], raw.Chunks.size(),
               raw.HadRsmp ? "yes" : "no", same ? "IDENTICAL" : "DIFFERENT", out.size(), orig.size(), diff, first == (size_t)-1 ? (ssize_t)-1 : (ssize_t)first);
    }
    return bad ? 1 : 0;
}

/// Decodes one raw OBJM record; returns the bits after the last decoded field.
static bool DecodeRecord(const OBJMRawChunk& objm, size_t i, OBJT* objt, OBJMInstance& inst, std::string& tail, std::string& err) {
    auto& rec = objm.Records[i];
    try {
        st::Reader io(rec.Data.data(), rec.Data.size(), st::Endian::Little);
        IffFieldEncode iop(&io, (rec.Offset & 1) == 1);
        auto content = FSO::Content::Content::Get();
        inst.Read(objm.Version, iop, [&](int16_t objID) {
            OBJMResource r;
            uint16_t type = 0;
            for (auto& p : objm.Table) if (p.first == (uint16_t)objID) { type = p.second; break; }
            for (auto& e : objt->Entries) if (e.TypeID == type) { r.OBJT = &e; break; }
            if (r.OBJT && content) { auto res = content->WorldObjects->Get(r.OBJT->GUID, false); if (res) r.OBJD = res->OBJ; }
            return r;
        });
        tail = iop.BitDebugTil((int64_t)rec.Data.size());
        return true;
    } catch (std::exception& e) { err = e.what(); return false; }
}

static int CmdObjm(int argc, char** argv) {
    FSO::Content::Content::Init(argv[2], argv[3]);
    int bad = 0;
    for (int i = 4; i < argc; i++) {
        IffRawFile raw;
        std::string err;
        if (!raw.Load(argv[i], &err)) { printf("%s: %s\n", argv[i], err.c_str()); bad++; continue; }
        auto ch = raw.Find("ObjM", 1);
        IffFile iff(argv[i]);
        auto objt = iff.Get<OBJT>(0);
        if (!ch || !objt) { printf("%s: no ObjM#1/objt#0\n", argv[i]); bad++; continue; }
        OBJMRawChunk objm;
        if (!objm.Parse(ch->Data, err)) { printf("%s: %s\n", argv[i], err.c_str()); bad++; continue; }
        int ok = 0, differ = 0, fail = 0, tails = 0, oddOff = 0, zeroSlots = 0;
        std::vector<OBJMRecordOut> outs;
        for (size_t r = 0; r < objm.Records.size(); r++) {
            OBJMInstance inst; std::string tail;
            if (objm.Records[r].Offset & 1) oddOff++;
            if (!DecodeRecord(objm, r, objt, inst, tail, err)) { fail++; if (fail <= 3) printf("  record %zu (id %d): decode failed: %s\n", r, objm.Table[r].first, err.c_str()); continue; }
            // trailing zero bits that only pad the last byte are not data
            std::string bits; for (char c : tail) if (c == '0' || c == '1') bits += c;
            bool hasLinked = true;
            // slot count 0 (no linked slot) is visible as LinkedSlot == {0,0} with no slots; try both encodings
            IffFieldEncodeWriter w(objm.Records[r].Offset);
            std::string keep = bits;
            WriteOBJMInstance(inst, objm.Version, w, true, "");
            size_t used = w.Data.size() * 8 - (w.BitPos() ? 8 - w.BitPos() : 0);
            (void)used;
            // remaining bits: write them unchanged
            IffFieldEncodeWriter w2(objm.Records[r].Offset);
            WriteOBJMInstance(inst, objm.Version, w2, hasLinked, keep);
            if (w2.Data != objm.Records[r].Data) {
                IffFieldEncodeWriter w3(objm.Records[r].Offset);
                WriteOBJMInstance(inst, objm.Version, w3, false, keep);
                if (w3.Data == objm.Records[r].Data) { zeroSlots++; ok++; }
                else {
                    differ++;
                    if (differ <= 3) {
                        size_t k = 0; while (k < w2.Data.size() && k < objm.Records[r].Data.size() && w2.Data[k] == objm.Records[r].Data[k]) k++;
                        printf("  record %zu (id %d, %s): re-encoded %zu bytes vs %zu, first difference at byte %zu, tail bits %zu\n", r, objm.Table[r].first,
                               inst.OBJT ? inst.OBJT->Name.c_str() : "?", w2.Data.size(), objm.Records[r].Data.size(), k, bits.size());
                    }
                }
            } else ok++;
            bool nonzeroTail = bits.find('1') != std::string::npos;
            if (nonzeroTail || bits.size() >= 8) { tails++; if (tails <= 3) printf("  record %zu (%s): %zu unhandled bits%s\n", r, inst.OBJT ? inst.OBJT->Name.c_str() : "?", bits.size(), nonzeroTail ? " (non-zero)" : ""); }
            OBJMRecordOut o; o.ObjectID = objm.Table[r].first; o.TypeID = objm.Table[r].second; o.Instance = inst; o.UnhandledBits = keep;
            outs.push_back(o);
        }
        // whole chunk re-encoded
        bool chunkSame = false;
        if (fail == 0) {
            auto data = WriteOBJMChunk(objm.Pad, objm.Version, outs, objm.Tail);
            chunkSame = data == ch->Data;
        }
        printf("%-50s v%X %zu records: %d identical, %d differ, %d undecodable, %d with data after the known fields, %d odd offsets, %d slotless; chunk %s; tail %zu bytes\n",
               argv[i], objm.Version, objm.Records.size(), ok, differ, fail, tails, oddOff, zeroSlots, chunkSame ? "IDENTICAL" : "different", objm.Tail.size());
        if (differ || fail || !chunkSame) bad++;
    }
    return bad ? 1 : 0;
}

static int CmdDump(int argc, char** argv) {
    FSO::Content::Content::Init(argv[2], argv[3]);
    IffRawFile raw; std::string err;
    if (!raw.Load(argv[4], &err)) { printf("%s\n", err.c_str()); return 1; }
    IffFile iff(argv[4]);
    auto objt = iff.Get<OBJT>(0);
    OBJMRawChunk objm;
    if (!objm.Parse(raw.Find("ObjM", 1)->Data, err)) { printf("%s\n", err.c_str()); return 1; }
    printf("objt entries:\n");
    for (auto& e : objt->Entries) {
        printf("  type %d guid %08X name '%s' objdtype %d unk1a %d inittv %d unk2a %d maintv %d", e.TypeID, e.GUID, e.Name.c_str(), (int)e.OBJDType, (int)e.Unknown1a, e.InitTreeVersion, e.Unknown2a, e.MainTreeVersion);
        auto res = FSO::Content::Content::Get()->WorldObjects->Get(e.GUID, false);
        if (res && res->OBJ) {
            auto o = res->OBJ;
            auto bi = res->Resource->Get<FSO::Files::Formats::IFF::Chunks::BHAV>(o->BHAV_Init);
            auto bm = res->Resource->Get<FSO::Files::Formats::IFF::Chunks::BHAV>(o->BHAV_MainID);
            printf(" | objd v%u attrs %d objver %d init %d main %d stack %d dyn %d", o->Version, o->NumAttributes, o->ObjectVersion, o->BHAV_Init, o->BHAV_MainID, o->StackSize, o->NumDynamicSprites);
            if (bi) printf(" initbhav f%u ", (unsigned)bi->Flags);
            if (bm) printf(" mainbhav f%u ", (unsigned)bm->Flags);
        }
        printf("\n");
    }
    for (size_t r = 0; r < objm.Records.size(); r++) {
        OBJMInstance in; std::string tail;
        if (!DecodeRecord(objm, r, objt, in, tail, err)) { printf("#%zu fail %s\n", r, err.c_str()); continue; }
        auto& d = in.ObjectData;
        printf("#%zu id %d type %d %-22s pos %d,%d,L%d fp[%d %d %d %d] unk %d attrs %zu dir %d cont %d slot %d flags %04X oid %d parent %d extra[%d %d %d %d %d] stack %zu/%d rel %zu linked(%d,%d) slots %zu dsf %zu mt %s portal %zu unkint %d\n",
            r, objm.Table[r].first, objm.Table[r].second, in.OBJT ? in.OBJT->Name.c_str() : "?", in.X, in.Y, in.Level,
            in.Footprint.MinY, in.Footprint.MinX, in.Footprint.MaxY, in.Footprint.MaxX, in.UnknownData, in.Attributes.size(), d[1], d[2], d[3], (uint16_t)d[8], d[11], d[26],
            in.ObjectDataExtra[0], in.ObjectDataExtra[1], in.ObjectDataExtra[2], in.ObjectDataExtra[3], in.ObjectDataExtra[4],
            in.Stack.size(), in.StackFlags, in.Relationships.size(), in.LinkedSlot.Unknown, in.LinkedSlot.ObjectID, in.Slots.size(), in.DynamicSpriteFlags.size(),
            in.HasMultitileData ? "yes" : "no", in.PortalData.size(), in.UnknownInt);
        if (getenv("DUMP_DATA")) { printf("    data:"); for (auto v : d) printf(" %d", v); printf("\n    temps:"); for (auto v : in.TempRegisters) printf(" %d", v); printf("\n"); }
        for (auto& f : in.Stack) {
            printf("    frame so %d tree %d node %d params %zu locals %zu state %d owner %d :", f.StackObjectID, f.TreeID, f.NodeID, f.Parameters.size(), f.Locals.size(), f.PrimitiveState, f.CodeOwnerObjType);
            for (auto v : f.Parameters) printf(" %d", v); printf(" |"); for (auto v : f.Locals) printf(" %d", v); printf("\n");
        }
        for (auto& s : in.Slots) printf("    slot (%d,%d)\n", s.Unknown, s.ObjectID);
        if (in.HasMultitileData) printf("    mt off %d,%d,%d group %d,%d,%d parent %d\n", in.MultitileData.OffsetX, in.MultitileData.OffsetY, in.MultitileData.OffsetLevel, in.MultitileData.GroupX, in.MultitileData.GroupY, in.MultitileData.GroupLevel, in.MultitileData.MultitileParentID);
        if (!in.PortalData.empty()) { printf("    portal:"); for (auto v : in.PortalData) printf(" %g", v); printf("\n"); }
        if (in.HasPersonData) {
            auto& p = in.PersonData;
            printf("    person anim ev %d engaged %d body '%s' '%s' '%s' '%s' head '%s' '%s' acc %zu anim '%s' carry '%s' base '%s' rs %d rfc %d\n", p.AnimEventCount, p.Engaged, p.Body.c_str(), p.BodyTex.c_str(), p.Unk1.c_str(), p.Unk2.c_str(), p.Head.c_str(), p.HeadTex.c_str(), p.Accessories.size(), p.Animation.c_str(), p.CarryAnimation.c_str(), p.BaseAnimation.c_str(), (int)p.RoutingState, p.RoutingFrameCount);
            printf("    first:"); for (auto v : p.FirstFloats) printf(" %g", v);
            printf("\n    motold:"); for (auto v : p.MotiveDataOld) printf(" %g", v);
            printf("\n    motive:"); for (auto v : p.MotiveData) printf(" %g", v);
            printf("\n    pd:"); for (auto v : p.PersonData) printf(" %d", v);
            auto pi = [](const char* n, const OBJMInteraction& i) { printf("\n    %s uid %d caller %d target %d icon %d tta %d args %d %d %d %d prio %d tree %d att %g flags %d", n, i.UID, i.CallerID, i.TargetID, i.Icon, i.TTAIndex, i.Args[0], i.Args[1], i.Args[2], i.Args[3], i.Priority, i.ActionTreeID, i.Attenuation, i.Flags); };
            pi("active", p.ActiveInteraction); pi("last", p.LastInteraction);
            for (auto& q : p.InteractionQueue) pi("queue", q);
            for (auto& u : p.ObjectUses) printf("\n    use target %d stacklen %d unk %d", u.TargetID, u.StackLength, u.Unknown2);
            for (auto& m : p.MotiveDeltas) printf("\n    delta motive %d tick %g stop %g", m.Motive, m.TickDelta, m.StopAt);
            printf("\n");
        }
    }
    printf("tail:"); for (auto b : objm.Tail) printf(" %02X", b); printf("\n");
    return 0;
}

/// Chunk classes with a Write(): parse + write == original payload?
static int CmdChunks(int argc, char** argv) {
    int bad = 0;
    for (int i = 2; i < argc; i++) {
        IffRawFile raw; std::string err;
        if (!raw.Load(argv[i], &err)) { printf("%s: %s\n", argv[i], err.c_str()); continue; }
        IffFile iff(argv[i]);
        for (auto c : iff.ListAll()) {
            st::Writer w(st::Endian::Big);
            if (!c->Write(&iff, w)) continue;
            auto rc = raw.Find(c->ChunkType.empty() ? c->GetTypeKey() : c->ChunkType, c->ChunkID);
            if (!rc) rc = raw.Find(c->GetTypeKey(), c->ChunkID);
            if (!rc) continue;
            auto out = w.ToArray();
            bool same = out == rc->Data;
            if (!same) bad++;
            size_t k = 0; while (k < out.size() && k < rc->Data.size() && out[k] == rc->Data[k]) k++;
            printf("%s %s#%d: %s (%zu vs %zu, first diff %zu)\n", argv[i], c->GetTypeKey(), c->ChunkID, same ? "identical" : "DIFFERENT", out.size(), rc->Data.size(), k);
        }
    }
    return bad ? 1 : 0;
}

// ---------------------------------------------------------------- record comparison
static bool LoadRecords(const char* path, std::map<int, OBJMInstance>& out, std::vector<std::unique_ptr<IffFile>>& keep) {
    IffRawFile raw; std::string err;
    if (!raw.Load(path, &err)) { printf("%s\n", err.c_str()); return false; }
    keep.emplace_back(new IffFile(path));
    auto objt = keep.back()->Get<OBJT>(0);
    OBJMRawChunk objm;
    if (!objt || !objm.Parse(raw.Find("ObjM", 1)->Data, err)) { printf("%s: %s\n", path, err.c_str()); return false; }
    for (size_t r = 0; r < objm.Records.size(); r++) {
        OBJMInstance in; std::string tail;
        if (!DecodeRecord(objm, r, objt, in, tail, err)) { printf("%s record %zu: %s\n", path, r, err.c_str()); continue; }
        out[objm.Table[r].first] = in;
    }
    return true;
}

static int CmdDiffObjm(int argc, char** argv) {
    FSO::Content::Content::Init(argv[2], argv[3]);
    std::map<int, OBJMInstance> a, b;
    std::vector<std::unique_ptr<IffFile>> keep;
    if (!LoadRecords(argv[4], a, keep) || !LoadRecords(argv[5], b, keep)) return 1;
    std::map<std::string, int> diff;
    std::map<std::string, std::string> example;
    int both = 0;
    auto note = [&](const std::string& f, const std::string& ex) { if (!diff[f]++) example[f] = ex; };
    for (auto& kv : a) {
        auto it = b.find(kv.first);
        if (it == b.end()) { note("missing in b", std::to_string(kv.first)); continue; }
        auto& x = kv.second; auto& y = it->second;
        if (!x.OBJT || !y.OBJT || x.OBJT->GUID != y.OBJT->GUID) { note("guid", std::to_string(kv.first)); continue; }
        both++;
        std::string nm = x.OBJT->Name + "#" + std::to_string(kv.first);
        char buf[256];
        if (x.X != y.X || x.Y != y.Y || x.Level != y.Level) { snprintf(buf, sizeof buf, "%s %d,%d -> %d,%d", nm.c_str(), x.X, x.Y, y.X, y.Y); note("position", buf); }
        if (x.Footprint.MinX != y.Footprint.MinX || x.Footprint.MinY != y.Footprint.MinY || x.Footprint.MaxX != y.Footprint.MaxX || x.Footprint.MaxY != y.Footprint.MaxY) {
            snprintf(buf, sizeof buf, "%s [%d %d %d %d] -> [%d %d %d %d]", nm.c_str(), x.Footprint.MinY, x.Footprint.MinX, x.Footprint.MaxY, x.Footprint.MaxX, y.Footprint.MinY, y.Footprint.MinX, y.Footprint.MaxY, y.Footprint.MaxX); note("footprint", buf); }
        if (x.UnknownData != y.UnknownData) note("unknowndata", nm);
        if (x.Attributes != y.Attributes) { std::string e = nm + ":"; for (auto v : x.Attributes) e += " " + std::to_string(v); e += " ->"; for (auto v : y.Attributes) e += " " + std::to_string(v); note("attributes", e); }
        if (x.TempRegisters != y.TempRegisters) note("temps", nm);
        for (int fi : { 8, 40, 5 }) { int xr = (uint16_t)(x.ObjectData[fi] ^ y.ObjectData[fi]); for (int bit = 0; bit < 16; bit++) if (xr & (1 << bit)) note("xorbit data[" + std::to_string(fi) + "] 0x" + [&]{ char b2[8]; snprintf(b2, sizeof b2, "%X", 1 << bit); return std::string(b2); }() + ((x.ObjectData[fi] >> bit) & 1 ? " lost" : " gained"), nm); }
        for (size_t i = 0; i < x.ObjectData.size() && i < y.ObjectData.size(); i++) if (x.ObjectData[i] != y.ObjectData[i]) {
            snprintf(buf, sizeof buf, "%s %d -> %d", nm.c_str(), x.ObjectData[i], y.ObjectData[i]); note("data[" + std::to_string(i) + "]", buf); }
        if (x.ObjectDataExtra != y.ObjectDataExtra) note("dataextra", nm);
        if (x.StackFlags != y.StackFlags) note("stackflags", nm);
        bool stackSame = x.Stack.size() == y.Stack.size();
        for (size_t i = 0; stackSame && i < x.Stack.size(); i++) {
            auto& f = x.Stack[i]; auto& g = y.Stack[i];
            if (f.TreeID != g.TreeID || f.NodeID != g.NodeID || f.StackObjectID != g.StackObjectID || f.Parameters != g.Parameters || f.Locals != g.Locals || f.CodeOwnerObjType != g.CodeOwnerObjType || f.PrimitiveState != g.PrimitiveState) stackSame = false;
        }
        if (!stackSame) {
            std::string e = nm + ":";
            for (auto& f : x.Stack) e += " " + std::to_string(f.TreeID) + "@" + std::to_string(f.NodeID) + "/o" + std::to_string(f.CodeOwnerObjType) + "/p" + std::to_string(f.Parameters.size()) + "l" + std::to_string(f.Locals.size());
            e += " ->";
            for (auto& f : y.Stack) e += " " + std::to_string(f.TreeID) + "@" + std::to_string(f.NodeID) + "/o" + std::to_string(f.CodeOwnerObjType) + "/p" + std::to_string(f.Parameters.size()) + "l" + std::to_string(f.Locals.size());
            note("stack", e);
            if (x.Stack.size() == y.Stack.size()) {
                bool shape = true;
                for (size_t i = 0; i < x.Stack.size(); i++) if (x.Stack[i].TreeID != y.Stack[i].TreeID || x.Stack[i].Parameters.size() != y.Stack[i].Parameters.size() || x.Stack[i].Locals.size() != y.Stack[i].Locals.size() || x.Stack[i].CodeOwnerObjType != y.Stack[i].CodeOwnerObjType) shape = false;
                if (!shape) note("stack shape (same depth)", e);
            }
        }
        bool relSame = x.Relationships.size() == y.Relationships.size();
        for (size_t i = 0; relSame && i < x.Relationships.size(); i++) if (x.Relationships[i].TargetID != y.Relationships[i].TargetID || x.Relationships[i].Values != y.Relationships[i].Values || x.Relationships[i].IsPresent != y.Relationships[i].IsPresent) relSame = false;
        if (!relSame) note("relationships", nm);
        if (x.LinkedSlot.ObjectID != y.LinkedSlot.ObjectID) { snprintf(buf, sizeof buf, "%s %d -> %d", nm.c_str(), x.LinkedSlot.ObjectID, y.LinkedSlot.ObjectID); note("linked", buf); }
        bool slotSame = x.Slots.size() == y.Slots.size();
        for (size_t i = 0; slotSame && i < x.Slots.size(); i++) if (x.Slots[i].ObjectID != y.Slots[i].ObjectID || x.Slots[i].Unknown != y.Slots[i].Unknown) slotSame = false;
        if (!slotSame) { snprintf(buf, sizeof buf, "%s %zu -> %zu", nm.c_str(), x.Slots.size(), y.Slots.size()); note("slots", buf); }
        if (x.DynamicSpriteFlags != y.DynamicSpriteFlags) note("dsf", nm);
        if (x.HasMultitileData != y.HasMultitileData || (x.HasMultitileData && (x.MultitileData.OffsetX != y.MultitileData.OffsetX || x.MultitileData.OffsetY != y.MultitileData.OffsetY ||
            x.MultitileData.GroupX != y.MultitileData.GroupX || x.MultitileData.GroupY != y.MultitileData.GroupY || x.MultitileData.MultitileParentID != y.MultitileData.MultitileParentID || x.MultitileData.GroupLevel != y.MultitileData.GroupLevel || x.MultitileData.OffsetLevel != y.MultitileData.OffsetLevel))) {
            snprintf(buf, sizeof buf, "%s off %d,%d grp %d,%d lead %d -> off %d,%d grp %d,%d lead %d", nm.c_str(), x.MultitileData.OffsetX, x.MultitileData.OffsetY, x.MultitileData.GroupX, x.MultitileData.GroupY, x.MultitileData.MultitileParentID,
                y.MultitileData.OffsetX, y.MultitileData.OffsetY, y.MultitileData.GroupX, y.MultitileData.GroupY, y.MultitileData.MultitileParentID); note("multitile", buf); }
        if (x.PortalData != y.PortalData) note("portal", nm);
        if (x.UnknownInt != y.UnknownInt) note("unknownint", nm);
        if (x.HasPersonData && y.HasPersonData) {
            auto& p = x.PersonData; auto& q = y.PersonData;
            for (int i = 0; i < 16; i++) if (p.MotiveData[i] != q.MotiveData[i]) { snprintf(buf, sizeof buf, "%s m%d %g -> %g", nm.c_str(), i, p.MotiveData[i], q.MotiveData[i]); note("motive", buf); }
            for (size_t i = 0; i < p.PersonData.size() && i < q.PersonData.size(); i++) if (p.PersonData[i] != q.PersonData[i]) { snprintf(buf, sizeof buf, "%s %d -> %d", nm.c_str(), p.PersonData[i], q.PersonData[i]); note("pd[" + std::to_string(i) + "]", buf); }
            if (p.InteractionQueue.size() != q.InteractionQueue.size() || p.ActiveInteraction.TTAIndex != q.ActiveInteraction.TTAIndex) note("queue", nm);
            if (p.ObjectUses.size() != q.ObjectUses.size()) note("objectuses", nm);
            if (p.MotiveDeltas.size() != q.MotiveDeltas.size()) note("motivedeltas", nm);
            if (p.Animation != q.Animation) note("animation", nm + " '" + p.Animation + "' -> '" + q.Animation + "'");
        }
    }
    int onlyB = 0; for (auto& kv : b) if (!a.count(kv.first)) onlyB++;
    printf("%zu vs %zu records, %d in both, %d only in b\n", a.size(), b.size(), both, onlyB);
    for (auto& kv : diff) printf("  %-28s %4d  e.g. %s\n", kv.first.c_str(), kv.second, example[kv.first].c_str());
    return 0;
}

// ---------------------------------------------------------------- round trip
using namespace FSO::SimAntics;
using FSO::LotView::Model::LotTilePos;

struct EntSnap {
    uint32_t GUID = 0; int16_t ID = 0;
    int X = 0, Y = 0, L = 0; int Dir = 0; int Cont = 0, Slot = -1;
    std::vector<int16_t> Attr, Data;
    std::map<uint16_t, std::vector<int16_t>> Rel;
    uint64_t DSF = 0;
    bool Person = false; std::vector<int16_t> PD; std::vector<int16_t> Mot;
    std::string Name;
};
struct LotSnap {
    std::map<int16_t, EntSnap> Ents;
    std::vector<std::vector<uint16_t>> Floors;
    std::vector<std::vector<uint64_t>> Walls;
    std::vector<int16_t> Globals;
    int Hours = 0, Minutes = 0;
};

struct Lot {
    std::unique_ptr<FSO::LotView::World> World;
    std::unique_ptr<VMContext> Ctx;
    FSO::SimAntics::Model::VMNullHeadlineProvider HP;
    std::unique_ptr<VM> Vm;
    std::unique_ptr<IffFile> House;
    VMWorldActivator::Blueprint* BP = nullptr;
    std::vector<VMAvatar*> Family;
    bool Load(const std::string& path, int16_t houseNumber, int w, int h) {
        World.reset(new FSO::LotView::World(w, h));
        VM::UseWorld(true);
        Ctx.reset(new VMContext(World.get()));
        Vm.reset(new VM(Ctx.get(), &HP));
        Vm->Init();
        Vm->OnDialog = [](FSO::SimAntics::Model::VMDialogInfo&) {};
        House.reset(new IffFile(path));
        VMWorldActivator act(Vm.get(), World.get());
        BP = act.LoadFromIff(House.get());
        VMTS1FamilyLoader loader(Vm.get());
        try { Family = loader.LoadFamily(houseNumber); } catch (std::exception& e) { printf("family load failed: %s\n", e.what()); }
        return BP != nullptr;
    }
    void Render(const std::string& png) {
        auto arch = Ctx->Architecture();
        int width = 800, height = 600;
        auto& st = World->State;
        st.Zoom(FSO::LotView::WorldZoom::Medium);
        st.Rotation(FSO::LotView::WorldRotation::TopLeft);
        st.Level(1);
        int minX = 9999, minY = 9999, maxX = -1, maxY = -1;
        auto& wl = arch->Walls[0];
        for (int y = 0; y < arch->Height; y++) for (int x = 0; x < arch->Width; x++)
            if ((int)wl[(size_t)(y * arch->Width + x)].Segments != 0) { minX = std::min(minX, x); minY = std::min(minY, y); maxX = std::max(maxX, x); maxY = std::max(maxY, y); }
        if (maxX >= 0) st.CenterTile(st::Vector2((minX + maxX + 1) / 2.0f, (minY + maxY + 1) / 2.0f));
        else st.CenterTile(st::Vector2(arch->Width / 2.0f, arch->Height / 2.0f));
        std::unordered_set<uint32_t> rooms;
        for (size_t i = 1; i < Ctx->RoomInfo.size(); i++) if (!Ctx->RoomInfo[i].Room.IsOutside) rooms.insert((uint32_t)i);
        BP->Cutaway = FSO::SimAntics::Utils::VMArchitectureTools::GenerateRoomCut(arch, 1, st.Rotation(), rooms);
        st.DynamicCutaway = true;
        BP->Damage.push_back(FSO::LotView::Model::BlueprintDamage(FSO::LotView::Model::BlueprintDamageType::WALL_CUT_CHANGED));
        FSO::LotView::Framebuffer fb(width, height);
        (void)width;
        World->Draw(fb);
        std::vector<uint8_t> rgba((size_t)fb.Width * fb.Height * 4);
        for (size_t i = 0; i < fb.Color.size(); i++) { uint32_t c = fb.Color[i];
            rgba[i * 4] = (uint8_t)c; rgba[i * 4 + 1] = (uint8_t)(c >> 8); rgba[i * 4 + 2] = (uint8_t)(c >> 16); rgba[i * 4 + 3] = 255; }
        st::writePNG(png.c_str(), fb.Width, fb.Height, rgba.data());
    }
    /// The neighbourhood picture (264x144 like the shipped BMP_ #512): far zoom on the house.
    void Thumbnail(int w, int h, std::vector<uint8_t>& rgb) {
        FSO::LotView::World tw(w, h);
        (void)tw;
        // render through the lot's own world at its view size, then sample down (the world's view is fixed at creation)
        auto arch = Ctx->Architecture();
        auto& st = World->State;
        st.Zoom(FSO::LotView::WorldZoom::Far);
        st.Rotation(FSO::LotView::WorldRotation::TopLeft);
        st.Level(1);
        int minX = 9999, minY = 9999, maxX = -1, maxY = -1;
        auto& wl = arch->Walls[0];
        for (int y = 0; y < arch->Height; y++) for (int x = 0; x < arch->Width; x++)
            if ((int)wl[(size_t)(y * arch->Width + x)].Segments != 0) { minX = std::min(minX, x); minY = std::min(minY, y); maxX = std::max(maxX, x); maxY = std::max(maxY, y); }
        if (maxX >= 0) st.CenterTile(st::Vector2((minX + maxX + 1) / 2.0f, (minY + maxY + 1) / 2.0f));
        BP->Cutaway.assign((size_t)(arch->Width * arch->Height), false);
        BP->Damage.push_back(FSO::LotView::Model::BlueprintDamage(FSO::LotView::Model::BlueprintDamageType::WALL_CUT_CHANGED));
        // twice the picture's size at far zoom, averaged down 2x2 (World::Draw sizes its view to the target)
        FSO::LotView::Framebuffer fb(w * 2, h * 2);
        World->Draw(fb);
        rgb.assign((size_t)w * h * 3, 0);
        for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
            int acc[3] = { 0, 0, 0 };
            for (int k = 0; k < 4; k++) {
                uint32_t c = fb.Color[(size_t)(y * 2 + (k >> 1)) * fb.Width + (x * 2 + (k & 1))];
                acc[0] += (int)(c & 0xFF); acc[1] += (int)((c >> 8) & 0xFF); acc[2] += (int)((c >> 16) & 0xFF);
            }
            uint8_t* p = &rgb[((size_t)y * w + x) * 3];
            p[0] = (uint8_t)(acc[0] / 4); p[1] = (uint8_t)(acc[1] / 4); p[2] = (uint8_t)(acc[2] / 4);
        }
        st.Zoom(FSO::LotView::WorldZoom::Medium);
    }
};

static LotSnap Snapshot(Lot& lot) {
    LotSnap s;
    for (auto e : lot.Vm->Entities) {
        if (!e || e->Dead || e->GhostImage) continue;
        EntSnap es;
        es.GUID = e->Object->OBJ->GUID; es.ID = e->ObjectID; es.Name = e->ToString();
        auto p = e->Position();
        es.X = p.x; es.Y = p.y; es.L = p.Level;
        if (auto a = dynamic_cast<VMAvatar*>(e)) { int d = (int)std::lround(a->RadianDirection() / (M_PI / 4)); es.Dir = ((d % 8) + 8) % 8; }
        else es.Dir = e->GetValue(FSO::SimAntics::Model::VMStackObjectVariable::Direction);
        es.Cont = e->Container ? e->Container->ObjectID : 0; es.Slot = e->Container ? e->ContainerSlot : -1;
        es.Attr = e->AttributeData();
        while (!es.Attr.empty() && es.Attr.back() == 0) es.Attr.pop_back();
        es.Data.assign(e->ObjectData.begin(), e->ObjectData.begin() + std::min<size_t>(68, e->ObjectData.size()));
        for (auto& kv : e->MeToObject) es.Rel[kv.first] = kv.second;
        es.DSF = e->DynamicSpriteFlags;
        if (auto a = dynamic_cast<VMAvatar*>(e)) {
            es.Person = true;
            for (int i = 0; i < 80; i++) es.PD.push_back(a->GetPersonData((FSO::SimAntics::Model::VMPersonDataVariable)i));
            for (int i = 0; i < 16; i++) es.Mot.push_back(a->GetMotiveData((FSO::SimAntics::Model::VMMotive)i));
        }
        s.Ents[es.ID] = es;
    }
    auto arch = lot.Ctx->Architecture();
    for (int l = 0; l < 2; l++) {
        std::vector<uint16_t> f; std::vector<uint64_t> w;
        for (auto& t : arch->Floors[(size_t)l]) f.push_back(t.Pattern);
        for (auto& t : arch->Walls[(size_t)l]) w.push_back((uint64_t)t.Segments | ((uint64_t)t.TopLeftPattern << 8) | ((uint64_t)t.TopRightPattern << 20) |
            ((uint64_t)t.BottomLeftPattern << 32) | ((uint64_t)t.BottomRightPattern << 44) | ((uint64_t)(t.TopLeftStyle & 0xF) << 56) | ((uint64_t)(t.TopRightStyle & 0xF) << 60));
        s.Floors.push_back(f); s.Walls.push_back(w);
    }
    s.Globals = lot.Vm->GlobalState;
    s.Hours = lot.Ctx->Clock->Hours; s.Minutes = lot.Ctx->Clock->Minutes;
    return s;
}

/// Compares two snapshots; returns the number of differences (printed, capped).
static int Compare(const LotSnap& a, const LotSnap& b, bool verbose) {
    int diffs = 0;
    auto say = [&](const std::string& m) { diffs++; if (verbose && diffs <= 60) printf("    %s\n", m.c_str()); };
    std::map<std::string, int> fieldDiffs;
    // objects: by id, same GUID
    int matched = 0;
    for (auto& kv : a.Ents) {
        auto it = b.Ents.find(kv.first);
        if (it == b.Ents.end() || it->second.GUID != kv.second.GUID) {
            // objects created fresh on every load (controllers) get new ids: match by GUID count below
            continue;
        }
        matched++;
        auto& x = kv.second; auto& y = it->second;
        char buf[256];
        if (x.X != y.X || x.Y != y.Y || x.L != y.L) { snprintf(buf, sizeof buf, "%s #%d position %d,%d,L%d -> %d,%d,L%d", x.Name.c_str(), x.ID, x.X, x.Y, x.L, y.X, y.Y, y.L); say(buf); fieldDiffs["position"]++; }
        if (x.Dir != y.Dir) { fieldDiffs["direction"]++; snprintf(buf, sizeof buf, "%s #%d dir %d -> %d", x.Name.c_str(), x.ID, x.Dir, y.Dir); say(buf); }
        if (x.Cont != y.Cont || x.Slot != y.Slot) { fieldDiffs["container"]++; snprintf(buf, sizeof buf, "%s #%d container %d/%d -> %d/%d", x.Name.c_str(), x.ID, x.Cont, x.Slot, y.Cont, y.Slot); say(buf); }
        if (x.Attr != y.Attr) { fieldDiffs["attributes"]++; say(x.Name + " #" + std::to_string(x.ID) + " attributes differ"); }
        for (size_t i = 0; i < x.Data.size() && i < y.Data.size(); i++) if (x.Data[i] != y.Data[i]) {
            fieldDiffs["objdata[" + std::to_string(i) + "]"]++;
        }
        if (x.Rel != y.Rel) { fieldDiffs["relationships"]++; say(x.Name + " #" + std::to_string(x.ID) + " relationships differ"); }
        if (x.DSF != y.DSF) { fieldDiffs["dynamic sprite flags"]++; say(x.Name + " #" + std::to_string(x.ID) + " dynamic sprite flags differ"); }
        if (x.Person) {
            for (size_t i = 0; i < x.PD.size() && i < y.PD.size(); i++) if (x.PD[i] != y.PD[i]) fieldDiffs["persondata[" + std::to_string(i) + "]"]++;
            for (size_t i = 0; i < x.Mot.size() && i < y.Mot.size(); i++) if (x.Mot[i] != y.Mot[i]) { fieldDiffs["motive[" + std::to_string(i) + "]"]++;
                snprintf(buf, sizeof buf, "%s motive %zu: %d -> %d", x.Name.c_str(), i, x.Mot[i], y.Mot[i]); say(buf); }
        }
    }
    // controllers ("global sim objects") are created fresh by every load (VMWorldActivator) and delete themselves
    // when their pack/lot does not need them: their count is not lot state
    std::set<uint32_t> controllers;
    if (auto c = FSO::Content::Content::Get()) for (auto g : c->WorldObjects->ControllerObjects) controllers.insert(g);
    std::map<uint32_t, int> ga, gb;
    for (auto& kv : a.Ents) if (!controllers.count(kv.second.GUID)) ga[kv.second.GUID]++;
    for (auto& kv : b.Ents) if (!controllers.count(kv.second.GUID)) gb[kv.second.GUID]++;
    int countDiff = 0;
    for (auto& kv : ga) if (gb[kv.first] != kv.second) { countDiff++; char buf[128]; snprintf(buf, sizeof buf, "GUID %08X count %d -> %d", kv.first, kv.second, gb[kv.first]); say(buf); }
    for (auto& kv : gb) if (!ga.count(kv.first)) { countDiff++; char buf[128]; snprintf(buf, sizeof buf, "GUID %08X count 0 -> %d", kv.first, kv.second); say(buf); }
    int fl = 0, wl = 0;
    for (int l = 0; l < 2; l++) {
        for (size_t i = 0; i < a.Floors[l].size() && i < b.Floors[l].size(); i++) if (a.Floors[l][i] != b.Floors[l][i]) fl++;
        for (size_t i = 0; i < a.Walls[l].size() && i < b.Walls[l].size(); i++) if (a.Walls[l][i] != b.Walls[l][i]) {
            if (wl++ < 4 && verbose) printf("    wall L%d #%zu: %016llx -> %016llx\n", l + 1, i, (unsigned long long)a.Walls[l][i], (unsigned long long)b.Walls[l][i]); }
    }
    if (fl) { fieldDiffs["floor tiles"] += fl; say("floor tiles differ: " + std::to_string(fl)); }
    if (wl) { fieldDiffs["wall tiles"] += wl; say("wall tiles differ: " + std::to_string(wl)); }
    if (a.Hours != b.Hours || a.Minutes != b.Minutes) { fieldDiffs["clock"]++; char buf[64]; snprintf(buf, sizeof buf, "clock %02d:%02d -> %02d:%02d", a.Hours, a.Minutes, b.Hours, b.Minutes); say(buf); }
    for (size_t i = 0; i < a.Globals.size() && i < b.Globals.size(); i++) if (a.Globals[i] != b.Globals[i]) {
        fieldDiffs["global[" + std::to_string(i) + "]"]++; char buf[64]; snprintf(buf, sizeof buf, "global %zu: %d -> %d", i, a.Globals[i], b.Globals[i]); say(buf); }
    printf("  compared: %zu -> %zu objects, %d matched by id+GUID, %d GUID count differences\n", a.Ents.size(), b.Ents.size(), matched, countDiff);
    if (!fieldDiffs.empty()) {
        printf("  differing fields (objects):");
        for (auto& kv : fieldDiffs) printf(" %s=%d", kv.first.c_str(), kv.second);
        printf("\n");
    }
    return diffs;
}

/// Build/buy edits a player would make (SIMSAVE_EDIT=1): delete, move, buy (a type the house has and one it has
/// not), paint floors (built-in and custom .flr), build and paper walls, change a motive and the money.
static void Edit(Lot& lot) {
    using namespace FSO::LotView::Model;
    auto ctx = lot.Ctx.get();
    auto vm = lot.Vm.get();
    auto arch = ctx->Architecture();
    auto content = FSO::Content::Content::Get();
    auto find = [&](const char* name) -> VMEntity* {
        for (auto e : vm->Entities) if (!e->Dead && e->ToString().find(name) != std::string::npos && e->Position() != LotTilePos::OUT_OF_WORLD) return e;
        return nullptr;
    };
    auto placeNear = [&](FSO::SimAntics::Entities::VMMultitileGroup* g, int cx, int cy) -> bool {
        for (int r = 0; r < 8; r++) for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) {
            if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
            auto res = g->ChangePosition(LotTilePos::FromBigTile((int16_t)(cx + dx), (int16_t)(cy + dy), 1), Direction::NORTH, ctx);
            if (res.Status == FSO::SimAntics::Model::VMPlacementError::Success) return true;
        }
        return false;
    };
    if (auto e = find("Flamingo")) { printf("  edit: delete %s #%d\n", e->ToString().c_str(), e->ObjectID); e->MultitileGroup->Delete(ctx); }
    if (auto e = find("Fountain")) {
        auto p = e->Position();
        bool ok = placeNear(e->MultitileGroup, p.x / 16 + 3, p.y / 16 + 3);
        printf("  edit: move %s #%d %d,%d -> %d,%d %s\n", e->ToString().c_str(), e->ObjectID, p.x, p.y, e->Position().x, e->Position().y, ok ? "ok" : "FAILED");
    }
    int bx = arch->Width / 2, by = arch->Height - 12;
    // buy: one type the house has (a flower bed / shrub) and types it does not have
    std::set<uint32_t> have;
    for (auto e : vm->Entities) have.insert(e->Object->OBJ->GUID);
    if (auto e = find("Tulips")) {
        auto g = ctx->CreateObjectInstance(e->Object->OBJ->GUID, LotTilePos::OUT_OF_WORLD, Direction::NORTH, false);
        if (g && placeNear(g, bx, by)) printf("  edit: bought another %s #%d at %d,%d\n", g->Objects[0]->ToString().c_str(), g->Objects[0]->ObjectID, g->Objects[0]->Position().x, g->Objects[0]->Position().y);
    }
    std::vector<uint64_t> guids;
    for (auto& kv : content->WorldObjects->Entries) guids.push_back(kv.first);
    std::sort(guids.begin(), guids.end());
    int bought = 0;
    for (auto g64 : guids) {
        if (bought >= 2) break;
        uint32_t guid = (uint32_t)g64;
        if (have.count(guid)) continue;
        auto go = content->WorldObjects->Get(guid, false);
        if (!go || !go->OBJ || go->OBJ->ObjectType != FSO::Files::Formats::IFF::Chunks::OBJDType::Normal || go->OBJ->Price < 50 || go->OBJ->Price > 3000) continue;
        if (bought == 0 && go->OBJ->IsMultiTile()) continue;
        if (bought == 1 && !go->OBJ->IsMultiTile()) continue;
        if (go->OBJ->IsMultiTile() && go->OBJ->SubIndex >= 0) continue; // create multitile objects through their master
        auto g = ctx->CreateObjectInstance(guid, LotTilePos::OUT_OF_WORLD, Direction::NORTH, false);
        if (!g || g->Objects.empty()) continue;
        if (!placeNear(g, bx + 4 + bought * 4, by)) { g->Delete(ctx); continue; }
        printf("  edit: bought new type %08X '%s' (%zu tiles) #%d at %d,%d\n", guid, g->Objects[0]->ToString().c_str(), g->Objects.size(), g->Objects[0]->ObjectID, g->Objects[0]->Position().x, g->Objects[0]->Position().y);
        bought++;
    }
    // floors: built-in id and a custom .flr (id >= 256) on a 3x2 patch
    uint16_t builtin = 0, custom = 0;
    for (auto& kv : content->WorldFloors->Entries) { if (kv.first > 30 && kv.first < 256 && (!builtin || kv.first < builtin)) builtin = kv.first; if (kv.first >= 256 && (!custom || kv.first < custom)) custom = kv.first; }
    int painted = 0;
    for (int dx = 0; dx < 3; dx++) for (int dy = 0; dy < 2; dy++) {
        FloorTile ft; ft.Pattern = (dy == 0 || !custom) ? builtin : custom;
        if (ft.Pattern && arch->SetFloor((int16_t)(bx - 6 + dx), (int16_t)(by + dy), 1, ft, true)) painted++;
    }
    printf("  edit: painted %d floor tiles (built-in %d, custom %d)\n", painted, builtin, custom);
    // walls: a short new wall, and repaper an existing wall side with a custom wallpaper
    uint16_t wbuiltin = 0, wcustom = 0;
    for (auto& kv : content->WorldWalls->Entries) { if (kv.first > 2 && kv.first < 256 && (!wbuiltin || kv.first < wbuiltin)) wbuiltin = kv.first; if (kv.first >= 256 && (!wcustom || kv.first < wcustom)) wcustom = kv.first; }
    for (int k = 0; k < 3; k++) {
        WallTile wt = arch->GetWall((int16_t)(bx - 6 + k), (int16_t)(by + 3), 1);
        wt.Segments = (WallSegments)((int)wt.Segments | (int)WallSegments::TopLeft);
        wt.TopLeftStyle = 1; wt.TopLeftPattern = wbuiltin; wt.BottomLeftPattern = wbuiltin;
        arch->SetWall((int16_t)(bx - 6 + k), (int16_t)(by + 3), 1, wt);
    }
    int repapered = 0;
    for (size_t i = 0; i < arch->Walls[0].size() && repapered < 4; i++) {
        auto& w = arch->Walls[0][i];
        if ((w.Segments & WallSegments::TopLeft) > 0 && w.TopLeftPattern != 0 && wcustom) { auto t = w; t.TopLeftPattern = wcustom; arch->SetWall((int16_t)(i % arch->Width), (int16_t)(i / arch->Width), 1, t); repapered++; }
    }
    arch->SignalRedraw();
    arch->RegenRoomMap();
    printf("  edit: 3 wall tiles built (wall %d), %d wall sides repapered (custom wall %d)\n", wbuiltin, repapered, wcustom);
    for (auto a : lot.Family) { a->SetMotiveData(FSO::SimAntics::Model::VMMotive::Hunger, -50); printf("  edit: %s hunger -> -50\n", a->ToString().c_str()); break; }
    vm->SetGlobalValue(13, (int16_t)((vm->GetGlobalValue(13) + 1234) % 10000));
    for (int i = 0; i < 60; i++) vm->Update();
}

static bool CopyFile(const std::string& a, const std::string& b) {
    std::vector<uint8_t> d;
    if (!st::ReadAllBytes(a, d)) return false;
    FILE* f = fopen(b.c_str(), "wb"); if (!f) return false;
    fwrite(d.data(), 1, d.size(), f); fclose(f); return true;
}

/// simsave roundtrip <gameDataDir> <cacheDir> <saveRoot> <houseNumber> <outDir> [ticks]
///  saveRoot: a save area holding UserData/ (a copy; it is written to). Loads the house (+ its family), runs `ticks`
///  VM updates, saves it (house + neighbours), reloads the saved file, compares, saves again and compares the files.
static int CmdRoundTrip(int argc, char** argv) {
    std::string gameData = argv[2], cache = argv[3], saveRoot = argv[4];
    int house = atoi(argv[5]);
    std::string outDir = argv[6];
    int ticks = argc > 7 ? atoi(argv[7]) : 0;
    FSO::Content::Content::Init(gameData, cache);
    auto content = FSO::Content::Content::Get();
    auto prov = content->Neighborhood.get();
    int hoodIdx = getenv("SIMSAVE_HOOD") ? atoi(getenv("SIMSAVE_HOOD")) : 0;
    if (prov) { prov->UserDataRoot = saveRoot; prov->InitSpecific(hoodIdx); prov->LoadCharacters(); }
    std::string path = VMTS1LotSaver::UserHousePath((int16_t)house);
    printf("house %d: %s\n", house, path.c_str());
    std::string tag = outDir + "/h" + std::to_string(house) + "_t" + std::to_string(ticks);

    LotSnap s1;
    {
        Lot lot;
        if (!lot.Load(path, (int16_t)house, 800, 600)) { printf("load failed\n"); return 1; }
        lot.Vm->OnSimanticsException = nullptr;
        for (int i = 0; i < ticks; i++) lot.Vm->Update();
        printf("  loaded, %d updates, clock %02d:%02d, %zu entities\n", ticks, lot.Ctx->Clock->Hours, lot.Ctx->Clock->Minutes, lot.Vm->Entities.size());
        if (getenv("SIMSAVE_EDIT")) { Edit(lot); tag += "_edit"; }
        lot.Render(tag + "_a_before.png");
        s1 = Snapshot(lot);
        VMTS1LotSaver saver(lot.Vm.get());
        std::vector<uint8_t> thumb;
        int32_t tw, th;
        VMTS1LotSaver::ThumbnailSizeOf(path, tw, th);
        lot.Thumbnail(tw, th, thumb);
        saver.SetThumbnail(tw, th, thumb);
        if (!saver.SaveHouse(path, path)) { printf("  SAVE FAILED: %s\n", saver.Error.c_str()); return 1; }
        if (!saver.SaveNeighbours(prov->UserPath + "/Neighborhood.iff")) printf("  neighbours not saved: %s\n", saver.Error.c_str());
        for (auto& l : saver.Log) printf("  saver: %s\n", l.c_str());
        CopyFile(path, tag + "_saved1.iff");
        lot.Render(tag + "_b_after_save.png");
    }
    // reload the neighbourhood as the game would (fresh provider state from the saved Neighborhood.iff)
    if (prov) { prov->InitSpecific(hoodIdx); }
    LotSnap s2;
    int diffs = 0;
    {
        Lot lot;
        if (!lot.Load(path, (int16_t)house, 800, 600)) { printf("reload failed\n"); return 1; }
        lot.Render(tag + "_c_reloaded.png");
        s2 = Snapshot(lot);
        printf("  reloaded the saved house: before save vs after reload\n");
        diffs = Compare(s1, s2, true);
        // save again without running: the second file should equal the first apart from what the reload re-initialises
        VMTS1LotSaver saver(lot.Vm.get());
        std::vector<uint8_t> again;
        if (!saver.BuildHouse(path, again)) { printf("  second save failed: %s\n", saver.Error.c_str()); return 1; }
        std::vector<uint8_t> first;
        st::ReadAllBytes(path, first);
        if (FILE* f2 = fopen((tag + "_saved2.iff").c_str(), "wb")) { fwrite(again.data(), 1, again.size(), f2); fclose(f2); }
        IffRawFile fa, fb; fa.Parse(first); fb.Parse(again);
        int same = 0, differ = 0;
        for (auto& c : fa.Chunks) {
            auto o = fb.Find(c.Type, c.ID);
            if (o && o->Data == c.Data) same++; else { differ++; printf("  save #2 differs in %s#%d\n", c.Type.c_str(), c.ID); }
        }
        printf("  save #2 (reload + save, no ticks): %d chunks identical, %d differ\n", same, differ);
    }
    printf("RESULT house %d ticks %d: %d differences\n", house, ticks, diffs);
    return 0;
}

/// simsave nhood <gameDataDir> <cacheDir> <saveRoot> [index]: load the neighbourhood and write it back unchanged
/// (must report "unchanged": every record parses and re-encodes to its saved bytes).
static int CmdNhood(int argc, char** argv) {
    FSO::Content::Content::Init(argv[2], argv[3]);
    auto prov = FSO::Content::Content::Get()->Neighborhood.get();
    prov->UserDataRoot = argv[4]; prov->InitSpecific(argc > 5 ? atoi(argv[5]) : 0);
    std::vector<std::string> log; std::string err;
    bool ok = FSO::Content::TS1::TS1NeighbourhoodWriter::Save(prov, prov->UserPath + "/Neighborhood.iff", {}, &err, &log);
    for (auto& l : log) printf("  %s\n", l.c_str());
    printf("%s %s\n", ok ? "ok" : "FAILED", err.c_str());
    return ok ? 0 : 1;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("usage: simsave raw <file.iff>... | objm <gameDataDir> <cacheDir> <House.iff>...\n");
        return 2;
    }
    std::string cmd = argv[1];
    // ADDED(sims): SIM3DS_SEED=<n> makes the VM random numbers repeatable (byte-identical round trips)
    if (const char* seed = getenv("SIM3DS_SEED")) FSO::SimAntics::VMContext::DebugSeed = strtoull(seed, nullptr, 10);
    if (cmd == "raw") return CmdRaw(argc, argv);
    if (cmd == "objm" && argc >= 5) return CmdObjm(argc, argv);
    if (cmd == "dump" && argc >= 5) return CmdDump(argc, argv);
    if (cmd == "chunks") return CmdChunks(argc, argv);
    if (cmd == "nhood" && argc >= 5) return CmdNhood(argc, argv);
    if (cmd == "diffobjm" && argc >= 6) return CmdDiffObjm(argc, argv);
    if (cmd == "roundtrip" && argc >= 7) return CmdRoundTrip(argc, argv);
    printf("unknown command\n");
    return 2;
}
