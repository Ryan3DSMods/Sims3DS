/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sim3ds_host — runs the 3DS app (src/app) on the host with scripted input and writes the two screens composed
// into one image (top 400x240 above the 320x240 touch screen, like the console), so the UI can be checked
// without hardware.
//
//   sim3ds_host <dataRoot> <cacheDir> <outDir> <script|->
//
// dataRoot is the install tree as on the SD card (GameData/..., UserData/...). Script commands, one per line
// ('#' comments), each advancing simulated time at 30 frames per second:
//   frames N                 run N frames without input
//   ticks N                  run N VM updates without drawing (fast-forward; 60 updates = 1 s at normal speed)
//   tap X Y                  stylus tap on the touch screen (down one frame, up the next)
//   drag X0 Y0 X1 Y1 N       stylus drag over N frames
//   press BTN [N]            press a button (A B X Y L R ZL ZR START SELECT UP DOWN LEFT RIGHT), hold N frames
//   circle DX DY N           hold the circle pad (-1..1) for N frames
//   house N                  tap lot N on the neighbourhood map (front end; tap twice to enter; travels to its area)
//   waithouse N H            step up to N frames until house H is the loaded lot (H1: phone trips); expecthouse H
//   boot                     front end: finish loading now (no loading-screen animation)
//   front                    print the front end's state
//   tapobj NAME              tap the object / Sim whose name contains NAME in the touch view (found by picking)
//   pick TEXT                tap the pie menu item containing TEXT
//   button ID                tap a UI button: speed0..speed3, sim0.., queue0.., resume/walls/up/down/leave/quit (menu);
//                            front end: movein bulldoze downtown vacation studiotown magictown return prev next exit
//                            credits, family bin add/delete/movein/cancel, pick:<family>, dialog buttons dlg0..dlg2
//   shot NAME                write <outDir>/NAME.png
//   record NAME EVERY COUNT  run COUNT x EVERY frames, writing NAME_000.png... every EVERY frames
//   stats on|off             frame timings overlay on the top screen
//   log                      print the app log, the selected Sim's queue and the open pie menu
//   bench N                  run N frames, print average real ms per phase (VM, world draw, UI)
//   expect TEXT              fail (exit 3) unless the app log or the queue contains TEXT
//   spawn GUIDHEX DX DY      create an object next to the selected Sim (e.g. 24C95F99 = fire)
//   use NAME TEXT            queue the pie entry containing TEXT of the object NAME on the selected Sim
//   moneyhead N              the VM money-over-head on the selected Sim
//   cursor X Y               put the top-screen cursor at X,Y
#include "app/cas/CASTextInput.h"
#include "vitaboy/Animation.h"
#include "app/buy/UIBuyMode.h"
#include "app/buy/UICatalog.h"
#include "app/App.h"
#include <algorithm>
#include "files/formats/iff/chunks/NGBH.h"
#include "simantics/model/VMSetToNextCache.h"
#include "simantics/model/VMPersonDataVariable.h"
#include "content/ContentManager.h"
#include "content/TS1NeighbourProvider.h"
#include "world/World.h"
#include "world/model/Blueprint.h"
#include "app/WorldPicking.h"
#include "audio/GameAudio.h"
#include "app/Font.h"
#include "app/front/FrontEnd.h"
#include "simantics/engine/VMTS1Travel.h"
#include "simantics/engine/VMTS1Neighbourhood.h"
#include "core/png.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/engine/VMThread.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/translator/VMRoutine.h"
#include "simantics/engine/VMQueuedAction.h"
#include "simantics/VMClock.h"
#include "simantics/model/VMStackObjectVariable.h"
#include "files/formats/iff/chunks/TTAB.h"
#include "files/formats/iff/chunks/STR.h"
#include "simantics/VM.h"
#include "simantics/primitives/VMFindBestAction.h"
#include "simantics/VMContext.h"
#include "simantics/entities/VMMultitileGroup.h"
#include "app/LotScreen.h"
#include "app/LotSession.h"
#include <chrono>
#if defined(__has_include)
#if __has_include(<valgrind/callgrind.h>)
#include <valgrind/callgrind.h> // bench: instruction counts per phase under `valgrind --tool=callgrind`
#define HAVE_CALLGRIND 1
#endif
#endif
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <malloc.h>
#include <new>
#include "files/FilePool.h"
#include "files/formats/iff/IffFile.h"

// TS1(sims): heap accounting for the memory budget (`mem LABEL`): every operator new/delete is counted with
// malloc_usable_size, so "live" is the C++ heap in use and "peak" its high-water mark since the last `mem reset`.
static size_t g_HeapLive = 0, g_HeapPeak = 0;
// FIX(sims) memory QA: SIM3DS_HEAPPROF=1 records the call stack of every live operator-new block; `memprof LABEL`
// prints the live bytes by allocating function (host tool only; slow).
#include <execinfo.h>
#include <dlfcn.h>
#include <unordered_map>
namespace heapprof {
template <class T> struct MallocAlloc {
    typedef T value_type;
    MallocAlloc() = default;
    template <class U> MallocAlloc(const MallocAlloc<U>&) {}
    T* allocate(size_t n) { return (T*)malloc(n * sizeof(T)); }
    void deallocate(T* p, size_t) { free(p); }
    template <class U> bool operator==(const MallocAlloc<U>&) const { return true; }
    template <class U> bool operator!=(const MallocAlloc<U>&) const { return false; }
};
enum { DEPTH = 14 };
struct Stack { void* f[DEPTH]; int n; bool operator==(const Stack& o) const { return n == o.n && !memcmp(f, o.f, n * sizeof(void*)); } };
struct StackHash { size_t operator()(const Stack& s) const { size_t h = 1469598103u; for (int i = 0; i < s.n; i++) h = (h ^ (size_t)s.f[i]) * 16777619u; return h; } };
struct Rec { int id; size_t sz; };
static bool on = getenv("SIM3DS_HEAPPROF") != nullptr, busy = false;
static std::unordered_map<Stack, int, StackHash, std::equal_to<Stack>, MallocAlloc<std::pair<const Stack, int>>>* ids;
static std::vector<Stack, MallocAlloc<Stack>>* stacks;
static std::vector<long long, MallocAlloc<long long>>* bytes;
static std::unordered_map<void*, Rec, std::hash<void*>, std::equal_to<void*>, MallocAlloc<std::pair<void* const, Rec>>>* live;
static void Add(void* p, size_t sz) {
    if (!on || busy) return;
    busy = true;
    if (!ids) { ids = new (malloc(sizeof *ids)) std::remove_pointer<decltype(ids)>::type(); stacks = new (malloc(sizeof *stacks)) std::remove_pointer<decltype(stacks)>::type();
                bytes = new (malloc(sizeof *bytes)) std::remove_pointer<decltype(bytes)>::type(); live = new (malloc(sizeof *live)) std::remove_pointer<decltype(live)>::type(); }
    Stack s; s.n = backtrace(s.f, DEPTH);
    auto it = ids->find(s); int id;
    if (it == ids->end()) { id = (int)stacks->size(); ids->emplace(s, id); stacks->push_back(s); bytes->push_back(0); } else id = it->second;
    (*bytes)[id] += sz; (*live)[p] = Rec{id, sz};
    busy = false;
}
static void Del(void* p) {
    if (!on || busy || !live) return;
    busy = true;
    auto it = live->find(p);
    if (it != live->end()) { (*bytes)[it->second.id] -= it->second.sz; live->erase(it); }
    busy = false;
}
}
void* operator new(size_t n) {
    // QA2 (host only): report huge single allocations (a 2 GB peak showed up in the 32-bit run)
    if (n > (size_t)64 * 1024 * 1024) {
        static int reported = 0;
        if (reported++ < 5) {
            fprintf(stderr, "[bigalloc] %zu bytes\n", n);
            void* bt[24]; int k = backtrace(bt, 24); backtrace_symbols_fd(bt, k, 2);
        }
    }
    void* p = malloc(n ? n : 1);
    if (!p) throw std::bad_alloc();
    size_t us = malloc_usable_size(p);
    g_HeapLive += us;
    if (g_HeapLive > g_HeapPeak) g_HeapPeak = g_HeapLive;
    heapprof::Add(p, us);
    return p;
}
void* operator new[](size_t n) { return operator new(n); }
void operator delete(void* p) noexcept { if (p) { heapprof::Del(p); g_HeapLive -= malloc_usable_size(p); free(p); } }
void operator delete[](void* p) noexcept { operator delete(p); }
void operator delete(void* p, size_t) noexcept { operator delete(p); }
void operator delete[](void* p, size_t) noexcept { operator delete(p); }

