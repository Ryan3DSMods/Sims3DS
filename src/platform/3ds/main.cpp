/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims — Nintendo 3DS entry point (thin platform layer).
//
// Everything game-related lives in src/app (shared with the host simulator src/tools/sim3ds_host.cpp); this
// file only does what needs libctru:
//   * New 3DS: 804 MHz + L2 cache (osSetSpeedupEnable), both screens as BGR8 framebuffers;
//   * input: hid buttons / touch / circle pad (+ ZL/ZR/C-stick, ir:rst on the New 3DS) -> simapp::AppInput;
//   * output: the app's 32-bit canvases are rotated into libctru's portrait framebuffers
//     (column-major, x -> column, y flipped: dst[(x * 240 + (239 - y)) * 3] = B,G,R);
//   * time: the ARM11 system tick.
//
// SD card layout: see src/app/README.md (sdmc:/3ds/sims/GameData/..., UserData/...).
#include <3ds.h>
#include <3ds/allocator/mappable.h>
#include <sys/stat.h>
#include <sys/iosupport.h>
#include <type_traits>
#include <cstdio>
#include <cstring>
#include <exception>
#include <malloc.h>
#include <new>
#include <string>
#include "app/App.h"
#include "audio/GameAudio.h"
#include "platform/3ds/AudioNdsp.h"
#include "files/FilePool.h"

using namespace simapp;

// TS1(sims): libctru splits the free memory into the app heap (malloc/new) and the linear heap (GPU/DSP-visible),
// giving the linear heap up to 32 MB by default. This app draws in software: linear memory only holds the framebuffers
// (~1.5 MB) and the NDSP wave buffers, so 8 MB is plenty and the game gets ~24 MB more heap (HBL: ~88 -> ~112 MB).
extern "C" { u32 __ctru_linear_heap_size = 8 * 1024 * 1024; }

// FIX(sims): replaces libctru's weak __system_allocateHeaps. With only __ctru_linear_heap_size set, libctru gives the
// app heap ALL remaining memory - but the heap area (OS_HEAP_AREA_BEGIN..END) holds at most 96 MB, so in the CIA's
// 178 MB mode (and large Homebrew Launcher takeovers) svcControlMemory failed and the app stopped in svcBreak before
// main(). Here the app heap is capped to the heap area and the rest goes to the linear heap (max 32 MB, like libctru).
extern "C" {
extern u32 __ctru_heap, __ctru_heap_size, __ctru_linear_heap;
extern char* fake_heap_start;
extern char* fake_heap_end;
void __system_allocateHeaps(void) {
    Handle reslimit = 0;
    if (R_FAILED(svcGetResourceLimit(&reslimit, CUR_PROCESS_HANDLE))) svcBreak(USERBREAK_PANIC);
    s64 maxCommit = 0, currentCommit = 0;
    ResourceLimitType reslimitType = RESLIMIT_COMMIT;
    svcGetResourceLimitLimitValues(&maxCommit, reslimit, &reslimitType, 1);
    svcGetResourceLimitCurrentValues(&currentCommit, reslimit, &reslimitType, 1);
    svcCloseHandle(reslimit);
    u32 remaining = (u32)(maxCommit - currentCommit) & ~0xFFFu;

    const u32 heapAreaMax = (u32)(OS_HEAP_AREA_END - OS_HEAP_AREA_BEGIN); // 96 MB
    const u32 linearCap = 32u << 20;
    u32 linear = 8u << 20;
    if (linear > remaining / 2) linear = (remaining / 2) & ~0xFFFu;
    u32 heap = remaining - linear;
    if (heap > heapAreaMax) {
        heap = heapAreaMax;
        linear = remaining - heap;
        if (linear > linearCap) linear = linearCap;
    }
    __ctru_heap_size = heap;
    __ctru_linear_heap_size = linear;

    if (R_FAILED(svcControlMemory(&__ctru_heap, OS_HEAP_AREA_BEGIN, 0x0, __ctru_heap_size, MEMOP_ALLOC,
                                  (MemPerm)(MEMPERM_READ | MEMPERM_WRITE))))
        svcBreak(USERBREAK_PANIC);
    if (R_FAILED(svcControlMemory(&__ctru_linear_heap, 0x0, 0x0, __ctru_linear_heap_size, MEMOP_ALLOC_LINEAR,
                                  (MemPerm)(MEMPERM_READ | MEMPERM_WRITE))))
        svcBreak(USERBREAK_PANIC);
    mappableInit(OS_MAP_AREA_BEGIN, OS_MAP_AREA_END);
    fake_heap_start = (char*)__ctru_heap;
    fake_heap_end = fake_heap_start + __ctru_heap_size;
}
}

