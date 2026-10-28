/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// simep — expansion pack health check (TS1(sims)). Loads lots with the translated engine on the full
// Complete Collection data and reports what does not work yet:
//   * objects the lot references (OBJT) that the content manager cannot resolve / load,
//   * SimAntics exceptions (object resets) with the BHAV stack at the time of the error,
//   * primitives the lot's code uses that the VM does not implement (they silently act as "true"),
//   * which TS1 Generic Sims Call modes and Manage Inventory modes actually ran,
//   * entity count before/after (objects deleted by resets).
//   simep <gameDataDir> <cacheDir> [--ticks N] [--family] [--quiet] House.iff [House.iff ...]
// --family moves the lot's family in (VMTS1FamilyLoader), like simlife.
#include "content/ContentManager.h"
#include "content/TS1NeighbourProvider.h"
#include "content/TS1JobProvider.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"
#include "simantics/utils/VMWorldActivator.h"
#include "simantics/utils/VMTS1FamilyLoader.h"
#include "simantics/engine/VMTS1Travel.h"
#include "simantics/engine/VMTS1Outfits.h"
#include "simantics/primitives/VMTS1MakeNewCharacter.h"
#include "simantics/primitives/VMFindLocationFor.h"
#include "simantics/engine/VMSuitProvider.h"
#include "content/TS1Clothing.h"
#include "content/TS1FamilyImport.h"
#include "vitaboy/content/TS1AvatarProvider.h"
#include "simantics/model/VMMotive.h"
#include "simantics/engine/VMTS1Neighbourhood.h"
#include "files/formats/iff/chunks/FAMI.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/entities/VMMultitileGroup.h"
#include "simantics/engine/VMThread.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/translator/VMRoutine.h"
#include "simantics/engine/translator/VMInstruction.h"
#include "simantics/engine/VMPrimitiveRegistration.h"
#include "files/formats/iff/IffFile.h"
#include "files/formats/iff/chunks/OBJT.h"
#include "files/formats/iff/chunks/OBJD.h"
#include "files/formats/iff/chunks/BHAV.h"
#include "files/formats/iff/chunks/WALm.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <chrono>
using namespace FSO::SimAntics;
using namespace FSO::Files::Formats::IFF;
using namespace FSO::Files::Formats::IFF::Chunks;

static std::string Clean(std::string s) {
    for (auto& c : s) if (c == '\r' || c == '\n' || c == '\t') c = ' ';
    return s;
}

static std::string RoutineName(Engine::VMStackFrame* f) {
    if (f == nullptr || f->Routine == nullptr) return "?";
    std::string n = f->Routine->Rti.Name;
    if (n.empty() && f->Routine->Chunk) n = f->Routine->Chunk->ChunkLabel;
    char buf[32]; snprintf(buf, sizeof buf, "#%04X:%d", f->Routine->ID, (int)f->InstructionPointer);
    return Clean(n) + buf;
}