using namespace simapp;

static double RealMs() {
    using namespace std::chrono;
    return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
}

static long HighWaterKB() {
    FILE* f = fopen("/proc/self/status", "r");
    if (!f) return -1;
    char line[256]; long kb = -1;
    while (fgets(line, sizeof line, f)) if (!strncmp(line, "VmHWM:", 6)) kb = atol(line + 6);
    fclose(f);
    return kb;
}

static std::string g_LastPicked; // FIX(sims) REL: the Sim "use *"/"use %F" picked last (also "relset @")
static std::string g_TraceName; // QA2: SIM3DS_TRACENAME or the script command "trace NAME"

struct Host {
    App app;
    std::string out;
    AppInput in;
    bool wasTouching = false;
    int frame = 0;
    double frameMs = 1000.0 / 30.0;
    bool autoDialogs = false; // TS1(sims) "autodlg on": answer SimAntics dialogs with their first button (logged)
    int dialogsAnswered = 0;

    void Step(int n = 1) {
        for (int i = 0; i < n; i++) {
            // TS1(sims) QA: SIM3DS_TRACENAME=<entity name part> prints the primitives that entity runs (SIM3DS_TRACEMAX, 20000)
            static const char* envTrace = getenv("SIM3DS_TRACENAME");
            if (envTrace && g_TraceName.empty()) g_TraceName = envTrace;
            const char* traceName = g_TraceName.empty() ? nullptr : g_TraceName.c_str(); // QA2: also "trace NAME" in a script
            if (traceName) if (auto s = app.Session()) if (!s->VM()->OnTraceInstruction) {
                s->VM()->OnTraceInstruction = [this](FSO::SimAntics::Engine::VMStackFrame* f, uint16_t opcode, int result) {
                    static int left = getenv("SIM3DS_TRACEMAX") ? atoi(getenv("SIM3DS_TRACEMAX")) : 20000;
                    if (left <= 0 || f == nullptr || f->Caller == nullptr || g_TraceName.empty()) return;
                    // H1: "trace =NAME" matches the whole entity name (e.g. a multitile lead, not its parts)
                    if (g_TraceName[0] == '=' ? f->Caller->ToString() != g_TraceName.substr(1) : f->Caller->ToString().find(g_TraceName) == std::string::npos) return;
                    left--;
                    std::string rn = f->Routine ? f->Routine->Rti.Name : std::string("?");
                    if (rn.empty() && f->Routine && f->Routine->Chunk) rn = f->Routine->Chunk->ChunkLabel;
                    printf("[trace %6.1fs] %s #%d %s#%04X:%d op %d -> %d%s\n", frame / 30.0, f->Caller->ToString().c_str(), f->Caller->ObjectID, rn.c_str(),
                           f->Routine ? f->Routine->ID : 0, (int)f->InstructionPointer, (int)opcode, result, f->Thread && f->Thread->IsCheck ? " [check]" : "");
                };
            }
            if (autoDialogs) if (auto s = app.Session()) if (!s->Dialogs.empty()) {
                auto& d = s->Dialogs.front();
                bool map = d.Type == 5 || d.Type == 7 || d.Type == 8 || d.Type == 11 || d.Type == 13 || d.Type == 6;
                if (!map) {
                    printf("[dlg %6.1fs] type %d '%s': %.160s -> %s\n", frame / 30.0, d.Type, d.Title.c_str(), d.Message.c_str(), d.HasYes ? d.Yes.c_str() : "OK");
                    s->RespondDialog(0); dialogsAnswered++;
                }
            }
            app.Frame(in, frameMs);
            if (auto b = simaudio::GameAudio::Get().Backend()) b->Pump(frameMs); // SIM3DS_AUDIO capture
            frame++;
            in.Down = 0;
            in.TouchDown = false;
            in.TouchUp = false;
            if (!app.Log.empty()) { printf("%s", app.Log.c_str()); app.Log.clear(); }
        }
    }
    void Touch(int x, int y) {
        in.TouchDown = !in.Touching; in.Touching = true; in.TouchX = x; in.TouchY = y;
        Step();
    }
    void Release() {
        in.Touching = false; in.TouchUp = true;
        Step();
    }
    void Tap(int x, int y) { Touch(x, y); Release(); }

    bool Save(const std::string& name) {
        const int W = 400, H = 480;
        std::vector<uint8_t> rgba((size_t)W * H * 4);
        for (size_t i = 0; i < (size_t)W * H; i++) { rgba[i * 4] = 18; rgba[i * 4 + 1] = 18; rgba[i * 4 + 2] = 22; rgba[i * 4 + 3] = 255; }
        auto put = [&](const Canvas& c, int ox, int oy) {
            for (int y = 0; y < c.Height; y++) for (int x = 0; x < c.Width; x++) {
                uint32_t p = c.Px[(size_t)y * c.Width + x];
                size_t o = ((size_t)(oy + y) * W + (ox + x)) * 4;
                rgba[o] = (uint8_t)p; rgba[o + 1] = (uint8_t)(p >> 8); rgba[o + 2] = (uint8_t)(p >> 16); rgba[o + 3] = 255;
            }
        };
        put(app.Top, 0, 0);
        put(app.Bottom, 40, 240);
        std::string path = out + "/" + name + ".png";
        bool ok = st::writePNG(path.c_str(), W, H, rgba.data());
        printf("[shot] %s%s\n", path.c_str(), ok ? "" : " (FAILED)");
        return ok;
    }
};

static uint32_t ButtonByName(const std::string& b) {
    static const struct { const char* n; uint32_t v; } map[] = {
        {"A", BTN_A}, {"B", BTN_B}, {"X", BTN_X}, {"Y", BTN_Y}, {"L", BTN_L}, {"R", BTN_R}, {"ZL", BTN_ZL}, {"ZR", BTN_ZR},
        {"START", BTN_START}, {"SELECT", BTN_SELECT}, {"UP", BTN_UP}, {"DOWN", BTN_DOWN}, {"LEFT", BTN_LEFT}, {"RIGHT", BTN_RIGHT} };
    for (auto& m : map) if (b == m.n) return m.v;
    return 0;
}

namespace FSO { namespace Files { void HostSdSimEnable(bool on); } }

