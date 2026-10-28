/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims — 3DS audio backend (libctru NDSP).
#include "platform/3ds/AudioNdsp.h"
#include <3ds.h>
#include <cstdio>
#include <cstring>

namespace simaudio {

volatile float NdspBackend::MixMs = 0;
volatile int NdspBackend::Underruns = 0;

namespace {
LightLock g_Lock;
LightEvent g_Wake;
Thread g_Thread = nullptr;
volatile bool g_Run = false;
bool g_Ndsp = false;
ndspWaveBuf g_Wave[NdspBackend::kBuffers];
int16_t* g_Pcm = nullptr;
AudioMixer* g_Mixer = nullptr;

void LockFn(void*) { LightLock_Lock(&g_Lock); }
void UnlockFn(void*) { LightLock_Unlock(&g_Lock); }

void DspCallback(void*) { LightEvent_Signal(&g_Wake); }

void Fill(ndspWaveBuf& wb) {
    u64 t0 = svcGetSystemTick();
    g_Mixer->Mix(wb.data_pcm16, NdspBackend::kFrames);
    NdspBackend::MixMs = (float)((double)(svcGetSystemTick() - t0) / (SYSCLOCK_ARM11 / 1000.0));
    DSP_FlushDataCache(wb.data_pcm16, NdspBackend::kFrames * 2 * sizeof(int16_t));
    wb.nsamples = NdspBackend::kFrames;
    ndspChnWaveBufAdd(0, &wb);
}

void AudioThread(void*) {
    while (g_Run) {
        int queued = 0;
        for (auto& wb : g_Wave) {
            if (wb.status == NDSP_WBUF_DONE || wb.status == NDSP_WBUF_FREE) Fill(wb);
            else queued++;
        }
        if (queued == 0) NdspBackend::Underruns = NdspBackend::Underruns + 1; // every buffer had drained
        LightEvent_Wait(&g_Wake);
    }
}
} // namespace

bool NdspBackend::Start(AudioMixer& mixer) {
    if (g_Ndsp) return true;
    if (R_FAILED(ndspInit())) {
        std::printf("audio: ndspInit failed (missing sdmc:/3ds/dspfirm.cdc?) - no sound\n");
        return false;
    }
    g_Ndsp = true;
    g_Mixer = &mixer;
    LightLock_Init(&g_Lock);
    LightEvent_Init(&g_Wake, RESET_ONESHOT);
    mixer.SetLock(LockFn, UnlockFn, nullptr);

    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspChnReset(0);
    ndspChnSetInterp(0, NDSP_INTERP_POLYPHASE);
    ndspChnSetRate(0, (float)mixer.OutputRate());
    _Rate = mixer.OutputRate(); // ADDED(sims)
    ndspChnSetFormat(0, NDSP_FORMAT_STEREO_PCM16);
    float mix[12] = { 1.0f, 1.0f };
    ndspChnSetMix(0, mix);

    g_Pcm = (int16_t*)linearAlloc(kBuffers * kFrames * 2 * sizeof(int16_t));
    if (g_Pcm == nullptr) { Stop(); return false; }
    std::memset(g_Wave, 0, sizeof g_Wave);
    for (int i = 0; i < kBuffers; i++) {
        g_Wave[i].data_vaddr = g_Pcm + (size_t)i * kFrames * 2;
        g_Wave[i].status = NDSP_WBUF_FREE;
    }
    ndspSetCallback(DspCallback, nullptr);

    // the mixer/MP3 thread runs above the game thread's priority; on the New 3DS it gets core 2 to itself
    s32 prio = 0x30;
    svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
    g_Run = true;
    bool isNew = false;
    APT_CheckNew3DS(&isNew);
    if (isNew) g_Thread = threadCreate(AudioThread, nullptr, 32 * 1024, prio - 1, 2, false);
    if (g_Thread == nullptr) {
        APT_SetAppCpuTimeLimit(30);
        g_Thread = threadCreate(AudioThread, nullptr, 32 * 1024, prio - 1, 1, false);
    }
    if (g_Thread == nullptr) g_Thread = threadCreate(AudioThread, nullptr, 32 * 1024, prio - 1, -2, false);
    if (g_Thread == nullptr) { Stop(); return false; }
    return true;
}

void NdspBackend::Stop() {
    if (g_Thread != nullptr) {
        g_Run = false;
        LightEvent_Signal(&g_Wake);
        threadJoin(g_Thread, U64_MAX);
        threadFree(g_Thread);
        g_Thread = nullptr;
    }
    if (g_Ndsp) {
        ndspSetCallback(nullptr, nullptr);
        ndspChnReset(0);
        ndspExit();
        g_Ndsp = false;
    }
    if (g_Mixer != nullptr) g_Mixer->SetLock(nullptr, nullptr, nullptr);
    g_Mixer = nullptr;
    if (g_Pcm != nullptr) { linearFree(g_Pcm); g_Pcm = nullptr; }
}

void NdspBackend::SetSuspended(bool suspended) {
    if (g_Ndsp) ndspChnSetPaused(0, suspended);
}

}