// --travel <home house>: the expansion travel flow (VMTS1Travel) on real lots: home -> each lot -> home.
static int RunTravel(int home, const std::vector<std::string>& dests, int ticks) {
    using FSO::SimAntics::Engine::Utils::VMTS1Travel;
    auto content = FSO::Content::Content::Get();
    auto hood = content->Neighborhood.get();
    int failures = 0;
    for (auto& dest : dests) {
        // which house is the destination? (global 10 in the file)
        int destHouse = 0;
        { IffFile f(dest); VM::UseWorld(false); VMContext c(nullptr); Model::VMNullHeadlineProvider h; VM v(&c, &h); v.Init();
          VMWorldActivator a(&v, nullptr); a.LoadFromIff(&f); destHouse = v.GetGlobalValue(10); }
        std::string traveller;
        // 1. at home: the first Sim calls Generic Sims Call 17 (here through the same API)
        {
            IffFile f(hood->GetHousePath(home));
            VM::UseWorld(false); VMContext c(nullptr); Model::VMNullHeadlineProvider h; VM v(&c, &h); v.Init();
            v.OnDialog = [](Model::VMDialogInfo&) {};
            VMWorldActivator a(&v, nullptr); a.LoadFromIff(&f);
            auto fam = VMTS1Travel::Arrive(&v, (int16_t)home);
            for (int i = 0; i < 60; i++) v.Update();
            if (fam.empty()) { printf("travel: house %d has no family\n", home); return 1; }
            int16_t requested = -2;
            v.OnRequestLotSwitch = [&](int16_t lot) { requested = lot; };
            traveller = fam[0]->Name();
            fam[0]->SetMotiveData(Model::VMMotive::Hunger, 33); // marker: must arrive with the Sim
            VMTS1Travel::RequestLotSwitch(&v, fam[0], (int16_t)destHouse);
            printf("travel: %s leaves house %d for %d (request %d, transit info %d)\n", traveller.c_str(), home, destHouse, requested, hood->GameState.LotTransitInfo);
        }
        // 2. the destination lot spawns the traveller
        {
            IffFile f(dest);
            VM::UseWorld(false); VMContext c(nullptr); Model::VMNullHeadlineProvider h; VM v(&c, &h); v.Init();
            v.OnDialog = [](Model::VMDialogInfo&) {};
            int resets = 0; v.OnSimanticsException = [&](VMEntity*, VMEntity*, const std::string&) { resets++; };
            int16_t selected = 0;
            v.OnTS1Event = [&](int32_t e, int32_t val) { if (e == VMTS1Travel::EventSelectedSimChanged) selected = (int16_t)val; };
            VMWorldActivator a(&v, nullptr); a.LoadFromIff(&f);
            std::vector<std::string> log;
            VMTS1Travel::Arrive(&v, (int16_t)destHouse, &log);
            for (auto& l : log) printf("   %s\n", l.c_str());
            for (int i = 0; i < ticks; i++) v.Update();
            auto sim = dynamic_cast<VMAvatar*>(v.GetObjectById(v.GetGlobalValue(3)));
            bool ok = sim != nullptr && sim->Name() == traveller;
            if (!ok) failures++;
            printf("== travel to %d (%s): selected Sim %s, on lot %s, hunger %d, family %d, resets %d -> %s\n", destHouse, dest.c_str(),
                   sim ? sim->Name().c_str() : "(none)", sim && sim->Position() != FSO::LotView::Model::LotTilePos::OUT_OF_WORLD ? "yes" : "no",
                   sim ? sim->GetMotiveData(Model::VMMotive::Hunger) : 0, v.GetGlobalValue(9), resets, ok ? "OK" : "FAILED");
            if (sim == nullptr) continue;
            sim->SetMotiveData(Model::VMMotive::Hunger, -20); // marker for the way home
            VMTS1Travel::RequestLotSwitch(&v, sim, -1);
        }
        // 3. back home
        {
            int16_t back = VMTS1Travel::ResolveLot(hood->GameState.PendingLotSwitch);
            IffFile f(hood->GetHousePath(back));
            VM::UseWorld(false); VMContext c(nullptr); Model::VMNullHeadlineProvider h; VM v(&c, &h); v.Init();
            v.OnDialog = [](Model::VMDialogInfo&) {};
            VMWorldActivator a(&v, nullptr); a.LoadFromIff(&f);
            std::vector<std::string> log;
            auto fam = VMTS1Travel::Arrive(&v, back, &log);
            for (int i = 0; i < 30; i++) v.Update();
            VMAvatar* me = nullptr; for (auto x : fam) if (x->Name() == traveller) me = x;
            printf("   back home %d: %zu Sims, %s hunger %d, clock %02d:%02d\n", back, fam.size(), traveller.c_str(),
                   me ? me->GetMotiveData(Model::VMMotive::Hunger) : 0, c.Clock->Hours, c.Clock->Minutes);
        }
    }
    printf("TRAVEL: %d failures\n", failures);
    return failures;
}