int main(int argc, char** argv) {
    if (argc < 5) {
        printf("usage: sim3ds_host <dataRoot> <cacheDir> <outDir> <script|->\n");
        return 2;
    }
    setvbuf(stdout, nullptr, _IONBF, 0);
    Host h;
    h.out = argv[3];
    App::Config cfg;
    cfg.DataRoot = argv[1];
    cfg.CacheDir = argv[2];
    cfg.ScrollBuffer = 64;
    cfg.NowMs = RealMs;
    cfg.SkipIntro = getenv("SIM3DS_INTRO") == nullptr; // the EA intro (cache/intro.stv) only when asked for
    if (const char* sr = getenv("SIM3DS_SAVEROOT")) cfg.SaveRoot = sr;
    // FIX(sims) QA: SIM3DS_SEED=<n> makes the VM random numbers repeatable
    if (const char* seed = getenv("SIM3DS_SEED")) FSO::SimAntics::VMContext::DebugSeed = strtoull(seed, nullptr, 10);
    // FIX(sims) QA: SIM3DS_ADS=<name part> prints every autonomy candidate that Sim scores (or rejects)
    if (const char* ads = getenv("SIM3DS_ADS")) {
        static std::string adsName = ads;
        FSO::SimAntics::Primitives::VMFindBestAction::DebugCandidate = [](FSO::SimAntics::VMEntity* caller, FSO::SimAntics::VMEntity* obj, const std::string& name, bool ok, float score) {
            if (caller->ToString().find(adsName) == std::string::npos) return;
            printf("   [ad] %s: %-28s on %-30s %s %.4g\n", caller->ToString().c_str(), name.c_str(), obj->ToString().c_str(), ok ? "score" : "REJECTED", score * 1000.0f);
        };
    }
    // TS1(sims): SIM3DS_LOGSTEPS=1: the 3DS log.txt step lines (time, heap, SD I/O per loading step)
    cfg.LogSteps = getenv("SIM3DS_LOGSTEPS") != nullptr;
    cfg.MemInfo = []() {
        char buf[96];
        snprintf(buf, sizeof buf, "heap live %.1f MB, peak %.1f MB", g_HeapLive / 1048576.0, g_HeapPeak / 1048576.0);
        return std::string(buf);
    };
    // TS1(sims) hardware pass: the 3DS SD card model (src/tools/host_sdsim.cpp; SIM3DS_SDSIM=<scale> also sleeps it)
    FSO::Files::HostSdSimEnable(true);
    cfg.CpuMs = []() { timespec ts; clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts); return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6; };
    if (const char* mo = getenv("SIM3DS_MAXOPEN")) FSO::Files::FilePool::MaxOpen = atoi(mo); // file handle pool size
    if (getenv("SIM3DS_NODEFER")) FSO::Files::Formats::IFF::IffFile::DEFER_HEAVY_CHUNKS = false; // A/B: read whole object iffs
    int presents = 0;
    // SIM3DS_PRESENTDIR=dir: the screens shown during blocking loads (loading progress) -> dir/present_NNN.png
    const char* presentDir = getenv("SIM3DS_PRESENTDIR");
    const int presentMax = getenv("SIM3DS_PRESENTMAX") ? atoi(getenv("SIM3DS_PRESENTMAX")) : 40; // ADDED(sims): more (save overlay)
    const int presentFrom = getenv("SIM3DS_PRESENTFROM") ? atoi(getenv("SIM3DS_PRESENTFROM")) : 0; // ADDED(sims): only from frame N
    h.app.Present = [&]() {
        presents++;
        if (presentDir && presents <= presentMax && h.frame >= presentFrom) { std::string o = h.out; h.out = presentDir; char nm[32]; snprintf(nm, sizeof nm, "present_%03d", presents); h.Save(nm); h.out = o; }
    };
    // sound: SIM3DS_AUDIO=<file.wav> records what the console would play; SIM3DS_AUDIO_LOG=1 prints every event
    if (const char* wav = getenv("SIM3DS_AUDIO")) {
        simaudio::GameAudio::Config acfg;
        acfg.DataRoot = cfg.DataRoot;
        acfg.OutputRate = 32728;
        if (simaudio::GameAudio::Get().Init(acfg)) {
            simaudio::GameAudio::Get().SetBackend(std::unique_ptr<simaudio::AudioBackend>(new simaudio::WavCaptureBackend(wav)));
            if (getenv("SIM3DS_AUDIO_LOG"))
                simaudio::GameAudio::Get().Log = [&h](const std::string& e) { printf("[snd %6.2fs] %s\n", h.frame / 30.0, e.c_str()); };
        }
    }
    double t0 = RealMs();
    if (!h.app.Init(cfg)) printf("[host] init failed (see the error screen)\n");
    printf("[host] boot %.0f ms\n", RealMs() - t0);

    std::istream* src = &std::cin;
    std::ifstream file;
    if (std::string(argv[4]) != "-") { file.open(argv[4]); if (!file) { printf("cannot open script %s\n", argv[4]); return 2; } src = &file; }
    std::string line;
    int failures = 0;
    std::string allLog;
    while (std::getline(*src, line)) {
        size_t hash = line.find('#');
        if (hash != std::string::npos) line.resize(hash);
        std::istringstream ss(line);
        std::string cmd;
        if (!(ss >> cmd)) continue;
        if (cmd == "frames") { int n = 1; ss >> n; h.Step(n); }
        else if (cmd == "ticks") {
            int n = 0; ss >> n;
            if (auto s = h.app.Session()) s->Update(n);
        }
        else if (cmd == "tap") { int x, y; ss >> x >> y; h.Tap(x, y); }
        else if (cmd == "invadd") { // TS1(sims): give the selected Sim inventory tokens: "invadd <name part> <type> <max>"
            std::string part; int type = 1, maxn = 4; ss >> part >> type >> maxn;
            auto ses = h.app.Session(); auto content = FSO::Content::Content::Get();
            if (!ses || !ses->SelectedSim() || !content || !content->Neighborhood) { printf("[host] invadd: no sim\n"); failures++; continue; }
            int nid = ses->SelectedSim()->GetPersonData(FSO::SimAntics::Model::VMPersonDataVariable::NeighborId);
            auto& inv = content->Neighborhood->InitInventoryForNID((int16_t)nid);
            auto lower = [](std::string x) { for (auto& ch : x) ch = (char)tolower((unsigned char)ch); return x; };
            std::vector<uint32_t> guids;
            if (part.size() > 2 && part[0] == '0' && part[1] == 'x') guids.push_back((uint32_t)strtoul(part.c_str(), nullptr, 16)); // "invadd 0xGUID type count"
            else for (auto& kv : content->WorldObjects->Entries) if (lower(kv.second.Name).find(lower(part)) != std::string::npos) guids.push_back((uint32_t)kv.first);
            std::sort(guids.begin(), guids.end());
            int n = 0;
            for (auto g : guids) {
                if (n >= maxn) break;
                FSO::Files::Formats::IFF::Chunks::InventoryItem it; it.Type = type; it.GUID = g; it.Count = (uint16_t)(guids.size() == 1 ? maxn : 1 + n % 3);
                bool merged = false;
                for (auto& x : inv) if (x.GUID == g && x.Type == type) { x.Count = (uint16_t)(x.Count + it.Count); merged = true; break; }
                if (!merged) inv.push_back(it);
                n++;
                printf("[host] invadd %08X %s\n", g, content->WorldObjects->Entries[g].Name.c_str());
            }
        }
        else if (cmd == "inv") { // TS1(sims) QA: the inventory (NGBH) of every family Sim on the lot: type GUID x count name
            auto ses = h.app.Session(); auto content = FSO::Content::Content::Get();
            if (ses && content && content->Neighborhood) for (auto e : ses->VM()->Entities) {
                auto av = dynamic_cast<FSO::SimAntics::VMAvatar*>(e);
                if (!av) continue;
                int nid = av->GetPersonData(FSO::SimAntics::Model::VMPersonDataVariable::NeighborId);
                auto inv = content->Neighborhood->GetInventoryByNID((int16_t)nid);
                if (!inv || inv->empty()) continue;
                printf("[inv] %s (nid %d):", av->ToString().c_str(), nid);
                for (auto& it : *inv) {
                    auto ref = content->WorldObjects->Entries.find(it.GUID);
                    printf(" [t%d %08X x%d %s]", it.Type, it.GUID, it.Count, ref != content->WorldObjects->Entries.end() ? ref->second.Name.c_str() : "?");
                }
                printf("\n");
            }
        }
        else if (cmd == "clothesdialog") { // TS1(sims): the Clothing dialog (type 6) for the selected Sim, non-blocking
            int type = 0; ss >> type;
            auto ses = h.app.Session();
            if (!ses || !ses->SelectedSim()) { printf("[host] clothesdialog: no sim\n"); failures++; continue; }
            simapp::PendingDialog d;
            d.Title = "Change Clothes"; d.Message = "Pick an outfit."; d.Type = 6; d.Block = false;
            d.AvatarID = ses->SelectedSim()->ObjectID; d.CallerID = d.AvatarID; d.OutfitType = (int16_t)type;
            ses->Dialogs.push_back(d);
            h.Step(2);
        }
        else if (cmd == "center") { // TS1(sims): scroll the lot to a tile ("center X Y")
            float x, y; ss >> x >> y;
            if (auto ses = h.app.Session()) { ses->World()->State.CenterTile(st::Vector2(x, y)); h.Step(2); }
        }
        else if (cmd == "centerfam") { // FIX(sims) QA4: scroll to the Nth on-lot Sim of family number F ("centerfam F N")
            int fam = 0, nth = 0; ss >> fam >> nth;
            if (auto ses = h.app.Session()) for (auto e : ses->VM()->Entities) {
                auto av = dynamic_cast<FSO::SimAntics::VMAvatar*>(e);
                if (!av || av->GetPersonData((FSO::SimAntics::Model::VMPersonDataVariable)61) != fam || av->Position().TileX() < 0) continue;
                if (nth-- > 0) continue;
                auto p = e->Position();
                ses->World()->State.CenterTile(st::Vector2(p.x / 16.0f, p.y / 16.0f)); h.Step(2);
                printf("[host] centerfam %d: %s (%08X) (%d,%d)\n", fam, e->ToString().c_str(), e->Object->OBJ->GUID, p.TileX(), p.TileY());
                break;
            }
        }
        else if (cmd == "centersim") { // TS1(sims) QA: scroll the lot to the Sim named NAME
            std::string name; ss >> name;
            if (auto ses = h.app.Session()) for (auto e : ses->VM()->Entities) {
                if (!dynamic_cast<FSO::SimAntics::VMAvatar*>(e) || e->ToString().find(name) == std::string::npos) continue;
                auto p = e->Position();
                ses->World()->State.CenterTile(st::Vector2(p.x / 16.0f, p.y / 16.0f)); h.Step(2);
                printf("[host] centersim %s (%d,%d)\n", name.c_str(), p.TileX(), p.TileY());
                break;
            }
        }
        else if (cmd == "tiledrag") {
            // TS1(sims): drag in the touch view between two lot tile corners (x y, level from the view, terrain
            // altitude included): "tiledrag X0 Y0 X1 Y1 N"
            float x0, y0, x1, y1; int n = 10; ss >> x0 >> y0 >> x1 >> y1 >> n;
            auto lot = h.app.Lot(); auto ses = h.app.Session();
            if (!lot || !ses) { printf("[host] tiledrag: no lot\n"); failures++; continue; }
            auto world = ses->World();
            auto bp = world->GetBlueprint();
            auto toView = [&](float tx, float ty, int& bx, int& by) {
                float alt = bp ? bp->InterpAltitude(st::Vector3(tx, ty, 0)) : 0.0f;
                auto sp = TileToScreen(*world, st::Vector3(tx, ty, (world->State.Level() - 1) * 2.95f + alt));
                lot->TopToView(sp.X, sp.Y, bx, by);
            };
            int bx0, by0, bx1, by1;
            toView(x0, y0, bx0, by0); toView(x1, y1, bx1, by1);
            printf("[host] tiledrag (%.1f,%.1f)-(%.1f,%.1f) -> view %d,%d - %d,%d\n", x0, y0, x1, y1, bx0, by0, bx1, by1);
            h.Touch(bx0, by0);
            for (int i = 1; i <= n; i++) h.Touch(bx0 + (bx1 - bx0) * i / n, by0 + (by1 - by0) * i / n);
            h.Release();
        }
        else if (cmd == "drag") {
            int x0, y0, x1, y1, n = 10; ss >> x0 >> y0 >> x1 >> y1 >> n;
            h.Touch(x0, y0);
            for (int i = 1; i <= n; i++) h.Touch(x0 + (x1 - x0) * i / n, y0 + (y1 - y0) * i / n);
            h.Release();
        }
        else if (cmd == "press") {
            std::string b; int n = 1; ss >> b >> n;
            uint32_t v = ButtonByName(b);
            if (!v) { printf("[host] unknown button %s\n", b.c_str()); continue; }
            h.in.Down = v; h.in.Held |= v;
            h.Step(std::max(1, n));
            h.in.Held &= ~v;
        }
        else if (cmd == "touch") { // ADDED(sims): "touch X Y": stylus down / moved to X,Y and kept held (shots mid-drag); "release" lifts it
            int x, y; ss >> x >> y; h.Touch(x, y);
        }
        else if (cmd == "release") { h.Release(); }
        else if (cmd == "tiletouch") { // ADDED(sims): "tiletouch TX TY": like touch, at a lot tile corner in the touch view (see tiledrag)
            float tx, ty; ss >> tx >> ty;
            auto lot = h.app.Lot(); auto ses = h.app.Session();
            if (!lot || !ses) { printf("[host] tiletouch: no lot\n"); failures++; continue; }
            auto world = ses->World(); auto bp = world->GetBlueprint();
            float alt = bp ? bp->InterpAltitude(st::Vector3(tx, ty, 0)) : 0.0f;
            auto sp = TileToScreen(*world, st::Vector3(tx, ty, (world->State.Level() - 1) * 2.95f + alt));
            int bx, by; lot->TopToView(sp.X, sp.Y, bx, by);
            h.Touch(bx, by);
        }
        else if (cmd == "cstick") { // ADDED(sims): "cstick DX DY N": the New 3DS C-stick (scrolls; shows the pointer)
            float dx, dy; int n; ss >> dx >> dy >> n;
            h.in.CStickX = dx; h.in.CStickY = dy;
            h.Step(n);
            h.in.CStickX = h.in.CStickY = 0;
        }
        else if (cmd == "pointer") { // ADDED(sims): "pointer": the top-screen pointer state (shown / alpha / cut)
            if (auto l = h.app.Lot()) printf("[host] pointer %s alpha %d at %d,%d  cut %s %d,%d\n", l->CursorShown ? "shown" : "hidden",
                                              l->PointerAlpha(), l->CursorX, l->CursorY, l->CutFollow ? "follows" : "kept", l->CutX, l->CutY);
        }
        else if (cmd == "circle") {
            float dx, dy; int n; ss >> dx >> dy >> n;
            h.in.CircleX = dx; h.in.CircleY = dy;
            h.Step(n);
            h.in.CircleX = h.in.CircleY = 0;
        }
        else if (cmd == "house") {
            int n; ss >> n; int x, y;
            if (h.app.ScriptHousePos(n, x, y)) { printf("[host] house %d row at %d,%d\n", n, x, y); h.Tap(x, y); }
            else { printf("[host] house %d not in the list\n", n); failures++; }
        }
        else if (cmd == "tapobj") {
            std::string name; std::getline(ss, name);
            while (!name.empty() && name[0] == ' ') name.erase(0, 1);
            int x, y; std::string found;
            if (h.app.ScriptObjectPos(name, x, y, &found)) { printf("[host] '%s' (%s) at touch %d,%d\n", name.c_str(), found.c_str(), x, y); h.Tap(x, y); }
            else { printf("[host] object '%s' not found / not pickable\n", name.c_str()); failures++; }
        }
        else if (cmd == "pick") {
            std::string text; std::getline(ss, text);
            while (!text.empty() && text[0] == ' ') text.erase(0, 1);
            int x, y;
            if (h.app.ScriptPieItemPos(text, x, y)) { printf("[host] pie item '%s' at %d,%d\n", text.c_str(), x, y); h.Tap(x, y); }
            else {
                printf("[host] pie item '%s' not found; pie:", text.c_str());
                for (auto& l : h.app.PieLabels()) printf(" [%s]", l.c_str());
                printf("\n");
                failures++;
            }
        }
        else if (cmd == "button") {
            std::string id; ss >> id; int x, y;
            if (h.app.ScriptButtonPos(id, x, y)) h.Tap(x, y);
            else { printf("[host] no button %s\n", id.c_str()); failures++; }
        }
        else if (cmd == "travel") { // expansion travel as Generic Sims Call 17 does it (VMTS1Travel::RequestLotSwitch)
            int lot = -1; ss >> lot;
            if (auto s = h.app.Session()) if (s->SelectedSim()) FSO::SimAntics::Engine::Utils::VMTS1Travel::RequestLotSwitch(s->VM(), s->SelectedSim(), (int16_t)lot);
            h.Step();
        }
        else if (cmd == "lotdialog") { // a TS1 lot selection dialog (types 5 downtown, 7 vacation, 8 old town, 11 studio, 13 magic)
            int t = 5; ss >> t;
            if (auto s = h.app.Session()) { PendingDialog d; d.Type = (uint8_t)t; d.Title = "Taxi"; d.Message = "Where would you like to go?"; s->Dialogs.push_back(d); }
            h.Step();
        }
        else if (cmd == "text") { // TS1(sims): queue an answer for the next CAS text entry (system keyboard on the 3DS)
            std::string rest; std::getline(ss, rest); size_t s0 = rest.find_first_not_of(' ');
            simapp::cas::CASQueueScriptedText(s0 == std::string::npos ? "" : rest.substr(s0));
        }
        else if (cmd == "chord") { // TS1(sims): hold L+R and press SELECT (the cheat console), as on the console
            h.in.Held |= BTN_L; h.in.Down = BTN_L; h.Step();
            h.in.Held |= BTN_R; h.in.Down = BTN_R; h.Step();
            h.in.Held |= BTN_SELECT; h.in.Down = BTN_SELECT; h.Step();
            h.in.Held &= ~(uint32_t)(BTN_L | BTN_R | BTN_SELECT); h.Step(3);
        }
        else if (cmd == "cheat") { // TS1(sims): "cheat TEXT" = chord + TEXT typed into the console keyboard
            std::string rest; std::getline(ss, rest); size_t s0 = rest.find_first_not_of(' ');
            simapp::cas::CASQueueScriptedText(s0 == std::string::npos ? "" : rest.substr(s0));
            if (!h.app.Cheats.Open) {
                h.in.Held |= BTN_L | BTN_R; h.in.Down = BTN_L | BTN_R; h.Step();
                h.in.Held |= BTN_SELECT; h.in.Down = BTN_SELECT; h.Step();
                h.in.Held &= ~(uint32_t)(BTN_L | BTN_R | BTN_SELECT); h.Step(3);
            } else { h.in.Down = BTN_A; h.Step(3); } // the box is open: A types another cheat
        }
        else if (cmd == "budget") { if (auto s = h.app.Session()) printf("[host] budget %d\n", s->Budget()); }
        else if (cmd == "buttons") { // TS1(sims): list the button ids of the current screen (lot screen hits)
            if (auto l = h.app.Lot()) { printf("[host] buttons:"); for (auto& hh : l->Hits) printf(" %s", hh.Id.c_str()); printf("\n"); }
        }
        else if (cmd == "buyfind") { // TS1(sims): pick up the first item of the open buy category whose name contains TEXT
            std::string text; std::getline(ss, text); while (!text.empty() && text[0] == ' ') text.erase(0, 1);
            auto l = h.app.Lot(); bool ok = false;
            auto lower = [](std::string x) { for (auto& ch : x) ch = (char)tolower((unsigned char)ch); return x; };
            if (l && l->Buy) {
                auto& cur = l->Buy->Current;
                for (size_t i = 0; i < cur.size() && !ok; i++) {
                    auto& d = simapp::UICatalog::Get().GetDetails(*cur[i]);
                    if (lower(d.Name).find(lower(text)) != std::string::npos || lower(cur[i]->Name).find(lower(text)) != std::string::npos) {
                        ok = l->Buy->Select((int)i);
                        printf("[host] buyfind '%s': %s ($%u) %s\n", text.c_str(), d.Name.c_str(), cur[i]->Price, ok ? "holding" : "refused");
                    }
                }
            }
            if (!ok) { printf("[host] buyfind '%s': not found\n", text.c_str()); failures++; }
            h.Step();
        }
        else if (cmd == "buysweep") { // FIX(sims) REL (host only): pick up every item of the 8 function categories once
            auto l = h.app.Lot(); auto ses = h.app.Session();
            if (!l || !l->Buy || !ses) { printf("[host] buysweep: not in buy mode\n"); failures++; continue; }
            int oldBudget = ses->Budget(); ses->SetBudget(1000000);
            int total = 0, bad = 0;
            for (int c = 0; c < 8; c++) {
                l->Buy->Tap("cat" + std::to_string(c));
                auto cur = l->Buy->Current;
                for (size_t i = 0; i < cur.size(); i++) {
                    total++;
                    if (l->Buy->Select((int)i)) { l->Buy->Tap("hold_cancel"); continue; }
                    bad++;
                    auto wo = FSO::Content::Content::Get()->WorldObjects.get();
                    auto def = wo->Get(cur[i]->GUID, true);
                    auto ent = wo->Entries.find(cur[i]->GUID);
                    {
                        auto grp = ses->VM()->Context->CreateObjectInstance(cur[i]->GUID, FSO::LotView::Model::LotTilePos::OUT_OF_WORLD, FSO::LotView::Model::Direction::NORTH, true, true);
                        printf("[buysweep]   direct create: group %s, %zu objects, multitile %d\n", grp ? "ok" : "null", grp ? grp->Objects.size() : (size_t)0, def ? (int)def->OBJ->MasterID : -1);
                        if (grp) for (auto o : grp->Objects) printf("[buysweep]   obj %s dead %d\n", o->ToString().c_str(), (int)o->Dead);
                    }
                    printf("[buysweep] cat%d #%zu cannot be picked up: %s (%08X, %s) def %s entry %s\n", c, i, simapp::UICatalog::Get().GetDetails(*cur[i]).Name.c_str(), cur[i]->GUID, cur[i]->File.c_str(),
                           def ? "ok" : "null", ent != wo->Entries.end() ? (ent->second.FileName + " far " + std::to_string(ent->second.FarIndex)).c_str() : "none");
                }
                h.Step();
            }
            ses->SetBudget(oldBudget);
            printf("[buysweep] %d catalogue entries, %d cannot be picked up\n", total, bad);
        }
        else if (cmd == "buylist") { // TS1(sims) QA: the items of the open buy category
            if (auto l = h.app.Lot()) if (l->Buy) {
                printf("[buylist] %zu:", l->Buy->Current.size());
                for (auto it : l->Buy->Current) printf(" [%s]", simapp::UICatalog::Get().GetDetails(*it).Name.c_str());
                printf("\n");
            }
        }
        else if (cmd == "sims") { // TS1(sims): every Sim on the lot: motives + current action
            if (auto s = h.app.Session()) for (auto e : s->VM()->Entities) {
                auto av = dynamic_cast<FSO::SimAntics::VMAvatar*>(e);
                if (!av) continue;
                using M = FSO::SimAntics::Model::VMMotive;
                std::string act = (av->Thread && !av->Thread->Queue.empty()) ? av->Thread->Queue[0]->Name : std::string("-");
                // FIX(sims) QA: the rest of the queue (+ its priority) after the running action
                if (av->Thread) for (size_t qi = 1; qi < av->Thread->Queue.size(); qi++) act += " | " + av->Thread->Queue[qi]->Name + "(" + std::to_string((int)av->Thread->Queue[qi]->Priority) + ")";
                auto p = av->Position();
                printf("[sim] %-12s pri %d date %d type %d greet %d mood %4d hun %4d com %4d hyg %4d bla %4d ene %4d fun %4d soc %4d room %4d @(%d,%d) %s\n", av->ToString().c_str(), av->GetPersonData((FSO::SimAntics::Model::VMPersonDataVariable)33), av->GetPersonData((FSO::SimAntics::Model::VMPersonDataVariable)77), av->GetPersonData((FSO::SimAntics::Model::VMPersonDataVariable)32), av->GetPersonData((FSO::SimAntics::Model::VMPersonDataVariable)34),
                       av->GetMotiveData(M::Mood), av->GetMotiveData(M::Hunger), av->GetMotiveData(M::Comfort), av->GetMotiveData(M::Hygiene),
                       av->GetMotiveData(M::Bladder), av->GetMotiveData(M::Energy), av->GetMotiveData(M::Fun), av->GetMotiveData(M::Social),
                       av->GetMotiveData(M::Room), p.TileX(), p.TileY(), act.c_str());
            }
        }
        else if (cmd == "piedebug") { // TS1(sims) QA: every TTAB entry of NAME for the selected Sim: flags, check result
            std::string name; std::getline(ss, name); while (!name.empty() && name[0] == ' ') name.erase(0, 1);
            auto s = h.app.Session(); auto sim = s ? s->SelectedSim() : nullptr;
            if (sim) for (auto e : s->VM()->Entities) {
                bool any = name == "*" || name == "**";
                if ((!any && e->ToString().find(name) == std::string::npos) || e == sim || !e->TreeTable) continue;
                if (any && !dynamic_cast<FSO::SimAntics::VMAvatar*>(e)) continue;
                printf("[piedebug] %s: %zu TTAB entries (caller %s type %d greet %d)\n", e->ToString().c_str(), e->TreeTable->Interactions.size(), sim->ToString().c_str(),
                       sim->GetPersonData((FSO::SimAntics::Model::VMPersonDataVariable)32), sim->GetPersonData((FSO::SimAntics::Model::VMPersonDataVariable)34));
                for (auto& ia : e->TreeTable->Interactions) {
                    auto act = e->GetAction((int32_t)ia->TTAIndex, sim, s->VM()->Context);
                    sim->ObjectData[(int)FSO::SimAntics::Model::VMStackObjectVariable::HideInteraction] = 0;
                    auto r = act ? sim->Thread->CheckAction(act) : nullptr;
                    std::string nm = e->TreeTableStrings ? e->TreeTableStrings->GetString((int32_t)ia->TTAIndex) : "?";
                    if (name != "**" || r) printf("   %-28s flags %08X check %s hide %d (test %04X action %04X)\n", nm.c_str(), (unsigned)ia->Flags, !act ? "no-action" : (r ? "ok" : "FAIL"),
                           sim->ObjectData[(int)FSO::SimAntics::Model::VMStackObjectVariable::HideInteraction], ia->TestFunction, ia->ActionFunction);
                    if (r) { printf("      ads:"); for (auto& m : ia->ActiveMotiveEntries) printf(" m%d %d+%d p%d", m.MotiveIndex, m.EffectRangeMinimum, m.EffectRangeMaximum, m.PersonalityModifier); printf(" thr %u\n", ia->AutonomyThreshold); }
                }
                if (name != "**") break;
            }
        }
        else if (cmd == "waitq") { // FIX(sims) QA: "waitq N TEXT": step until no queued action of the selected Sim contains TEXT (max N frames)
            int n = 900; ss >> n; std::string text; std::getline(ss, text); while (!text.empty() && text[0] == ' ') text.erase(0, 1);
            int f = 0;
            for (; f < n; f++) {
                auto s = h.app.Session(); auto sim = s ? s->SelectedSim() : nullptr;
                bool found = false;
                if (sim && sim->Thread) for (auto& q : sim->Thread->Queue) if (q->Name.find(text) != std::string::npos) found = true;
                if (!found) break;
                h.Step();
            }
            printf("[host] waitq '%s': %d frames\n", text.c_str(), f);
        }
        else if (cmd == "pie") { // FIX(sims) QA: "pie NAME": the pie menu entries (as the player sees them) of NAME for the selected Sim
            std::string name; std::getline(ss, name); while (!name.empty() && name[0] == ' ') name.erase(0, 1);
            auto s = h.app.Session(); auto sim = s ? s->SelectedSim() : nullptr;
            if (sim) for (auto e : s->VM()->Entities) {
                if (e->ToString().find(name) == std::string::npos || e == sim) continue;
                auto target = (e->MultitileGroup != nullptr) ? e->MultitileGroup->GetInteractionGroupLeader(e) : e;
                for (auto& m : target->GetPieMenu(s->VM(), sim, false)) printf("[pie] %s: '%s' (id %d)\n", target->ToString().c_str(), m.Name.c_str(), (int)m.ID);
                break;
            }
        }
        else if (cmd == "piefind") { // FIX(sims) REL (host only): every object whose pie (for the selected Sim) has an entry containing TEXT
            std::string text; std::getline(ss, text); while (!text.empty() && text[0] == ' ') text.erase(0, 1);
            auto s = h.app.Session(); auto sim = s ? s->SelectedSim() : nullptr;
            std::set<void*> seen;
            if (sim) for (auto e : s->VM()->Entities) {
                if (e == sim || e->Position().TileX() < 0) continue;
                auto target = (e->MultitileGroup != nullptr) ? e->MultitileGroup->GetInteractionGroupLeader(e) : e;
                if (!seen.insert(target).second) continue;
                for (auto& m : target->GetPieMenu(s->VM(), sim, false))
                    if (m.Name.find(text) != std::string::npos) printf("[piefind] %s: '%s'\n", target->ToString().c_str(), m.Name.c_str());
            }
        }
        else if (cmd == "rel") { // FIX(sims) QA: the selected Sim's TS1 relationships (daily, lifetime, flags) to every Sim on the lot
            auto s = h.app.Session(); auto sim = s ? s->SelectedSim() : nullptr;
            using NB = FSO::SimAntics::Engine::Utils::VMTS1Neighbourhood;
            if (sim) for (auto e : s->VM()->Entities) {
                auto av = dynamic_cast<FSO::SimAntics::VMAvatar*>(e);
                if (!av || av == sim) continue;
                auto r = NB::GetRelationship(NB::NeighborIDOf(sim), NB::NeighborIDOf(av), false);
                auto b = NB::GetRelationship(NB::NeighborIDOf(av), NB::NeighborIDOf(sim), false);
                if (!r && !b) continue;
                printf("[rel] %s -> %s: %d/%d flags %d | back %d/%d\n", sim->ToString().c_str(), av->ToString().c_str(),
                       r && r->size() > 0 ? (*r)[0] : 0, r && r->size() > 1 ? (*r)[1] : 0, r && r->size() > 2 ? (*r)[2] : 0,
                       b && b->size() > 0 ? (*b)[0] : 0, b && b->size() > 1 ? (*b)[1] : 0);
            }
        }
        else if (cmd == "relset") { // QA2 (host only): "relset NAME daily lifetime" sets the TS1 relationship both ways between the selected Sim and NAME
            std::string name; int d = 0, l = 0; ss >> name >> d >> l;
            if (name == "@") name = g_LastPicked.empty() ? std::string("\x01") : g_LastPicked; // FIX(sims) REL
            auto s = h.app.Session(); auto sim = s ? s->SelectedSim() : nullptr;
            using NB = FSO::SimAntics::Engine::Utils::VMTS1Neighbourhood;
            if (sim) for (auto e : s->VM()->Entities) {
                auto av = dynamic_cast<FSO::SimAntics::VMAvatar*>(e);
                if (!av || av == sim || av->ToString().find(name) == std::string::npos) continue;
                for (int k = 0; k < 2; k++) {
                    auto r = k == 0 ? NB::GetRelationship(NB::NeighborIDOf(sim), NB::NeighborIDOf(av), true) : NB::GetRelationship(NB::NeighborIDOf(av), NB::NeighborIDOf(sim), true);
                    if (!r) continue;
                    if (r->size() < 2) r->resize(2);
                    (*r)[0] = (int16_t)d; (*r)[1] = (int16_t)l;
                }
                printf("[host] relset %s <-> %s: %d/%d\n", sim->ToString().c_str(), av->ToString().c_str(), d, l);
                break;
            }
        }
        else if (cmd == "waitmap") { // QA2 (host only): "waitmap N": step until a lot selection map is open (max N frames)
            int n = 600; ss >> n; int k = 0;
            while (k < n && !(h.app.Front() && h.app.Front()->Picking())) { h.Step(); k++; }
            printf("[host] waitmap: %s after %d frames\n", (h.app.Front() && h.app.Front()->Picking()) ? "open" : "not open", k);
        }
        else if (cmd == "trace") { std::string n; std::getline(ss, n); while (!n.empty() && n[0] == ' ') n.erase(0, 1); g_TraceName = n; } // QA2: start/stop (empty) tracing an entity
        else if (cmd == "census") { // TS1(sims): entity counts by name (top 12), to find objects piling up
            if (auto s = h.app.Session()) {
                std::map<std::string, int> c;
                for (auto e : s->VM()->Entities) c[e->ToString()]++;
                std::vector<std::pair<int, std::string>> v;
                for (auto& kv : c) v.push_back({ kv.second, kv.first });
                std::sort(v.rbegin(), v.rend());
                printf("[census]");
                for (size_t i = 0; i < v.size() && i < 12; i++) printf(" %s x%d;", v[i].second.c_str(), v[i].first);
                printf("\n");
            }
        }
        else if (cmd == "autodlg") { std::string v; ss >> v; h.autoDialogs = v == "on"; }
        else if (cmd == "clock") { // TS1(sims): game clock + motives of the family
            if (auto s = h.app.Session()) {
                auto vm = s->VM();
                printf("[host] clock day %d %02d:%02d, budget %d, resets %d, entities %zu, anim keyframes %.1f MB\n", (int)vm->Context->Clock->DayOfMonth, (int)vm->Context->Clock->Hours, (int)vm->Context->Clock->Minutes, s->Budget(), s->Resets, vm->Entities.size(), FSO::Vitaboy::Animation::KeyframeBytesLoaded() / 1048576.0);
            }
        }
        else if (cmd == "boot") { if (auto f = h.app.Front()) f->FinishBootNow(); h.Step(); }   // front end: skip the loading screen
        else if (cmd == "mem") { // mem LABEL | mem reset: C++ heap live/peak, malloc arena, SD-card I/O so far
            std::string label; ss >> label;
            if (label == "reset") { g_HeapPeak = g_HeapLive; continue; }
            struct mallinfo2 mi = mallinfo2();
            auto& io = FSO::Files::FilePool::Stats;
            auto& fs = FSO::Files::Fs;
            printf("[mem] %-14s live %6.1f MB  peak %6.1f MB  malloc-in-use %6.1f MB  | io %7.1f MB read, %llu reads, %llu seeks, %llu opens, est. SD %.1f s"
                   " | fs %llu opens, %llu stats, %llu dirs, %llu reads, %.1f MB, 3DS model %.1f s\n",
                   label.c_str(), g_HeapLive / 1048576.0, g_HeapPeak / 1048576.0, mi.uordblks / 1048576.0,
                   io.BytesRead / 1048576.0, (unsigned long long)io.Reads, (unsigned long long)io.Seeks, (unsigned long long)io.Opens,
                   io.EstimatedSdMs() / 1000.0, (unsigned long long)fs.Opens, (unsigned long long)fs.Stats, (unsigned long long)fs.DirOpens,
                   (unsigned long long)fs.Reads, fs.BytesRead / 1048576.0, fs.TotalMs() / 1000.0);
        }
        else if (cmd == "memprof") { // FIX(sims) memory QA: memprof LABEL -> $SIM3DS_HEAPPROF/LABEL.txt (bytes + exe-relative stack)
            std::string label; ss >> label;
            if (heapprof::on && heapprof::stacks) {
                heapprof::busy = true;
                Dl_info di; dladdr((void*)&RealMs, &di);
                uintptr_t base = (uintptr_t)di.dli_fbase;
                std::string fn = std::string(getenv("SIM3DS_HEAPPROF")) + "/" + label + ".txt";
                FILE* f = fopen(fn.c_str(), "w");
                for (size_t i = 0; f && i < heapprof::stacks->size(); i++) {
                    long long b = (*heapprof::bytes)[i];
                    if (b < 4096) continue;
                    fprintf(f, "%lld", b);
                    auto& st = (*heapprof::stacks)[i];
                    for (int k = 2; k < st.n; k++) fprintf(f, " %lx", (unsigned long)((uintptr_t)st.f[k] - base));
                    fprintf(f, "\n");
                }
                if (f) fclose(f);
                heapprof::busy = false;
                printf("[memprof] %s -> %s\n", label.c_str(), fn.c_str());
            }
        }
        else if (cmd == "front") { if (auto f = h.app.Front()) printf("[host] %s\n", f->Describe().c_str()); }
        else if (cmd == "shot") { std::string n; ss >> n; h.Save(n); }
        else if (cmd == "record") {
            std::string n; int every = 1, count = 1; ss >> n >> every >> count;
            for (int i = 0; i < count; i++) {
                h.Step(every);
                char nm[256]; snprintf(nm, sizeof nm, "%s_%03d", n.c_str(), i);
                h.Save(nm);
            }
        }
        else if (cmd == "stats") { std::string v; ss >> v; h.app.ShowStats(v == "on"); }
        else if (cmd == "log") {
            printf("[host] queue: %s\n", h.app.DescribeQueue().c_str());
            if (h.app.PieOpen()) { printf("[host] pie:"); for (auto& l : h.app.PieLabels()) printf(" [%s]", l.c_str()); printf("\n"); }
        }
        else if (cmd == "testdialog") { // shows a VM-style dialog (UI check only; not tied to a thread)
            if (auto s = h.app.Session()) {
                PendingDialog d;
                d.Title = "Telephone"; d.Message = "This is a test of the dialog panel that SimAntics dialog primitives use (VMDialogInfo): title, a wrapped message and the Yes/No buttons.";
                d.Yes = "Yes"; d.HasYes = true; d.No = "No"; d.HasNo = true;
                s->Dialogs.push_back(d);
                h.Step();
            }
        }
        else if (cmd == "spawn") { // spawn GUIDHEX DX DY: object created next to the selected Sim (effects tests)
            std::string g; int dx = 1, dy = 0; ss >> g >> dx >> dy;
            auto s = h.app.Session();
            auto sim = s ? s->SelectedSim() : nullptr;
            if (sim) {
                auto p = sim->Position();
                auto grp = s->VM()->Context->CreateObjectInstance((uint32_t)strtoul(g.c_str(), nullptr, 16),
                    FSO::LotView::Model::LotTilePos::FromBigTile((int16_t)(p.TileX() + dx), (int16_t)(p.TileY() + dy), p.Level),
                    FSO::LotView::Model::Direction::NORTH, true);
                printf("[host] spawn %s at (%d,%d): %s\n", g.c_str(), p.TileX() + dx, p.TileY() + dy, grp ? "ok" : "failed");
            }
        }
        else if (cmd == "use") { // use NAME TEXT: queue the pie entry containing TEXT of the object NAME on the selected Sim
            std::string name, text; ss >> name; std::getline(ss, text); if (!text.empty() && text[0] == ' ') text = text.substr(1);
            auto s = h.app.Session();
            auto sim = s ? s->SelectedSim() : nullptr;
            bool ok = false;
            // FIX(sims) QA: NAME "*" = any other Sim offering TEXT (remembered), "@" = the Sim "*" picked last
            std::string& lastPicked = g_LastPicked;
            bool anySim = name == "*";
            // FIX(sims) QA4: NAME "%F" = any other Sim of family number F (e.g. %2000 = a downtownie), remembered like "*"
            int famOnly = (name.size() > 1 && name[0] == '%') ? atoi(name.c_str() + 1) : 0;
            if (famOnly) anySim = true;
            if (name == "@") name = lastPicked.empty() ? std::string("\x01") : lastPicked;
            if (sim) for (auto e : s->VM()->Entities) {
                if (anySim ? dynamic_cast<FSO::SimAntics::VMAvatar*>(e) == nullptr : e->ToString().find(name) == std::string::npos) continue;
                if (famOnly && static_cast<FSO::SimAntics::VMAvatar*>(e)->GetPersonData((FSO::SimAntics::Model::VMPersonDataVariable)61) != famOnly) continue;
                if (e == sim) continue;
                if (anySim) lastPicked = e->ToString();
                auto target = (e->MultitileGroup != nullptr) ? e->MultitileGroup->GetInteractionGroupLeader(e) : e;
                for (auto& m : target->GetPieMenu(s->VM(), sim, false)) {
                    if (m.Name.find(text) == std::string::npos) continue;
                    PieChoice c; c.Name = m.Name; c.ID = m.ID; c.Param0 = m.Param0;
                    ok = s->PushInteraction(target, false, c);
                    printf("[host] use %s / %s: %s\n", target->ToString().c_str(), m.Name.c_str(), ok ? "queued" : "failed");
                    break;
                }
                if (ok) break;
            }
            if (!ok) printf("[host] use %s %s: not found\n", name.c_str(), text.c_str());
        }
        else if (cmd == "moneyhead") { // moneyhead N: the VM's money-over-head on the selected Sim
            int n = 0; ss >> n;
            if (auto s = h.app.Session()) if (auto sim = s->SelectedSim()) sim->ShowMoneyHeadline(n);
        }
        else if (cmd == "cursor") { // cursor X Y: put the top-screen cursor at X,Y (as the circle pad would)
            int x = 0, y = 0; ss >> x >> y;
            if (auto l = h.app.Lot()) { l->CursorX = x; l->CursorY = y; l->ShowPointer(); }
        }
        else if (cmd == "attrs") { // TS1(sims) QA: attributes 0..N-1 of every entity whose name contains NAME ("attrs N NAME")
            int n = 8; std::string name; ss >> n; std::getline(ss, name); while (!name.empty() && name[0] == ' ') name.erase(0, 1);
            if (auto s = h.app.Session()) for (auto e : s->VM()->Entities) {
                uint32_t guid = e->Object ? e->Object->OBJ->GUID : 0;
                if (name.size() > 2 && name[0] == '0' && name[1] == 'x') { if (guid != (uint32_t)strtoul(name.c_str(), nullptr, 16)) continue; }
                else if (e->ToString().find(name) == std::string::npos) continue;
                printf("[attrs] #%d %s (%08X):", e->ObjectID, e->ToString().c_str(), guid);
                for (int i = 0; i < n; i++) printf(" %d", e->GetAttribute(i));
                printf("\n");
            }
        }
        else if (cmd == "stack") { // H1: main-thread stack (routine:ip) + attrs 0..7 of entities whose name contains NAME
            std::string name; std::getline(ss, name); while (!name.empty() && name[0] == ' ') name.erase(0, 1);
            if (auto s = h.app.Session()) for (auto e : s->VM()->Entities) {
                if (e->ToString().find(name) == std::string::npos || e->Thread == nullptr) continue;
                auto p = e->Position();
                printf("[stack] #%d %s @(%d,%d) raw(%d,%d) attrs", e->ObjectID, e->ToString().c_str(), p.TileX(), p.TileY(), (int)p.x, (int)p.y);
                for (int i = 0; i < 8; i++) printf(" %d", e->GetAttribute(i));
                printf(" pr%d/%d uc%d :", e->GetValue(FSO::SimAntics::Model::VMStackObjectVariable::PrimitiveResult),
                       e->GetValue(FSO::SimAntics::Model::VMStackObjectVariable::PrimitiveResultID), e->GetValue(FSO::SimAntics::Model::VMStackObjectVariable::UseCount));
                for (auto& f : e->Thread->Stack) {
                    std::string rn = f->Routine ? f->Routine->Rti.Name : std::string("?");
                    if (rn.empty() && f->Routine && f->Routine->Chunk) rn = f->Routine->Chunk->ChunkLabel;
                    printf(" %s#%04X:%d", rn.c_str(), f->Routine ? f->Routine->ID : 0, (int)f->InstructionPointer);
                }
                printf("\n");
            }
        }
        else if (cmd == "pdata") { // TS1(sims) QA: person data of the Sims named NAME: "pdata NAME i j k ..."
            std::string name; ss >> name; std::vector<int> idx; int v; while (ss >> v) idx.push_back(v);
            if (auto s = h.app.Session()) for (auto e : s->VM()->Entities) {
                auto av = dynamic_cast<FSO::SimAntics::VMAvatar*>(e);
                if (!av || av->ToString().find(name) == std::string::npos) continue;
                printf("[pdata] %s:", av->ToString().c_str());
                for (int i : idx) printf(" pd%d=%d", i, av->GetPersonData((FSO::SimAntics::Model::VMPersonDataVariable)i));
                printf("\n");
            }
        }
        else if (cmd == "pdset") { // TS1(sims) QA: set person data: "pdset NAME index value"
            std::string name; int idx = 0, val = 0; ss >> name >> idx >> val;
            if (auto s = h.app.Session()) for (auto e : s->VM()->Entities) {
                auto av = dynamic_cast<FSO::SimAntics::VMAvatar*>(e);
                if (!av || av->ToString().find(name) == std::string::npos) continue;
                av->SetPersonData((FSO::SimAntics::Model::VMPersonDataVariable)idx, (int16_t)val);
                printf("[pdset] %s pd%d = %d\n", av->ToString().c_str(), idx, val);
            }
        }
        else if (cmd == "motives") { // QA3 (host only): "motives NAME value" sets energy, comfort, hunger, hygiene, bladder, social, fun of NAME
            std::string name; int val = 0; ss >> name >> val;
            if (auto s = h.app.Session()) for (auto e : s->VM()->Entities) {
                auto av = dynamic_cast<FSO::SimAntics::VMAvatar*>(e);
                if (!av || av->ToString().find(name) == std::string::npos) continue;
                for (int m : { 5, 6, 7, 8, 9, 14, 15 }) av->SetMotiveData((FSO::SimAntics::Model::VMMotive)m, (int16_t)val);
                printf("[motives] %s = %d\n", av->ToString().c_str(), val);
            }
        }
        else if (cmd == "findguid") { // TS1(sims) QA: what Set To Next "object of type GUID" sees
            std::string g; ss >> g;
            if (auto s = h.app.Session()) {
                auto v = s->VM()->Context->SetToNextCache->GetObjectsByGUID((uint32_t)strtoul(g.c_str(), nullptr, 16));
                printf("[findguid] %s:", g.c_str());
                if (v) for (auto e : *v) printf(" #%d%s", e->ObjectID, e->Dead ? "(dead)" : "");
                printf("\n");
            }
        }
        else if (cmd == "tatt") { // TS1(sims) QA: the neighbourhood type attributes (TATT) of the objects named NAME
            std::string name; std::getline(ss, name); while (!name.empty() && name[0] == ' ') name.erase(0, 1);
            auto content = FSO::Content::Content::Get();
            std::set<uint32_t> done;
            if (auto s = h.app.Session()) for (auto e : s->VM()->Entities) {
                if (!e->Object || e->ToString().find(name) == std::string::npos || !done.insert(e->Object->OBJ->GUID).second) continue;
                printf("[tatt] %s (%08X):", e->ToString().c_str(), e->Object->OBJ->GUID);
                for (int i = 0; i < 32; i++) printf(" %d", content->Neighborhood->GetTATT(e->Object->OBJ->GUID, i));
                printf("\n");
            }
        }
        else if (cmd == "list") { // entities on the lot ("list all": also the out-of-world ones)
            std::string extra; ss >> extra;
            if (auto s = h.app.Session()) for (auto e : s->VM()->Entities) {
                auto p = e->Position();
                if (p.TileX() < 0) { // H1: out-of-world entities too (multitile leads, held objects), with their main thread
                    if (extra == "all") printf("  #%d %s (out of world)\n", e->ObjectID, e->ToString().c_str());
                    continue;
                }
                printf("  #%d %s @(%d,%d,L%d)\n", e->ObjectID, e->ToString().c_str(), p.TileX(), p.TileY(), (int)p.Level);
            }
        }
        else if (cmd == "bench") {
            int n = 60; ss >> n;
            double vm = 0, world = 0, ui = 0, total = 0; int upd = 0;
#ifdef HAVE_CALLGRIND
            CALLGRIND_ZERO_STATS;
#endif
            for (int i = 0; i < n; i++) {
                h.Step();
                vm += h.app.Stats.Vm; world += h.app.Stats.World; ui += h.app.Stats.Ui; total += h.app.Stats.Total; upd += h.app.Stats.Updates;
            }
#ifdef HAVE_CALLGRIND
            { char nm[64]; snprintf(nm, sizeof nm, "bench_speed%d", h.app.Speed); CALLGRIND_DUMP_STATS_AT(nm); }
#endif
            printf("[bench] %d frames, speed %d: avg total %.2f ms = VM %.2f ms (%.1f updates) + world draw %.2f ms + UI/compose %.2f ms\n",
                   n, h.app.Speed, total / n, vm / n, (double)upd / n, world / n, ui / n);
        }
        else if (cmd == "waithouse" || cmd == "expecthouse") {
            // H1: "waithouse N H": step up to N frames until the lot loaded is house H (global 10), e.g. a phone trip
            // home; "expecthouse H": counts a failure unless the lot loaded now is house H.
            int n = 0, want = 0;
            if (cmd == "waithouse") ss >> n;
            ss >> want;
            auto cur = [&]() -> int { auto s = h.app.Session(); return (s && s->VM()) ? s->VM()->GetGlobalValue(10) : -1; };
            int k = 0;
            while (k < n && cur() != want) { h.Step(); k++; }
            bool ok = cur() == want;
            if (cmd == "waithouse") printf("[host] waithouse %d: %s after %d frames\n", want, ok ? "there" : "not there", k);
            else if (!ok) { printf("[host] EXPECT FAILED: house %d expected, lot is %d\n", want, cur()); failures++; }
            else printf("[host] expect ok: house %d\n", want);
        }
        else if (cmd == "expect") {
            std::string text; std::getline(ss, text);
            while (!text.empty() && text[0] == ' ') text.erase(0, 1);
            std::string q = h.app.DescribeQueue();
            if (q.find(text) == std::string::npos) { printf("[host] EXPECT FAILED: '%s' not in queue: %s\n", text.c_str(), q.c_str()); failures++; }
            else printf("[host] expect ok: '%s'\n", text.c_str());
        }
        else printf("[host] unknown command '%s'\n", cmd.c_str());
        if (h.app.QuitRequested) { printf("[host] app requested quit\n"); break; }
    }
    if (auto s = h.app.Session()) printf("[host] VM updates %lld, object resets %d\n", (long long)s->Updates, s->Resets);
    printf("[host] %d frames, memory high-water %ld KB, %d failures\n", h.frame, HighWaterKB(), failures);
    if (auto cap = dynamic_cast<simaudio::WavCaptureBackend*>(simaudio::GameAudio::Get().Backend()))
        printf("[host] audio: %.1f s captured, peak %d\n", cap->SecondsWritten(), cap->Peak());
    simaudio::GameAudio::Get().Shutdown();
    return failures ? 3 : 0;
}
