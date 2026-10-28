/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// simaudio — host tool for the sims sound engine (src/hit + src/audio): decodes the game's samples and music,
// plays HIT events through the real HIT VM and mixer, and writes what would be heard to WAV files.
//
//   simaudio <dataRoot> info                               banks / events / samples found
//   simaudio <dataRoot> decode <sample.xa|.wav> <out.wav>  one archive sample, decoded at its own rate
//   simaudio <dataRoot> event <name> <out.wav> [sec] [gender]   a sound event (gender 0 male, 1 female, 2 child)
//   simaudio <dataRoot> music <mode|event> <out.wav> [sec]      mode music (3 = neighbourhood, 1 buy, 2 build, 5 load)
//   simaudio <dataRoot> station <eventName> <out.wav> [sec]     a radio/TV event (stereo_rock, tv_action_mod ...)
//   simaudio <dataRoot> playfile <file.mp3|.wav> <out.wav> [sec]  one file through the mixer (resampler check)
//   simaudio <dataRoot> sweep [max]                        plays every event headless: coverage, failures, CPU
//
// dataRoot is the install tree (SoundData/, Music/, ExpansionPack*/, ExpansionShared/, Downloads/).
#include "audio/AudioBackend.h"
#include "audio/GameAudio.h"
#include "audio/Mp3Stream.h"
#include "audio/WavFile.h"
#include "content/Audio.h"
#include "files/HIT/HITText.h"
#include "hit/HITThread.h"
#include "hit/HITVM.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

using namespace simaudio;
using FSO::HIT::HITVM;

static double NowMs() {
    using namespace std::chrono;
    return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
}

static void Run(double seconds) {
    // 30 fps frames like the 3DS app: Update ticks the HIT VM at 60 Hz, Pump mixes the frame's audio
    double frame = 1000.0 / 30.0;
    for (double t = 0; t < seconds * 1000.0; t += frame) {
        GameAudio::Get().Update(frame);
        GameAudio::Get().Backend()->Pump(frame);
    }
}

