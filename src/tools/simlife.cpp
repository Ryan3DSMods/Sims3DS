/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// simlife — loads a Sims 1 house with the family that lives there and lets the translated VM run the Sims.
//   simlife <gameDataDir> <cacheDir> <houseNumber> [days=2] [--move-in <familyNumber>] [--quiet] [--hours-per-snapshot N]
// gameDataDir is an install tree (GameData/, UserData/Neighborhood.iff, UserData/Houses/HouseNN.iff,
// UserData/Characters/User*.iff). The log shows every interaction each Sim queues/starts/finishes (name, object,
// interaction) and periodic motive snapshots, plus every object reset (SimAntics exception).
// Nothing here drives the Sims: they run their own person trees (main -> 'try autonomy' -> Find Best Interaction
// (VMFindBestAction) -> Gosub Found Action) and FreeSims' VMFreeWill tick.
#include "content/ContentManager.h"
#include "content/TS1NeighbourProvider.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"
#include "simantics/utils/VMWorldActivator.h"
#include "simantics/utils/VMTS1FamilyLoader.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/engine/VMThread.h"
#include "simantics/engine/VMPrimitiveRegistration.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/primitives/VMFindBestAction.h"
#include "simantics/engine/VMQueuedAction.h"
#include "simantics/engine/VMTS1Neighbourhood.h"
#include "files/formats/iff/IffFile.h"
#include "files/formats/iff/chunks/BHAV.h"
#include "simantics/engine/translator/VMRoutine.h"
#include <cstdio>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
using namespace FSO::SimAntics;
using FSO::SimAntics::Model::VMMotive;
using FSO::SimAntics::Model::VMPersonDataVariable;
using FSO::SimAntics::Engine::VMQueuedAction;

struct SimTrack {
    VMAvatar* Avatar;
    std::map<VMQueuedAction*, std::shared_ptr<VMQueuedAction>> Queued;
    VMQueuedAction* Running = nullptr;
    std::string LastActionFrame; // deepest action-tree frame seen on the last update (for --why)
    int64_t RunningSince = 0;
    VMQueuedAction* LastRunning = nullptr;
    std::map<std::string, int> Started;
    int StartedCount = 0;
};

static std::string Clean(const std::string& s) {
    std::string r;
    for (char c : s) r += (c == '\n' || c == '\r') ? ' ' : c;
    return r;
}

static std::string ActionDesc(VMQueuedAction* a) {
    std::string obj = (a->Callee == nullptr) ? std::string("?") : Clean(a->Callee->ToString());
    char buf[64];
    snprintf(buf, sizeof(buf), " [prio %d]", a->Priority);
    return "'" + Clean(a->Name) + "' on " + obj + buf;
}