// TS1(sims) hardware pass: every file system operation of the main thread on the SD card, counted and timed (system
// ticks around the FS service call) into FSO::Files::Fs for the step / heartbeat lines of log.txt (src/app/README.md
// "Calibrated SD cost model"). libctru's sdmc device table lives in writable memory: its entries are wrapped in place.
// Each wrapper forwards whatever the device function takes (signature-agnostic templates), so it builds with any
// newlib devoptab layout.
namespace sdhook {
static devoptab_t g_Orig;
enum Kind { kOpen, kClose, kRead, kWrite, kSeek, kStat, kDirOpen, kDirNext, kOther };
static void Account(int kind, double ms, long long bytes) {
    auto& f = FSO::Files::Fs;
    switch (kind) {
        case kOpen: f.Opens++; f.OpenMs += ms; break;
        case kClose: f.Closes++; f.CloseMs += ms; break;
        case kRead: f.Reads++; f.ReadMs += ms; if (bytes > 0) f.BytesRead += (uint64_t)bytes; break;
        case kWrite: f.Writes++; f.WriteMs += ms; if (bytes > 0) f.BytesWritten += (uint64_t)bytes; break;
        case kSeek: f.Seeks++; f.SeekMs += ms; break;
        case kStat: f.Stats++; f.StatMs += ms; break;
        case kDirOpen: f.DirOpens++; f.DirMs += ms; break;
        case kDirNext: f.DirReads++; f.DirMs += ms; break;
        default: break;
    }
}
static bool g_InHook = false;
template <typename T, T devoptab_t::*Field, int K> struct Hook;
template <typename R, typename... A, R (*devoptab_t::*Field)(A...), int K> struct Hook<R (*)(A...), Field, K> {
    static R Call(A... a) {
        // other threads (the music stream) and the log writes made by the heartbeat itself are not counted
        if (threadGetCurrent() != nullptr || g_InHook) return (g_Orig.*Field)(a...);
        u64 t0 = svcGetSystemTick();
        R r = (g_Orig.*Field)(a...);
        double ms = (double)(svcGetSystemTick() - t0) / (double)(SYSCLOCK_ARM11 / 1000);
        long long bytes = 0;
        if constexpr (std::is_integral<R>::value) bytes = (long long)r;
        Account(K, ms, bytes);
        // the heartbeat only after reads / opens / stats / directory reads: a write or close can be stdout's own flush
        // (printing from inside it would re-enter that stream)
        if (FSO::Files::FsOps::OnOp && (K == kOpen || K == kRead || K == kStat || K == kDirOpen || K == kDirNext)) {
            g_InHook = true; FSO::Files::FsOps::OnOp(); g_InHook = false;
        }
        return r;
    }
};
#define SIMS_HOOK(field, kind) if (dev->field) dev->field = &Hook<decltype(devoptab_t::field), &devoptab_t::field, kind>::Call
static void Install() {
    devoptab_t* dev = const_cast<devoptab_t*>(GetDeviceOpTab("sdmc"));
    if (dev == nullptr) return;
    g_Orig = *dev;
    SIMS_HOOK(open_r, kOpen);
    SIMS_HOOK(close_r, kClose);
    SIMS_HOOK(read_r, kRead);
    SIMS_HOOK(write_r, kWrite);
    SIMS_HOOK(seek_r, kSeek);
    SIMS_HOOK(stat_r, kStat);
    SIMS_HOOK(diropen_r, kDirOpen);
    SIMS_HOOK(dirnext_r, kDirNext);
}
#undef SIMS_HOOK
} // namespace sdhook

static const char* DATA_ROOT = "sdmc:/3ds/sims";
static const char* CACHE_DIR = "sdmc:/3ds/sims/cache";

static double NowMs() {
    return (double)svcGetSystemTick() / (double)(SYSCLOCK_ARM11 / 1000);
}

// Canvas (row-major RGBA, R in the low byte) -> libctru BGR8 framebuffer (rotated 90 degrees: the LCDs are
// portrait panels). Processed in 8-column strips so the source reads stay in cache.
static void BlitCanvas(const Canvas& c, gfxScreen_t screen) {
    u16 fbW = 0, fbH = 0; // fbW = 240 (screen height), fbH = 400/320 (screen width)
    u8* fb = gfxGetFramebuffer(screen, GFX_LEFT, &fbW, &fbH);
    if (fb == nullptr) return;
    const int W = c.Width, H = c.Height;
    if (W > fbH || H > fbW) return;
    const uint32_t* src = c.Px.data();
    for (int x0 = 0; x0 < W; x0 += 8) {
        int x1 = x0 + 8 < W ? x0 + 8 : W;
        for (int y = 0; y < H; y++) {
            const uint32_t* row = src + (size_t)y * W;
            for (int x = x0; x < x1; x++) {
                uint32_t p = row[x];
                u8* d = fb + ((size_t)x * fbW + (size_t)(fbW - 1 - y)) * 3;
                d[0] = (u8)(p >> 16); // B
                d[1] = (u8)(p >> 8);  // G
                d[2] = (u8)p;         // R
            }
        }
    }
}