int main(int argc, char** argv) {
    if (argc < 3) { std::fprintf(stderr, "usage: see the header of src/tools/simaudio.cpp\n"); return 2; }
    std::string root = argv[1], cmd = argv[2];
    auto& ga = GameAudio::Get();
    GameAudio::Config cfg;
    cfg.DataRoot = root;
    cfg.MusicVolume = 1.0f;
    double t0 = NowMs();
    if (!ga.Init(cfg)) return 1;
    double initMs = NowMs() - t0;
    auto content = FSO::Content::Audio::Get();
    bool quiet = std::getenv("SIMAUDIO_QUIET") != nullptr;
    ga.Log = [quiet](const std::string& s) { if (!quiet) std::printf("  [hit] %s\n", s.c_str()); };

    if (cmd == "info") {
        std::printf("init %.1f ms\n", initMs);
        size_t ev = 0, tr = 0, pa = 0, hl = 0;
        for (auto& g : content->Groups) {
            std::printf("  bank %-45s prio %d  events %5zu tracks %5zu patches %5zu hitlists %5zu code %zu B\n", g->Name.c_str(),
                g->LoadPriority, g->hot->Events.size(), g->hot->Tracks.size(), g->hot->Patches.size(), g->hot->Hitlists.size(),
                g->hit ? g->hit->Data.size() : (size_t)0);
            ev += g->hot->Events.size(); tr += g->hot->Tracks.size(); pa += g->hot->Patches.size(); hl += g->hot->Hitlists.size();
        }
        std::printf("total: %zu events, %zu tracks, %zu patches, %zu hitlists, %zu samples indexed\n", ev, tr, pa, hl, content->IndexedSamples());
        for (auto& s : content->StationPaths()) {
            size_t n = s.second.TV ? (content->GetTVStation(s.second.Path) ? content->GetTVStation(s.second.Path)->Episodes.size() : 0)
                                   : content->ListMusic(s.second.Path).size();
            std::printf("  station %6u %-14s %-28s %zu %s\n", s.first, s.second.Label.c_str(), s.second.Path.c_str(), n, s.second.TV ? "shows" : "songs");
        }
        for (auto& m : content->MusicModes())
            std::printf("  music mode %2d %-28s %zu files\n", m.first, m.second.c_str(), m.second.empty() ? (size_t)0 : content->ListMusic(m.second).size());
        return 0;
    }
    if (cmd == "decode" && argc >= 5) {
        SoundBufferPtr buf;
        std::string name = argv[3];
        if (FSO::Files::HIT::Text::ToLower(name).find(".mp3") != std::string::npos) buf = Mp3Stream::DecodeAll(name);
        else buf = content->GetSampleByName(name);
        if (!buf) { std::fprintf(stderr, "not found: %s\n", name.c_str()); return 1; }
        WriteWavFile(argv[4], *buf);
        std::printf("%s: %d Hz, %d ch, %.3f s\n", name.c_str(), buf->Rate, buf->Channels, buf->Seconds());
        return 0;
    }
    if ((cmd == "event" || cmd == "station" || cmd == "music") && argc >= 5) {
        double sec = argc >= 6 ? std::atof(argv[5]) : 6.0;
        ga.SetBackend(std::unique_ptr<AudioBackend>(new WavCaptureBackend(argv[4])));
        std::string what = argv[3];
        if (cmd == "music") {
            if (std::isdigit((unsigned char)what[0])) ga.SetMusicMode((MusicMode)std::atoi(what.c_str()));
            else ga.SetMusicEvent(what);
        } else {
            auto snd = HITVM::Get()->PlaySoundEvent(what);
            if (!snd) { std::fprintf(stderr, "event did not start: %s\n", what.c_str()); }
            auto th = std::dynamic_pointer_cast<FSO::HIT::HITThread>(snd);
            if (th && !th->ObjectVar.empty()) {
                int gender = argc >= 7 ? std::atoi(argv[6]) : 0;
                th->ObjectVar[0] = gender;  // what VMAvatar::SubmitHITVars gives the HIT code
                th->WriteVar(1, 0x1000 + 7); // arg1 = the speaking instance
            }
        }
        double m0 = NowMs();
        Run(sec);
        double mixMs = NowMs() - m0;
        auto cap = static_cast<WavCaptureBackend*>(ga.Backend());
        std::printf("wrote %s: %.2f s, peak %d, %.1f ms wall (%.1fx real time), notes %d, voices now %d\n", argv[4], cap->SecondsWritten(),
                    cap->Peak(), mixMs, sec * 1000.0 / (mixMs > 0 ? mixMs : 1), HITVM::Get()->NotesPlayed, ga.ActiveVoices());
        ga.Shutdown();
        return 0;
    }
    if (cmd == "playfile" && argc >= 5) {
        // a music file straight through the mixer (resampler check)
        double sec = argc >= 6 ? std::atof(argv[5]) : 10.0;
        ga.SetBackend(std::unique_ptr<AudioBackend>(new WavCaptureBackend(argv[4])));
        std::unique_ptr<AudioStream> st;
        std::string f = argv[3];
        if (f.size() > 4 && FSO::Files::HIT::Text::ToLower(f.substr(f.size() - 4)) == ".wav") st = WavStream::Open(f);
        else st = Mp3Stream::Open(f);
        if (!st) return 1;
        ga.Mixer()->PlayStream(std::move(st), 1.0f, 0.0f, false, AudioMixer::GroupSFX);
        Run(sec);
        ga.Shutdown();
        return 0;
    }
    if (cmd == "sweep") {
        int max = argc >= 4 ? std::atoi(argv[3]) : 100000;
        ga.Log = nullptr;
        ga.SetBackend(std::unique_ptr<AudioBackend>(new WavCaptureBackend("/dev/null")));
        std::map<std::string, int> kinds;
        std::vector<std::string> names;
        for (auto& g : content->Groups) for (auto& e : g->hot->Events) names.push_back(e.first);
        int started = 0, withNotes = 0, silent = 0, n = 0, nostart = 0;
        std::vector<std::string> silentList;
        double m0 = NowMs();
        for (auto& name : names) {
            if (n++ >= max) break;
            auto reg = HITVM::Get()->GetEvent(name);
            if (!reg || reg->EventType != FSO::Files::HIT::HITEvents::kSoundobPlay) { kinds["other"]++; continue; }
            int before = HITVM::Get()->NotesPlayed;
            auto snd = HITVM::Get()->PlaySoundEvent(name);
            if (!snd) { nostart++; continue; }
            started++;
            Run(0.25);
            if (HITVM::Get()->NotesPlayed > before) withNotes++;
            else { silent++; if (silentList.size() < 25) silentList.push_back(name); }
            HITVM::Get()->KillAll(true);
            Run(0.05);
        }
        double ms = NowMs() - m0;
        std::printf("sweep: %d events tried, %d started, %d played notes, %d silent, %d did not start, %d non-play events\n",
                    n, started, withNotes, silent, nostart, kinds["other"]);
        for (auto& s : silentList) std::printf("  silent: %s\n", s.c_str());
        std::printf("sample cache %zu KB, %d misses; %.0f ms wall for %.0f s of audio\n", content->CacheBytes() / 1024, content->Misses, ms, started * 0.3);
        ga.Shutdown();
        return 0;
    }
    std::fprintf(stderr, "unknown command\n");
    return 2;
}