int main(int argc, char** argv) {
    if (argc < 4) {
        printf("usage: simlife <gameDataDir> <cacheDir> <houseNumber> [days=2] [--move-in <familyNumber>] [--quiet] [--hours-per-snapshot N]\n");
        return 2;
    }
    int house = atoi(argv[3]);
    double days = 2;
    int moveIn = -1;
    bool quiet = false;
    bool stacks = false;
    std::string traceName; int traceFrom = 0, traceTo = 0; // --trace <sim> <fromMinute> <toMinute>: every primitive that Sim runs
    uint64_t seed = 12345; // --seed N: VM random seed (runs are reproducible)
    std::string adsName; int adsMinute = -1; // --ads <sim> <minute>: every autonomy candidate that Sim evaluates in that minute
    bool traceChecks = false;
    int pushMinute = -1; std::string pushObject, pushName; // --push <minute> <object substring> <interaction>: user-queue it on the first Sim, report the delay
    int64_t pushTick = -1; std::shared_ptr<VMQueuedAction> pushed;
    bool why = false; // print where short interactions returned (last action-tree frame) and the route result
    int snapHours = 3;
    int snapMinutes = 0; // --snap-minutes N: extra compact motive line every N game minutes
    for (int i = 4; i < argc; i++) {
        if (!strcmp(argv[i], "--move-in") && i + 1 < argc) moveIn = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--quiet")) quiet = true;
        else if (!strcmp(argv[i], "--stacks")) stacks = true;
        else if (!strcmp(argv[i], "--why")) why = true;
        else if (!strcmp(argv[i], "--trace-checks")) traceChecks = true;
        else if (!strcmp(argv[i], "--push") && i + 3 < argc) { pushMinute = atoi(argv[++i]); pushObject = argv[++i]; pushName = argv[++i]; }
        else if (!strcmp(argv[i], "--ads") && i + 2 < argc) { adsName = argv[++i]; adsMinute = atoi(argv[++i]); }
        else if (!strcmp(argv[i], "--seed") && i + 1 < argc) seed = strtoull(argv[++i], nullptr, 10);
        else if (!strcmp(argv[i], "--trace") && i + 3 < argc) { traceName = argv[++i]; traceFrom = atoi(argv[++i]); traceTo = atoi(argv[++i]); }
        else if (!strcmp(argv[i], "--hours-per-snapshot") && i + 1 < argc) snapHours = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--snap-minutes") && i + 1 < argc) snapMinutes = atoi(argv[++i]);
        else days = atof(argv[i]);
    }

    FSO::Content::Content::Init(argv[1], argv[2]);
    auto content = FSO::Content::Content::Get();
    auto hood = content->Neighborhood.get();
    printf("objects indexed: %zu\n", content->WorldObjects->Entries.size());
    if (hood == nullptr || hood->Neighbors == nullptr) { printf("no neighbourhood found under %s/UserData\n", argv[1]); return 1; }
    printf("neighbourhood: %zu neighbour records, %zu character iffs registered, NGBH data:",
        hood->Neighbors->NeighbourByID.size(), hood->CharacterFiles.size());
    if (hood->Neighborhood) for (int i = 0; i < 4; i++) printf(" %d", hood->Neighborhood->NeighborhoodData[i]);
    printf("\nfamilies:\n");
    for (auto fam : hood->ListFamilies()) {
        printf("  #%d %-10s house %2d budget %6d members:", fam->ChunkID, hood->GetFamilyName((int16_t)fam->ChunkID).c_str(), fam->HouseNumber, fam->Budget);
        for (auto g : fam->FamilyGUIDs) {
            int16_t nid = 0;
            auto n = hood->GetNeighborIDForGUID(g, nid) ? hood->GetNeighborByID(nid) : nullptr;
            printf(" %s", (n == nullptr) ? "?" : n->Name.c_str());
        }
        printf("\n");
    }

    VM::UseWorld(false);
    VMContext ctx(nullptr);
    Model::VMNullHeadlineProvider hp;
    VM vm(&ctx, &hp);
    ctx.RandomSeed = seed;
    vm.Init();

    std::map<std::string, int> resetsByObject;
    std::map<std::string, int> resetsByMessage;
    std::vector<std::string> firstResets;
    int resets = 0;
    vm.OnSimanticsException = [&](VMEntity* ent, VMEntity* callee, const std::string& msg) {
        resets++;
        std::string who = ent ? Clean(ent->ToString()) : "?";
        resetsByObject[who]++;
        resetsByMessage[who + ": " + msg]++;
        (void)callee;
    };
    vm.OnAutonomyChoice = [&](VMEntity* caller, const std::vector<VMPieMenuInteraction>& top, const VMPieMenuInteraction& chosen) {
        if (!why) return;
        printf("   [auto] %s picks '%s' on %s from:", caller->Name().c_str(), Clean(chosen.Name).c_str(), chosen.Callee ? Clean(chosen.Callee->ToString()).c_str() : "?");
        for (auto& t : top) printf(" '%s'@%s=%.3g", Clean(t.Name).c_str(), t.Callee ? Clean(t.Callee->ToString()).c_str() : "?", t.Score * 1000.0f);
        printf("\n");
    };
    int64_t traceElapsed = 0;
    if (!traceName.empty()) vm.OnTraceInstruction = [&](Engine::VMStackFrame* f, uint16_t opcode, int result) {
        if (traceElapsed < traceFrom || traceElapsed > traceTo) return;
        auto t = f->Thread ? f->Thread->GetEntity() : nullptr;
        if (t == nullptr || t->Name() != traceName) return;
        auto r = f->Routine;
        std::string pname = (opcode < 256 && ctx.Primitives[opcode]) ? ctx.Primitives[opcode]->Name : ("call " + std::to_string(opcode));
        if (opcode >= 256) {
            FSO::Files::Formats::IFF::Chunks::BHAV* b = nullptr;
            if (opcode >= 8192) b = f->ScopeResource()->SemiGlobal ? f->ScopeResource()->SemiGlobal->Get<FSO::Files::Formats::IFF::Chunks::BHAV>(opcode) : nullptr;
            else if (opcode >= 4096) b = f->ScopeResource()->Get<FSO::Files::Formats::IFF::Chunks::BHAV>(opcode);
            else b = f->Global()->Resource->Get<FSO::Files::Formats::IFF::Chunks::BHAV>(opcode);
            if (b) pname += " '" + Clean(b->ChunkLabel) + "'";
        }
        if (f->Thread->IsCheck && !traceChecks) return;
        printf("      T%lld %s%s%s:%d %s -> %d  (stack %zu, aqb %d, q %zu, prio %d)\n", (long long)traceElapsed, f->Thread->IsCheck ? "[C] " : "", f->ActionTree ? "[A] " : "",
            (r && r->Chunk) ? Clean(r->Chunk->ChunkLabel).c_str() : "?", f->InstructionPointer, pname.c_str(), result,
            f->Thread->Stack.size(), (int)f->Thread->ActiveQueueBlock, f->Thread->Queue.size(),
            dynamic_cast<VMAvatar*>(t) ? ((VMAvatar*)t)->GetPersonData(VMPersonDataVariable::Priority) : -1);
    };
    int64_t adsElapsed = 0, adsSeen = -1; // the first minute >= adsMinute in which that Sim evaluates candidates
    if (!adsName.empty()) FSO::SimAntics::Primitives::VMFindBestAction::DebugCandidate = [&](VMEntity* caller, VMEntity* obj, const std::string& name, bool ok, float score) {
        if (adsElapsed < adsMinute || caller->Name() != adsName) return;
        if (adsSeen < 0) adsSeen = adsElapsed;
        if (adsSeen != adsElapsed) return;
        printf("   [ad] %s: %-28s on %-40s %s %.4g\n", caller->Name().c_str(), Clean(name).c_str(), Clean(obj->ToString()).c_str(), ok ? "score" : "REJECTED", score * 1000.0f);
    };
    std::vector<std::string> dialogs;
    vm.OnDialog = [&](Model::VMDialogInfo& i) {
        if (i.Title == "SimAntics Exception!") {
            if (firstResets.size() < 6) firstResets.push_back(i.Message);
        } else if (dialogs.size() < 50) {
            dialogs.push_back(Clean(i.Title + ": " + i.Message));
        }
    };

    auto path = hood->GetHousePath(house);
    std::unique_ptr<FSO::Files::Formats::IFF::IffFile> houseIff;
    try { houseIff.reset(new FSO::Files::Formats::IFF::IffFile(path)); }
    catch (std::exception& e) { printf("cannot open %s: %s\n", path.c_str(), e.what()); return 1; }
    VMWorldActivator act(&vm, nullptr);
    act.LoadFromIff(houseIff.get());
    printf("house %d (%s): lot %dx%d, entities %zu, rooms %zu\n", house, path.c_str(),
        ctx.Architecture()->Width, ctx.Architecture()->Height, vm.Entities.size(), ctx.RoomInfo.size());

    VMTS1FamilyLoader loader(&vm);
    FSO::Files::Formats::IFF::Chunks::FAMI* fam = nullptr;
    if (moveIn >= 0) {
        fam = hood->GetFamily((int16_t)moveIn);
        if (fam == nullptr) { printf("no family #%d\n", moveIn); return 1; }
        if (fam->HouseNumber != 0 && fam->HouseNumber != house) { printf("family #%d lives in house %d\n", moveIn, fam->HouseNumber); return 1; }
        printf("moving family #%d (%s) into house %d\n", moveIn, hood->GetFamilyName((int16_t)moveIn).c_str(), house);
    }
    auto sims = loader.LoadFamily((int16_t)house, fam);
    for (auto& l : loader.Log) printf("%s\n", l.c_str());
    if (sims.empty()) { printf("no Sims on this lot.\n"); }

    std::vector<SimTrack> tracks;
    for (auto s : sims) tracks.push_back(SimTrack{ s });
    if (getenv("SIMLIFE_TESTGUID")) { // debug: can this GUID be created out of world?
        uint32_t g = (uint32_t)strtoul(getenv("SIMLIFE_TESTGUID"), nullptr, 16);
        auto def = FSO::Content::Content::Get()->WorldObjects->Get(g, false);
        auto grp = ctx.CreateObjectInstance(g, FSO::LotView::Model::LotTilePos::OUT_OF_WORLD, FSO::LotView::Model::Direction::NORTH, 0, 0, false, false);
        printf("test create %08X: definition %s, group %s\n", g, def ? "found" : "MISSING", grp ? "created" : "null");
    }
    auto clockStr = [&](int64_t minutes) {
        char buf[32];
        int64_t m = minutes + ctx.Clock->Hours * 0; (void)m;
        snprintf(buf, sizeof(buf), "D%lld %02d:%02d", (long long)(minutes / 1440 + 1), ctx.Clock->Hours, ctx.Clock->Minutes);
        return std::string(buf);
    };
    auto motives = [&](VMAvatar* a) {
        char buf[256];
        snprintf(buf, sizeof(buf), "hunger %4d comfort %4d hygiene %4d bladder %4d energy %4d fun %4d social %4d room %4d | mood %4d",
            a->GetMotiveData(VMMotive::Hunger), a->GetMotiveData(VMMotive::Comfort), a->GetMotiveData(VMMotive::Hygiene),
            a->GetMotiveData(VMMotive::Bladder), a->GetMotiveData(VMMotive::Energy), a->GetMotiveData(VMMotive::Fun),
            a->GetMotiveData(VMMotive::Social), a->GetMotiveData(VMMotive::Room), a->GetMotiveData(VMMotive::Mood));
        return std::string(buf);
    };
    for (auto& t : tracks) {
        auto a = t.Avatar;
        auto& ap = a->TS1Appearance;
        printf("  %-10s obj %3d  neighbour %2d family %d age %2d gender %d  skills cook %d mech %d charisma %d body %d logic %d creat %d  personality nice %d active %d playful %d outgoing %d neat %d\n",
            a->Name().c_str(), a->ObjectID, a->GetPersonData(VMPersonDataVariable::NeighborId), a->GetPersonData((VMPersonDataVariable)61),
            a->GetPersonData((VMPersonDataVariable)58), a->GetPersonData((VMPersonDataVariable)65),
            a->GetPersonData((VMPersonDataVariable)10), a->GetPersonData((VMPersonDataVariable)12), a->GetPersonData((VMPersonDataVariable)11),
            a->GetPersonData((VMPersonDataVariable)17), a->GetPersonData((VMPersonDataVariable)18), a->GetPersonData((VMPersonDataVariable)15),
            a->GetPersonData((VMPersonDataVariable)2), a->GetPersonData((VMPersonDataVariable)3), a->GetPersonData((VMPersonDataVariable)5),
            a->GetPersonData((VMPersonDataVariable)6), a->GetPersonData((VMPersonDataVariable)7));
        printf("      appearance: %s body %s/%s head %s/%s skin %s\n", ap.AgeType.c_str(), ap.Body.Mesh.c_str(), ap.Body.Texture.c_str(),
            ap.Head.Mesh.c_str(), ap.Head.Texture.c_str(), ap.SkinTone.c_str());
        { auto nb = FSO::SimAntics::Engine::Utils::VMTS1Neighbourhood::GetNeighbor(FSO::SimAntics::Engine::Utils::VMTS1Neighbourhood::NeighborIDOf(a));
          printf("      NBRS person data:"); if (nb) for (size_t k = 0; k < nb->PersonData.size(); k++) if (nb->PersonData[k]) printf(" %zu=%d", k, nb->PersonData[k]); printf("\n"); }
        printf("      preference female %d male %d; relationships:", a->GetPersonData((VMPersonDataVariable)75), a->GetPersonData((VMPersonDataVariable)76));
        for (auto& o : tracks) {
            if (o.Avatar == a) continue;
            auto rel = FSO::SimAntics::Engine::Utils::VMTS1Neighbourhood::GetRelationship(
                FSO::SimAntics::Engine::Utils::VMTS1Neighbourhood::NeighborIDOf(a), FSO::SimAntics::Engine::Utils::VMTS1Neighbourhood::NeighborIDOf(o.Avatar), false);
            printf(" %s[", o.Avatar->Name().c_str());
            if (rel != nullptr) for (size_t k = 0; k < rel->size(); k++) printf("%s%d", k ? " " : "", (*rel)[k]);
            printf("]");
        }
        printf("\n");
    }

    // ---- run ----
    const int64_t totalMinutes = (int64_t)(days * 1440);
    int64_t elapsed = 0;
    int lastMinute = ctx.Clock->Minutes, lastHourSnap = -1;
    int64_t updates = 0;
    int64_t lastSnapMinute = -1;
    printf("---- running %.1f game days (clock %02d:%02d) ----\n", days, ctx.Clock->Hours, ctx.Clock->Minutes);
    while (elapsed < totalMinutes) {
        vm.Update();
        updates++;
        if (ctx.Clock->Minutes != lastMinute) { lastMinute = ctx.Clock->Minutes; elapsed++; }
        traceElapsed = elapsed;
        if (pushMinute >= 0 && elapsed == pushMinute && !pushed && !tracks.empty()) {
            auto sim = tracks[0].Avatar;
            for (auto ent : vm.Entities) {
                if (Clean(ent->ToString()).find(pushObject) == std::string::npos || ent->TreeTable == nullptr) continue;
                for (auto& ia : ent->TreeTable->Interactions) {
                    if (ent->TreeTableStrings == nullptr || ent->TreeTableStrings->GetString((int32_t)ia->TTAIndex) != pushName) continue;
                    auto act = ent->GetAction((int32_t)ia->TTAIndex, sim, &ctx);
                    if (act) { act->Priority = (int16_t)Engine::VMQueuePriority::UserDriven; sim->Thread->EnqueueAction(act); pushed = act; pushTick = ctx.Clock->Ticks; }
                    break;
                }
                if (pushed) break;
            }
            printf("%s %-10s PUSH     '%s' on %s: %s\n", clockStr(elapsed).c_str(), sim->Name().c_str(), pushName.c_str(), pushObject.c_str(), pushed ? "queued (user-driven)" : "NOT FOUND");
        }
        if (pushed && pushTick >= 0) {
            auto th = tracks[0].Avatar->Thread.get();
            bool running = false;
            for (int qi = 0; qi <= th->ActiveQueueBlock && qi < (int)th->Queue.size(); qi++) if (th->Queue[qi] == pushed) running = true;
            bool gone = std::find(th->Queue.begin(), th->Queue.end(), pushed) == th->Queue.end();
            if (running || gone) {
                printf("   [push] %s after %lld ticks (%.1f s at 30 ticks/s)\n", running ? "started" : "dropped", (long long)(ctx.Clock->Ticks - pushTick), (ctx.Clock->Ticks - pushTick) / 30.0);
                pushTick = -1;
            }
        }
        adsElapsed = elapsed;
        { // TS1(sims) QA: other avatars (visitors, NPCs) arriving / leaving, with their family number and first action
            static std::map<int16_t, std::string> others;
            std::map<int16_t, std::string> now;
            for (auto ent : vm.Entities) {
                auto av = dynamic_cast<VMAvatar*>(ent);
                if (!av) continue;
                bool fam = false;
                for (auto& t : tracks) if (t.Avatar == av) fam = true;
                if (fam) continue;
                now[av->ObjectID] = Clean(av->ToString());
                if (!others.count(av->ObjectID)) {
                    auto p = av->Position();
                    printf("%s VISITOR  + %s (obj %d, nid %d, family pd61 %d, pd job %d) @(%d,%d) %s\n", clockStr(elapsed).c_str(), now[av->ObjectID].c_str(), av->ObjectID,
                           av->GetPersonData(VMPersonDataVariable::NeighborId), av->GetPersonData((VMPersonDataVariable)61), av->GetPersonData(VMPersonDataVariable::JobType),
                           p.TileX(), p.TileY(), (av->Thread && !av->Thread->Queue.empty()) ? Clean(av->Thread->Queue[0]->Name).c_str() : "-");
                }
            }
            static int64_t lastVisSnap = -1;
            if (elapsed % 60 == 0 && elapsed != lastVisSnap) {
                lastVisSnap = elapsed;
                for (auto ent : vm.Entities) {
                    auto av = dynamic_cast<VMAvatar*>(ent);
                    if (!av || !now.count(av->ObjectID)) continue;
                    auto p = av->Position();
                    std::string q;
                    if (av->Thread) for (auto& a : av->Thread->Queue) q += "[" + Clean(a->Name) + (a->Callee ? " @ " + Clean(a->Callee->ToString()) : std::string()) + "]";
                    printf("%s VISITOR  . %s @(%d,%d) type %d greet %d sched %d hunger %d energy %d %s\n", clockStr(elapsed).c_str(), now[av->ObjectID].c_str(), p.TileX(), p.TileY(), av->GetPersonData((VMPersonDataVariable)32), av->GetPersonData((VMPersonDataVariable)34), av->GetPersonData((VMPersonDataVariable)35),
                           av->GetMotiveData(VMMotive::Hunger), av->GetMotiveData(VMMotive::Energy), q.c_str());
                }
            }
            for (auto& kv : others) if (!now.count(kv.first)) printf("%s VISITOR  - %s (obj %d) left\n", clockStr(elapsed).c_str(), kv.second.c_str(), kv.first);
            others.swap(now);
        }
        if (snapMinutes > 0 && elapsed != lastSnapMinute && elapsed % snapMinutes == 0) {
            lastSnapMinute = elapsed;
            for (auto& t : tracks) {
                auto a = t.Avatar; if (a->Dead) continue;
                auto p = a->Position();
                printf("%s %-10s m H%d C%d Hy%d B%d E%d F%d S%d R%d Mood%d sit%d @(%d,%d)\n", clockStr(elapsed).c_str(), a->Name().c_str(),
                    a->GetMotiveData(VMMotive::Hunger), a->GetMotiveData(VMMotive::Comfort), a->GetMotiveData(VMMotive::Hygiene),
                    a->GetMotiveData(VMMotive::Bladder), a->GetMotiveData(VMMotive::Energy), a->GetMotiveData(VMMotive::Fun),
                    a->GetMotiveData(VMMotive::Social), a->GetMotiveData(VMMotive::Room), a->GetMotiveData(VMMotive::Mood),
                    a->GetPersonData((VMPersonDataVariable)0), p.TileX(), p.TileY());
            }
        }
        for (auto& t : tracks) {
            auto a = t.Avatar;
            if (a->Dead || !a->Thread) continue;
            auto& q = a->Thread->Queue;
            // newly queued
            for (auto& item : q) {
                if (t.Queued.find(item.get()) == t.Queued.end()) {
                    t.Queued[item.get()] = item;
                    if (!quiet) printf("%s %-10s queued   %s\n", clockStr(elapsed).c_str(), a->Name().c_str(), ActionDesc(item.get()).c_str());
                }
            }
            // started: the queue head while an action tree is on the stack
            bool inAction = false;
            for (auto& f : a->Thread->Stack) if (f->ActionTree) { inAction = true; break; }
            VMQueuedAction* head = (inAction && !q.empty()) ? q[0].get() : nullptr;
            if (head != nullptr && head != t.Running) {
                t.Running = head;
                t.RunningSince = elapsed;
                t.LastRunning = head;
                t.StartedCount++;
                t.Started[Clean(head->Name)]++;
                if (!quiet) printf("%s %-10s STARTED  %s\n", clockStr(elapsed).c_str(), a->Name().c_str(), ActionDesc(head).c_str());
            }
            if (head == nullptr) t.Running = nullptr;
            if (why && inAction) {
                std::string fr;
                for (auto& f : a->Thread->Stack) if (f->ActionTree) {
                    auto r = f->Routine;
                    fr += " > " + ((r && r->Chunk) ? Clean(r->Chunk->ChunkLabel) : std::string("?")) + ":" + std::to_string(f->InstructionPointer) + (f->IsRoutingFrame() ? "(route)" : "");
                }
                t.LastActionFrame = fr;
            }
            // finished / dropped
            for (auto it = t.Queued.begin(); it != t.Queued.end();) {
                bool still = false;
                for (auto& item : q) if (item.get() == it->first) { still = true; break; }
                if (!still) {
                    if (!quiet) printf("%s %-10s done     %s%s\n", clockStr(elapsed).c_str(), a->Name().c_str(), ActionDesc(it->first).c_str(),
                        (why && it->first == t.LastRunning) ? (" <- " + t.LastActionFrame + " route result " + std::to_string(a->GetPersonData((VMPersonDataVariable)62))).c_str() : "");
                    it = t.Queued.erase(it);
                } else ++it;
            }
        }
        if (ctx.Clock->Minutes == 0 && ctx.Clock->Hours % snapHours == 0 && ctx.Clock->Hours != lastHourSnap) {
            lastHourSnap = ctx.Clock->Hours;
            for (auto& t : tracks) {
                if (t.Avatar->Dead) { printf("%s %-10s (gone)\n", clockStr(elapsed).c_str(), t.Avatar->Name().c_str()); continue; }
                auto p = t.Avatar->Position();
                printf("%s %-10s MOTIVES  %s  @(%d,%d,L%d)\n", clockStr(elapsed).c_str(), t.Avatar->Name().c_str(), motives(t.Avatar).c_str(), p.TileX(), p.TileY(), (int)p.Level);
                if (stacks && t.Avatar->Thread) {
                    printf("      stack:");
                    for (auto& f : t.Avatar->Thread->Stack) {
                        auto r = f->Routine;
                        printf(" > %s:%d%s", (r && r->Chunk) ? Clean(r->Chunk->ChunkLabel).c_str() : "?", f->InstructionPointer,
                            f->IsRoutingFrame() ? "(route)" : "");
                        if (r && f->InstructionPointer < r->Instructions.size()) printf("[op %d]", r->Instructions[f->InstructionPointer].Opcode);
                    }
                    printf("\n");
                    for (auto& st : t.Avatar->Animations)
                        printf("      anim %s frame %.1f/%d speed %.2f weight %.2f end %d loop %d events queued %zu run %d\n",
                            st->Anim ? st->Anim->Name.c_str() : "?", st->CurrentFrame, st->Anim ? st->Anim->NumFrames() : -1,
                            st->Speed, st->Weight, (int)st->EndReached, (int)st->Loop, st->EventQueue.size(), (int)st->EventsRun);
                }
            }
        } else if (ctx.Clock->Minutes != 0) {
            if (ctx.Clock->Hours % snapHours != 0) lastHourSnap = -1;
        }
    }

    if (getenv("SIMLIFE_DUMP")) for (auto e : vm.Entities) if (Clean(e->ToString()).find(getenv("SIMLIFE_DUMP")) != std::string::npos)
        printf("  dump %s id %d dir %d lockout %d flags %04x pos (%d,%d,L%d) usecount %d\n", Clean(e->ToString()).c_str(), e->ObjectID, (int)e->Direction(),
            e->GetValue(Model::VMStackObjectVariable::LockoutCount), (uint16_t)e->GetValue(Model::VMStackObjectVariable::Flags),
            e->Position().TileX(), e->Position().TileY(), (int)e->Position().Level, e->GetValue(Model::VMStackObjectVariable::UseCount));
    if (getenv("SIMLIFE_DUMP")) for (auto e : vm.Entities) if (e->Thread && Clean(e->ToString()).find(getenv("SIMLIFE_DUMP")) != std::string::npos) { // TS1(sims) debug: stack
        for (auto& f : e->Thread->Stack) printf("    frame %s #%04X ip %d callee %s\n", f->Routine ? Clean(f->Routine->Rti.Name).c_str() : "?", f->Routine ? f->Routine->ID : 0,
            (int)f->InstructionPointer, f->Callee ? Clean(f->Callee->ToString()).c_str() : "-");
        for (auto& q : e->Thread->Queue) printf("    queue %s on %s\n", Clean(q->Name).c_str(), q->Callee ? Clean(q->Callee->ToString()).c_str() : "-");
    }
    if (getenv("SIMLIFE_DUMPAT")) { int x = atoi(getenv("SIMLIFE_DUMPAT")), y = atoi(strchr(getenv("SIMLIFE_DUMPAT"), ',') + 1);
        for (auto e : vm.Entities) if (std::abs(e->Position().TileX() - x) <= 1 && std::abs(e->Position().TileY() - y) <= 1 && e->Position().Level == 1)
            printf("  near (%d,%d): %s id %d at (%d,%d) fine (%d,%d) room %d pd0 %d\n", x, y, Clean(e->ToString()).c_str(), e->ObjectID, e->Position().TileX(), e->Position().TileY(), e->Position().x, e->Position().y,
                (int)ctx.GetObjectRoom(e), dynamic_cast<VMAvatar*>(e) ? ((VMAvatar*)e)->GetPersonData((VMPersonDataVariable)0) : -1); }
    printf("---- summary after %lld game minutes (%lld VM updates) ----\n", (long long)elapsed, (long long)updates);
    for (auto& t : tracks) {
        printf("%-10s started %d interactions:", t.Avatar->Name().c_str(), t.StartedCount);
        for (auto& kv : t.Started) printf(" %s x%d,", kv.first.c_str(), kv.second);
        printf("\n");
    }
    printf("entities %zu, dialogs (non-exception) %zu\n", vm.Entities.size(), dialogs.size());
    for (size_t i = 0; i < dialogs.size() && i < 8; i++) printf("  dialog: %s\n", dialogs[i].substr(0, 160).c_str());
    printf("object resets (SimAntics exceptions): %d\n", resets);
    for (auto& kv : resetsByMessage) printf("  %4dx %s\n", kv.second, kv.first.substr(0, 160).c_str());
    for (auto& r : firstResets) printf("---- first reset detail:\n%s\n", r.c_str());
    return 0;
}
