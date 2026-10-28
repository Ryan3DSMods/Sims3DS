/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// simsound — a lot played with sound, headless: loads a house (LotSession, the same as the 3DS app), lets the
// family live (their own autonomy) and records what the console would play to a WAV file, logging every sound
// event with the game time. Optional scripted interactions: "Object:Interaction@seconds" (selected Sim).
//
//   simsound <dataRoot> <cacheDir> <house> <out.wav> [seconds=120] [speed=1] ["Stereo:Turn On@5" ...]
#include "app/LotSession.h"
#include "audio/AudioBackend.h"
#include "audio/GameAudio.h"
#include "content/ContentManager.h"
#include "hit/HITVM.h"
#include "simantics/VM.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/entities/VMEntity.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

using namespace simapp;
using namespace simaudio;

static double RealMs() {
    using namespace std::chrono;
    return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
}

struct Action { std::string Obj, Interaction; double At; bool Done = false; };

int main(int argc, char** argv) {
    if (argc < 5) { std::printf("usage: simsound <dataRoot> <cacheDir> <house> <out.wav> [seconds] [speed] [Obj:Interaction@sec ...]\n"); return 2; }
    setvbuf(stdout, nullptr, _IONBF, 0);
    std::string root = argv[1], cache = argv[2];
    int house = std::atoi(argv[3]);
    double seconds = argc > 5 ? std::atof(argv[5]) : 120;
    int speed = argc > 6 ? std::atoi(argv[6]) : 1;
    std::vector<Action> actions;
    for (int i = 7; i < argc; i++) {
        std::string a = argv[i];
        auto c = a.find(':'), at = a.find('@');
        if (c == std::string::npos || at == std::string::npos) continue;
        actions.push_back({ a.substr(0, c), a.substr(c + 1, at - c - 1), std::atof(a.substr(at + 1).c_str()) });
    }

    FSO::Content::Content::Init(root, cache);
    GameAudio::Config acfg;
    acfg.DataRoot = root;
    if (!GameAudio::Get().Init(acfg)) return 1;
    GameAudio::Get().SetBackend(std::unique_ptr<AudioBackend>(new WavCaptureBackend(argv[4])));
    double simT = 0;
    std::map<std::string, int> counts;
    GameAudio::Get().Log = [&](const std::string& e) {
        std::printf("[snd %7.2fs] %s\n", simT, e.c_str());
        auto p = e.find(':');
        if (e.find(": note") == std::string::npos && e.find(": music") == std::string::npos && e.find(": tv") == std::string::npos)
            counts[e.substr(0, p)]++;
    };

    LotSession s;
    std::string err;
    if (!s.Load(house, 400, 240, 64, err)) { std::printf("load failed: %s\n", err.c_str()); return 1; }
    std::printf("[lot] house %d, %zu Sims\n", house, s.Family.size());
    if (std::getenv("SIMSOUND_LIST")) for (auto e : s.VM()->Entities) std::printf("[obj] %s\n", e->ToString().c_str());
    GameAudio::Get().SetMusicMode(MusicMode::Live);

    const double frame = 1000.0 / 30.0;
    static const int UPF[4] = { 0, 2, 6, 20 }; // VM::Update calls per 30 Hz frame (TS1 speeds)
    double t0 = RealMs(), vmMs = 0, audioMs = 0;
    for (; simT < seconds; simT += frame / 1000.0) {
        for (auto& a : actions) {
            if (a.Done || simT < a.At) continue;
            a.Done = true;
            auto sim = s.SelectedSim();
            if (sim == nullptr) { std::printf("[act] no Sim selected\n"); continue; }
            for (auto e : s.VM()->Entities) {
                if (e->ToString().find(a.Obj) == std::string::npos) continue;
                auto menu = e->GetPieMenu(s.VM(), sim, false);
                for (auto& m : menu) {
                    if (m.Name.find(a.Interaction) == std::string::npos) continue;
                    PieChoice c; c.Name = m.Name; c.ID = m.ID; c.Param0 = m.Param0;
                    bool ok = s.PushInteraction(e, false, c);
                    std::printf("[act %7.2fs] %s '%s' on %s\n", simT, ok ? "queued" : "FAILED", m.Name.c_str(), e->ToString().c_str());
                    goto next;
                }
            }
            std::printf("[act %7.2fs] no '%s' on '%s'\n", simT, a.Interaction.c_str(), a.Obj.c_str());
        next:;
        }
        if (std::getenv("SIMSOUND_POS") && ((int)(simT * 30) % 150) == 0) {
            for (auto e : s.VM()->Entities)
                for (auto& t : e->SoundThreads)
                    if (t.Sound) std::printf("[pos %7.2fs] %-28s %-24s tile (%d,%d,L%d) vol %.2f pan %+.2f%s\n", simT, t.Name.c_str(), e->ToString().c_str(),
                        e->Position().TileX(), e->Position().TileY(), e->Position().Level, t.Sound->Volume, t.Sound->Pan, t.Pan ? "" : " (no pan)");
        }
        double a = RealMs();
        s.Update(UPF[speed]);
        double b = RealMs();
        GameAudio::Get().Update(frame);
        GameAudio::Get().Backend()->Pump(frame);
        audioMs += RealMs() - b;
        vmMs += b - a;
    }
    auto cap = static_cast<WavCaptureBackend*>(GameAudio::Get().Backend());
    std::printf("[done] %.0f s of game at speed %d: %.1f s audio captured, peak %d; wall %.1f s (vm %.1f s, audio %.2f s)\n",
                seconds, speed, cap->SecondsWritten(), cap->Peak(), (RealMs() - t0) / 1000, vmMs / 1000, audioMs / 1000);
    std::printf("[done] events: %d played, %d notes, %d object resets\n", FSO::HIT::HITVM::Get()->EventsPlayed, FSO::HIT::HITVM::Get()->NotesPlayed, s.Resets);
    for (auto& c : counts) std::printf("  %4d x %s\n", c.second, c.first.c_str());
    GameAudio::Get().Shutdown();          // finalises the WAV
    std::fflush(stdout);
    std::_Exit(0);                        // skip static teardown (content manager / lot order is not ours)
}