int main(int argc, char** argv) {
    if (argc < 4) { printf("usage: simep <gameDataDir> <cacheDir> [--ticks N] [--family] [--quiet] House.iff ...\n"); return 2; }
    int ticks = 3000; bool family = false, quiet = false; int travel = 0;
    std::vector<std::string> houses;
    for (int i = 3; i < argc; i++) {
        if (!strcmp(argv[i], "--ticks") && i + 1 < argc) ticks = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--family")) family = true;
        else if (!strcmp(argv[i], "--quiet")) quiet = true;
        else if (!strcmp(argv[i], "--travel") && i + 1 < argc) travel = atoi(argv[++i]);
        else houses.push_back(argv[i]);
    }
    auto t0 = std::chrono::steady_clock::now();
    FSO::Content::Content::Init(argv[1], argv[2]);
    auto content = FSO::Content::Content::Get();
    double initMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    printf("content: %zu objects indexed (%zu archives), %zu global sim objects, %zu careers, init %.0f ms\n",
           content->WorldObjects->Entries.size(), content->WorldObjects->Archives.size(),
           content->WorldObjects->ControllerObjects.size(), content->Jobs ? content->Jobs->Jobs.size() : (size_t)0, initMs);

    if (const char* hood = getenv("SIMEP_HOOD")) { // neighbourhood index (0 = UserData, 1 = UserData2, ...); SIMEP_USERROOT = save area
        if (getenv("SIMEP_USERROOT")) content->Neighborhood->UserDataRoot = getenv("SIMEP_USERROOT");
        content->Neighborhood->InitSpecific(atoi(hood));
        content->Neighborhood->LoadCharacters();
        content->WorldObjects->RebuildIndexes();
        printf("neighbourhood %s: %s\n", hood, content->Neighborhood->UserPath.c_str());
        if (getenv("SIMEP_IMPORTFAM")) { // install TemplateFamilyUnleashed/*.FAM into this neighbourhood (writes files!)
            auto rs = FSO::Content::TS1::TS1FamilyImport::InstallTemplates(content->Neighborhood->UserPath, argv[1]);
            for (auto& r : rs) printf("import: ok %d skipped %d family %d house %d members %zu %s\n", (int)r.Ok, (int)r.Skipped, r.FamilyID, r.HouseNumber, r.GUIDs.size(), r.Error.c_str());
            content->Neighborhood->InitSpecific(atoi(hood));
            content->Neighborhood->LoadCharacters();
            content->WorldObjects->RebuildIndexes();
        }
    }
    if (getenv("SIMEP_CLOTHES")) {
        auto& clo = FSO::Content::TS1::TS1Clothing::Get();
        bool okb = clo.EnsureBuilt(); printf("clothes built %d, %zu collections, %zu skels\n", (int)okb, clo.Collections.size(), FSO::Content::TS1::TS1AvatarContent::Get()->BCF.SkelHostBCF.size());
        for (auto& kv : FSO::Content::TS1::TS1AvatarContent::Get()->BCF.SkelHostBCF) printf("skeleton %s (%s)\n", kv.first.c_str(), kv.second.c_str());
        for (auto& c : clo.Collections) { printf("collection %s:", c.first.c_str()); for (auto& t : c.second) printf(" %s(%zu)", t.first.c_str(), t.second.size()); printf("\n"); }
    }
    if (travel > 0) return RunTravel(travel, houses, ticks);
    int totalMissing = 0, totalResets = 0, totalUnimpl = 0;
    for (auto& path : houses) {
        std::unique_ptr<IffFile> house;
        try { house.reset(new IffFile(path)); } catch (std::exception& e) { printf("== %s: cannot open (%s)\n", path.c_str(), e.what()); continue; }
        VM::UseWorld(false);
        VMContext ctx(nullptr);
        Model::VMNullHeadlineProvider hp;
        VM vm(&ctx, &hp);
        vm.Init();
        vm.OnDialog = [](Model::VMDialogInfo&) {};
        std::map<std::string, int> resets;
        std::map<int, int> gcalls, invModes, unimplRun;
        vm.OnSimanticsException = [&](VMEntity* ent, VMEntity* callee, const std::string& msg) {
            std::string where;
            if (ent && ent->Thread) {
                auto& st = ent->Thread->Stack;
                for (int k = (int)st.size() - 1, n = 0; k >= 0 && n < 3; k--, n++) where += (n ? " < " : "") + RoutineName(st[k].get());
            }
            std::string file = (ent && ent->Object && ent->Object->Resource) ? " [" + ent->Object->Resource->Name + "]" : "";
            resets[Clean(ent ? ent->ToString() : std::string("?")) + file + " | " + Clean(msg).substr(0, 90) + " | " + where]++;
        };
        // operand bytes are not exposed by the trace hook; read them from the routine's BHAV instead
        static int traceId = getenv("SIMEP_TRACE") ? (int)strtol(getenv("SIMEP_TRACE"), nullptr, 16) : -1;
        static int traceLeft = getenv("SIMEP_TRACEMAX") ? atoi(getenv("SIMEP_TRACEMAX")) : 200;
        vm.OnTraceInstruction = [&](Engine::VMStackFrame* f, uint16_t opcode, int result) {
            static int traceOp = getenv("SIMEP_TRACEOP") ? atoi(getenv("SIMEP_TRACEOP")) : -1;
            if (traceOp >= 0 && opcode == traceOp && traceLeft > 0) {
                traceLeft--;
                printf("   traceop %s in %s -> %d\n", Clean(f->Caller ? f->Caller->ToString() : "?").c_str(), RoutineName(f).c_str(), result);
            }
            static const char* traceName = getenv("SIMEP_TRACENAME");
            if ((traceId == 0xFFFF || (traceId >= 0 && f->Routine && f->Routine->ID == traceId)) && traceLeft > 0
                && (traceName == nullptr || (f->Caller && f->Caller->ToString().find(traceName) != std::string::npos))) {
                traceLeft--;
                printf("   trace %s %s ip %d op %d -> %d dead %d so %s#%d lock %d\n", Clean(f->Caller ? f->Caller->ToString() : "?").c_str(), RoutineName(f).c_str(), (int)f->InstructionPointer, (int)opcode, result, f->Caller ? (int)f->Caller->Dead : -1,
                       f->StackObject ? Clean(f->StackObject->ToString()).c_str() : "-", f->StackObject ? f->StackObject->ObjectID : 0, f->StackObject ? f->StackObject->ObjectData[25] : 0);
            }
            if (opcode != 1 && opcode != 51) return;
            auto bhav = f->Routine ? f->Routine->Chunk : nullptr;
            if (bhav == nullptr || f->InstructionPointer >= bhav->Instructions.size()) return;
            auto& ins = bhav->Instructions[f->InstructionPointer];
            if (opcode == 1) gcalls[ins.Operand[0]]++;
            else invModes[ins.Operand[0]]++;
        };

        // objects referenced by the lot
        int missing = 0, objtTotal = 0;
        std::vector<std::string> missingNames;
        if (auto objt = house->Get<OBJT>(0)) {
            for (auto& e : objt->Entries) {
                if (e.GUID == 0) continue;
                objtTotal++;
                auto obj = content->WorldObjects->Get(e.GUID, true);
                if (obj == nullptr) { missing++; if (missingNames.size() < 12) { char b[96]; snprintf(b, sizeof b, "%08X %s", e.GUID, e.Name.c_str()); missingNames.push_back(b); } }
            }
        }
        // walls / floors the lot uses that no provider knows (they draw as the missing-texture pattern)
        std::vector<std::string> missingWF;
        auto checkWF = [&](const std::vector<WALmEntry>& ents, std::unordered_map<std::string, uint16_t>& dict, const char* kind) {
            for (auto& e : ents) {
                auto n = FSO::Content::ToLowerInvariant(e.Name); auto d = n.find('.'); if (d != std::string::npos) n = n.substr(0, d);
                if (!dict.count(n)) missingWF.push_back(std::string(kind) + " " + e.Name + " #" + std::to_string(e.ID));
            }
        };
        for (auto w : house->List<WALm>()) if (w->ChunkType != "FLRm") checkWF(w->Entries, content->WorldWalls->DynamicWallFromID, "wall");
        for (auto f : house->List<FLRm>()) checkWF(f->Entries, content->WorldFloors->DynamicFloorFromID, "floor");
        int entBefore = 0;
        try {
            VMWorldActivator act(&vm, nullptr);
            act.LoadFromIff(house.get());
        } catch (std::exception& e) { printf("== %s: LOAD FAILED: %s\n", path.c_str(), e.what()); continue; }
        if (family) {
            VMTS1FamilyLoader loader(&vm);
            auto sims = loader.LoadFamily(vm.GetGlobalValue(10));
            if (!quiet) printf("   family: %zu sims\n", sims.size());
        }
        entBefore = (int)vm.Entities.size();
        if (const char* cg = getenv("SIMEP_CREATE")) { // debug: create an object by GUID and report the group
            int already = 0;
            for (auto e : vm.Entities) if (e->Object && e->Object->OBJ->GUID == (uint32_t)strtoul(cg, nullptr, 16)) already++;
            printf("   %s on the lot after load: %d\n", cg, already);
            auto g = ctx.CreateObjectInstance((uint32_t)strtoul(cg, nullptr, 16), FSO::LotView::Model::LotTilePos::OUT_OF_WORLD, FSO::LotView::Model::Direction::NORTH, 0, 0, false, true);
            printf("   create %s: group %p objects %zu\n", cg, (void*)g, g ? g->Objects.size() : (size_t)0);
        }
        // unimplemented primitives used by the code of the objects on this lot
        std::set<std::string> seenRes;
        std::map<int, int> unimplUse;
        for (auto e : vm.Entities) {
            if (e->Object == nullptr || e->Object->Resource == nullptr) continue;
            auto res = e->Object->Resource;
            if (!seenRes.insert(res->Name).second) continue;
            std::vector<BHAV*> bhavs = res->List<BHAV>();
            if (res->SemiGlobal) for (auto b : res->SemiGlobal->List<BHAV>()) bhavs.push_back(b);
            for (auto b : bhavs) for (auto& ins : b->Instructions)
                if (ins.Opcode < 256 && (ins.Opcode >= ctx.Primitives.size() || ctx.Primitives[ins.Opcode] == nullptr)) unimplUse[ins.Opcode]++;
        }
        auto t1 = std::chrono::steady_clock::now();
        std::map<VMEntity*, uint64_t> lastOutfit;
        bool suitLog = getenv("SIMEP_SUITLOG") != nullptr;
        for (int i = 0; i < ticks; i++) {
            vm.Update();
            if (!suitLog) continue;
            for (auto e : vm.Entities) if (auto a = dynamic_cast<VMAvatar*>(e)) {
                auto o = a->BodyOutfit();
                auto it = lastOutfit.find(e);
                if (it != lastOutfit.end() && it->second != o) {
                    auto oft = FSO::Content::TS1::TS1AvatarContent::Get()->GetOutfit(o);
                    std::string act = (a->Thread && a->Thread->ActiveAction()) ? Clean(a->Thread->ActiveAction()->Name) : std::string("-");
                    printf("   [%02d:%02d] %s now wears %s / %s (doing '%s')\n", vm.GetGlobalValue(0), vm.GetGlobalValue(5), Clean(a->Name()).c_str(),
                           oft ? oft->TS1AppearanceID.c_str() : "?", oft ? oft->TS1TextureID.c_str() : "?", act.c_str());
                }
                lastOutfit[e] = o;
            }
        }
        double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t1).count();
        if (const char* mk = getenv("SIMEP_MAKECHAR")) { // test: Make New Character "color,age,gender,type[,coat]" then create the Sim
            int c = 1, ag = 9, g = 0, ty = 0, coat = 0;
            sscanf(mk, "%d,%d,%d,%d,%d", &c, &ag, &g, &ty, &coat);
            VMAvatar* caller = nullptr;
            for (auto e : vm.Entities) if (auto a = dynamic_cast<VMAvatar*>(e)) if (a->GetPersonData(Model::VMPersonDataVariable::NeighborId) > 0) { caller = a; break; }
            if (caller && caller->Thread) {
                Engine::VMStackFrame f;
                f.Thread = caller->Thread.get();
                f.Caller = f.Callee = caller;
                f.StackObject = caller;
                caller->SetAttribute(8, (int16_t)coat);
                f.Locals = { (int16_t)c, (int16_t)ag, (int16_t)g, 0, 0, 0, 0, 0 };
                Primitives::VMTS1MakeNewCharacterOperand op; uint8_t b[8] = { 0, 1, 2, (uint8_t)ty, 0, 0, 0, 0 }; op.Read(b);
                Primitives::VMTS1MakeNewCharacter prim;
                auto r = prim.Execute(&f, &op);
                printf("   make new character -> %d, neighbour %d\n", (int)r, f.StackObjectID());
                if (auto n = Engine::Utils::VMTS1Neighbourhood::GetNeighbor(f.StackObjectID())) {
                    auto sim = Engine::Utils::VMTS1Travel::CreatePerson(&vm, n->GUID);
                    if (sim) {
                        Primitives::VMFindLocationFor::FindLocationFor(sim, caller, vm.Context);
                        auto bs = Engine::VMSuitProvider::BodyStringsOf(sim);
                        printf("   created %s (%s, %s, age %d) at %s: %s | %s\n", Clean(sim->Name()).c_str(), n->Name.c_str(), bs.size() > 12 ? bs[12].c_str() : "?",
                               sim->GetPersonData(Model::VMPersonDataVariable::PersonsAge), (std::to_string(sim->Position().TileX()) + "," + std::to_string(sim->Position().TileY())).c_str(), bs.size() > 1 ? bs[1].c_str() : "", bs.size() > 2 ? bs[2].c_str() : "");
                    }
                }
            }
        }
        if (getenv("SIMEP_SUITS")) { // debug: person suits / wardrobes of the lot's Sims, and whether their files exist
            auto av = FSO::Content::TS1::TS1AvatarContent::Get();
            auto& clo = FSO::Content::TS1::TS1Clothing::Get();
            clo.EnsureBuilt();
            auto have = [&](const std::string& bodyString) {
                auto mesh = FSO::Content::ToLowerInvariant(bodyString.substr(0, bodyString.find(',')));
                auto tex = FSO::Content::ToLowerInvariant(bodyString.substr(bodyString.find('=') + 1));
                bool m = av->BCF.SkinHostBCF.count(mesh) || av->TS1Global.Has(mesh + ".cmx") || av->TS1Global.Has(mesh + ".cmx.bcf");
                bool t = av->TS1Global.Has(tex + ".bmp");
                return std::string(m ? "" : " [NO MESH]") + (t ? "" : " [NO TEX]");
            };
            for (auto e : vm.Entities) if (auto a = dynamic_cast<VMAvatar*>(e)) {
                if (a->GetPersonData(Model::VMPersonDataVariable::NeighborId) <= 0) continue;
                printf("   suits of %s:\n", Clean(a->Name()).c_str());
                auto bs = Engine::VMSuitProvider::BodyStringsOf(a);
                bool male = a->GetPersonData(Model::VMPersonDataVariable::Gender) == 0;
                auto age = a->GetPersonData(Model::VMPersonDataVariable::PersonsAge);
                for (int id : { 0, 1, 2, 3, 4, 5, 6, 13, 14, 15, 17, 18 }) {
                    auto b = FSO::Content::TS1::TS1PersonSuitBodyString(bs, (uint16_t)id, male, age < 18 && age != 0);
                    if (!b.empty()) printf("     suit %2d: %s%s\n", id, b.c_str(), have(b).c_str());
                }
                for (int t = 0; t < 6; t++) {
                    auto v = Engine::Utils::VMTS1Outfits::ValidOutfits(a, (int16_t)t);
                    printf("     wardrobe type %d: %zu outfits, current %d%s%s\n", t, v.size(), Engine::Utils::VMTS1Outfits::GetSuitIndex(a, (int16_t)t),
                           v.empty() ? "" : ", e.g. ", v.empty() ? "" : v[0].BodyString.c_str());
                }
            }
        }
        if (const char* pie = getenv("SIMEP_PIE")) { // debug: pie menus of matching objects, for the first family Sim
            VMEntity* caller = nullptr;
            for (auto e : vm.Entities) if (auto a = dynamic_cast<VMAvatar*>(e)) if (a->GetPersonData(Model::VMPersonDataVariable::NeighborId) > 0 && !a->IsPet() && a->GetPersonData(Model::VMPersonDataVariable::PersonsAge) >= 18) { caller = a; break; }
            for (auto e : vm.Entities) {
                if (caller == nullptr || Clean(e->ToString()).find(pie) == std::string::npos) continue;
                auto menu = e->GetPieMenu(&vm, caller, getenv("SIMEP_PIEHIDDEN") != nullptr);
                printf("   pie %s for %s:", Clean(e->ToString()).c_str(), Clean(caller->Name()).c_str());
                for (auto& m : menu) printf(" '%s'", Clean(m.Name).c_str());
                printf("\n");
            }
        }
        int nres = 0; for (auto& kv : resets) nres += kv.second;
        int nun = 0; for (auto& kv : unimplUse) nun += kv.second;
        totalMissing += missing; totalResets += nres; totalUnimpl += nun;
        int avatars = 0; for (auto e : vm.Entities) if (dynamic_cast<VMAvatar*>(e)) avatars++;
        printf("== %s: house %d, OBJT %d, missing %d, entities %d -> %d (avatars %d), resets %d, unimplemented-primitive uses %d, %d ticks %.0f ms\n",
               path.c_str(), vm.GetGlobalValue(10), objtTotal, missing, entBefore, (int)vm.Entities.size(), avatars, nres, nun, ticks, ms);
        if (!quiet) {
            for (auto& m : missingNames) printf("   missing: %s\n", m.c_str());
            for (auto& m : missingWF) printf("   missing %s\n", m.c_str());
            for (auto& kv : unimplUse) printf("   unimplemented primitive %d used %d times\n", kv.first, kv.second);
            for (auto& kv : resets) printf("   reset %3dx %s\n", kv.second, kv.first.c_str());
            std::string g; for (auto& kv : gcalls) g += " " + std::to_string(kv.first) + "x" + std::to_string(kv.second);
            if (!g.empty()) printf("   generic calls run:%s\n", g.c_str());
            std::string inv; for (auto& kv : invModes) inv += " " + std::to_string(kv.first) + "x" + std::to_string(kv.second);
            if (!inv.empty()) printf("   inventory modes run:%s\n", inv.c_str());
        }
    }
    printf("TOTAL: missing objects %d, resets %d, unimplemented-primitive uses %d\n", totalMissing, totalResets, totalUnimpl);
    return 0;
}