// Frames that took longer than a refresh (~16.7 ms) do not wait for VBlank again: the swap queued by
// gfxSwapBuffers has long taken effect by the time the next frame is written, and waiting would quantize a
// 35 ms frame to 50 ms (20 fps) instead of ~28 fps.
static double g_lastPresent = 0;
static void PresentFrame(App& app) {
    BlitCanvas(app.Top, GFX_TOP);
    BlitCanvas(app.Bottom, GFX_BOTTOM);
    gfxFlushBuffers();
    gfxSwapBuffers();
    double now = NowMs();
    if (now - g_lastPresent < 15.0) gspWaitForVBlank();
    g_lastPresent = NowMs();
}

// FIX(sims): HOME menu / sleep. Pause the game's own sound mixing while the app is suspended (HOME menu, sleep, power
// menu) and flush the log, so nothing competes with the system applets for CPU/SD while they run.
static aptHookCookie g_aptCookie;
static void OnAptEvent(APT_HookType hook, void*) {
    switch (hook) {
    case APTHOOK_ONSUSPEND: case APTHOOK_ONSLEEP:
        simaudio::GameAudio::Get().SetSuspended(true); fflush(stdout); break;
    case APTHOOK_ONRESTORE: case APTHOOK_ONWAKEUP:
        simaudio::GameAudio::Get().SetSuspended(false); break;
    // FIX(sims) SAVE: closed from the HOME menu / power button. APT hooks run inside aptMainLoop() / aptExit() on the
    // main thread, and every save is a blocking call on that thread (files/SafeFile.h: .new written, flushed, closed,
    // then renamed), so no save is ever half-done here; give back the read handles and push the log out.
    case APTHOOK_ONEXIT:
        FSO::Files::FilePool::CloseAll(); fflush(stdout); break;
    default: break;
    }
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    osSetSpeedupEnable(true); // New 3DS: 804 MHz + L2 cache (no effect on the old 3DS)
    gfxInit(GSP_BGR8_OES, GSP_BGR8_OES, false);
    gfxSet3D(false);
    bool isNew3DS = false;
    APT_CheckNew3DS(&isNew3DS);
    aptHook(&g_aptCookie, OnAptEvent, nullptr);
    // New 3DS ZL/ZR/C-stick: libctru's hidInit starts ir:rst itself and merges ZL/ZR into hidKeysHeld()

    sdhook::Install(); // TS1(sims) hardware pass: SD operations counted + timed for log.txt
    mkdir(DATA_ROOT, 0777);
    mkdir(CACHE_DIR, 0777);
    // optional log: create an empty file "debug" next to the data to get stdout in log.txt
    bool logSteps = false;
    if (FILE* dbg = fopen("sdmc:/3ds/sims/debug", "r")) {
        fclose(dbg);
        freopen("sdmc:/3ds/sims/log.txt", "w", stdout);
        logSteps = true;
        printf("sims: %s 3DS, application memory %.1f MB (not mapped %.1f MB), app heap %.1f MB, linear heap %.1f MB (free %.1f MB)\n",
               isNew3DS ? "New" : "Old", osGetMemRegionSize(MEMREGION_APPLICATION) / 1048576.0,
               osGetMemRegionFree(MEMREGION_APPLICATION) / 1048576.0, __ctru_heap_size / 1048576.0,
               __ctru_linear_heap_size / 1048576.0, linearSpaceFree() / 1048576.0);
        fflush(stdout);
    }

    // sound: SoundData/, Music/ and the pack folders next to GameData (src/audio/README.md); silent without them
    {
        simaudio::GameAudio::Config acfg;
        acfg.DataRoot = DATA_ROOT;
        acfg.OutputRate = 32728;
        if (simaudio::GameAudio::Get().Init(acfg))
            simaudio::GameAudio::Get().SetBackend(std::unique_ptr<simaudio::AudioBackend>(new simaudio::NdspBackend()));
    }

    App* app = new App();
    App::Config cfg;
    cfg.DataRoot = DATA_ROOT;
    cfg.CacheDir = CACHE_DIR;
    cfg.ScrollBuffer = 64;      // 3 static layers x 464x304 x 8 bytes = 3.4 MB
    cfg.VMBudgetMs = 14.0;      // at most ~14 ms of SimAntics per frame; ultra speed degrades gracefully
    cfg.ThumbInterval = 2;      // touch-screen copy of the lot at half rate (saves ~1.5 M instructions/frame)
    cfg.NowMs = NowMs;
    // TS1(sims): one timestamped line per loading step in log.txt with the memory left (App::InstallLoadHooks)
    cfg.LogSteps = logSteps;
    // FIX(sims) hardware pass: "app free" was osGetMemRegionFree(APPLICATION), which never changes after start-up
    // (__system_allocateHeaps commits the whole app heap before main: 46.3 MB on every line of the first log was the
    // part of the 178 MB that is in neither heap). Now: what malloc can still hand out = the heap's size minus what
    // the arena took from it, plus the free blocks inside the arena.
    cfg.MemInfo = []() {
        struct mallinfo mi = mallinfo();
        double heapFree = ((double)__ctru_heap_size - (double)mi.arena + (double)mi.fordblks) / 1048576.0;
        char buf[192];
        snprintf(buf, sizeof buf, "heap used %.1f MB (arena %.1f MB of %.1f), heap free %.1f MB, linear free %.1f MB",
                 mi.uordblks / 1048576.0, mi.arena / 1048576.0, __ctru_heap_size / 1048576.0, heapFree, linearSpaceFree() / 1048576.0);
        return std::string(buf);
    };
    app->Present = [app]() { PresentFrame(*app); };
    try {
        app->Init(cfg);
    } catch (std::bad_alloc&) {
        printf("init: out of memory\n");
        fflush(stdout);
    } catch (std::exception& e) {
        printf("init: %s\n", e.what());
        fflush(stdout);
    }

    AppInput in;
    double last = NowMs();
    bool wasTouching = false;
    while (aptMainLoop() && !app->QuitRequested) {
        hidScanInput(); // also scans ir:rst on the New 3DS
        u32 down = hidKeysDown();
        u32 held = hidKeysHeld();
        auto map = [](u32 k) {
            uint32_t r = 0;
            if (k & KEY_A) r |= BTN_A;
            if (k & KEY_B) r |= BTN_B;
            if (k & KEY_X) r |= BTN_X;
            if (k & KEY_Y) r |= BTN_Y;
            if (k & KEY_L) r |= BTN_L;
            if (k & KEY_R) r |= BTN_R;
            if (k & KEY_ZL) r |= BTN_ZL;
            if (k & KEY_ZR) r |= BTN_ZR;
            if (k & KEY_START) r |= BTN_START;
            if (k & KEY_SELECT) r |= BTN_SELECT;
            if (k & KEY_DUP) r |= BTN_UP;      // d-pad only (KEY_UP also includes the circle pad)
            if (k & KEY_DDOWN) r |= BTN_DOWN;
            if (k & KEY_DLEFT) r |= BTN_LEFT;
            if (k & KEY_DRIGHT) r |= BTN_RIGHT;
            return r;
        };
        in.Down = map(down);
        in.Held = map(held);
        circlePosition cp;
        hidCircleRead(&cp);
        in.CircleX = cp.dx / 156.0f;
        in.CircleY = cp.dy / 156.0f;
        in.CStickX = in.CStickY = 0;
        if (isNew3DS) {
            circlePosition cs;
            hidCstickRead(&cs);
            in.CStickX = cs.dx / 156.0f;
            in.CStickY = cs.dy / 156.0f;
        }
        bool touching = (held & KEY_TOUCH) != 0;
        if (touching) {
            touchPosition tp;
            hidTouchRead(&tp);
            in.TouchX = tp.px;
            in.TouchY = tp.py;
        }
        in.Touching = touching;
        in.TouchDown = touching && !wasTouching;
        in.TouchUp = !touching && wasTouching; // TouchX/Y keep the last position
        wasTouching = touching;

        double now = NowMs();
        double dt = now - last;
        last = now;
        if (dt > 250) dt = 250; // after a suspend (HOME menu / sleep)
        try {
            app->Frame(in, dt);
        } catch (std::bad_alloc&) {
            printf("frame: out of memory | %s\n", cfg.MemInfo().c_str());
            fflush(stdout);
        } catch (std::exception& e) {
            printf("frame: %s\n", e.what());
            fflush(stdout);
        }
        PresentFrame(*app);
    }
    delete app;
    simaudio::GameAudio::Get().Shutdown();
    gfxExit();
    return 0;
}
