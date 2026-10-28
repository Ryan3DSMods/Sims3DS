/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
#include "autonomy.h"
#include "vm.h"
#include <algorithm>

namespace st {

const char* motiveName(int m) {
    static const char* N[16]={"HappyLife","HappyWeek","HappyDay","Mood","-","Energy","Comfort","Hunger",
                              "Hygiene","Bladder","-","SleepState","-","Room","Social","Fun"};
    return (m>=0&&m<16)? N[m] : "?";
}

Sim::Sim() { for (int i=0;i<16;i++) motives[i]=0; }

// Rough per-hour decay in the game's -100..100 scale (needs fall over time).
void Sim::decay(int hours) {
    static const int rate[16]={0,0,0,0,0, -4 /*Energy*/, -3 /*Comfort*/, -8 /*Hunger*/, -3 /*Hygiene*/,
                               -6 /*Bladder*/, 0,0,0, 0 /*Room*/, -2 /*Social*/, -5 /*Fun*/};
    for (int h=0;h<hours;h++) for (int i=0;i<16;i++) motives[i] = std::max(-100, motives[i]+rate[i]);
}

// Port of the game's interpolation: when the motive is low the interaction is
// worth (min+max); when it's high only (min). Negative needs clamp to 0 (max urgency).
int scoreInteraction(const Sim& s, const LotInteraction& it, int* bestMotive) {
    int base = 0; int bm = -1;
    for (size_t i=0;i<it.ads.size() && i<16;i++) {
        const MotiveAd& a = it.ads[i];
        if (a.min==0 && a.max==0) continue;
        int my = std::max(0, s.motives[i]);
        int interp = (my*a.min + (100-my)*(a.min+a.max)) / 100;
        if (interp > base) { base = interp; bm=(int)i; }
    }
    if (bestMotive) *bestMotive = bm;
    return base;
}

int Sim::nextRandom(int n) {
    rng = rng * 1103515245u + 12345u;             // LCG, deterministic per seed
    return (int)((rng >> 16) % (uint32_t)n);
}

int findBestAction(Sim& s, const std::vector<LotInteraction>& lot, int* outScore) {
    int best=-1, bestScore=0;
    for (size_t i=0;i<lot.size();i++) {
        int sc = scoreInteraction(s, lot[i]);
        if (sc != 0) sc += s.nextRandom(31) - 15;   // the game's jitter
        if (sc > (int)lot[i].threshold && sc > bestScore) { bestScore=sc; best=(int)i; }
    }
    if (outScore) *outScore = bestScore;
    return best;
}

void performInteraction(Sim& s, const LotInteraction& it) {
    for (size_t i=0;i<it.ads.size() && i<16;i++) {
        const MotiveAd& a = it.ads[i];
        if (a.min==0 && a.max==0) continue;
        int gain = (a.max!=0)? a.max : a.min;
        s.motives[i] = std::max(-100, std::min(100, s.motives[i] + gain));
    }
}

void collectInteractions(IffFile& obj, const std::string& objName, std::vector<LotInteraction>& out, const Sim* sim) {
    StrSet names; bool haveNames=false;
    for (auto* c : obj.byType("TTAs")) { if (names.parse(*c)) { haveNames=true; break; } }
    for (auto* c : obj.byType("TTAB")) {
        Ttab tt; if (!tt.parse(*c)) continue;
        for (auto& in : tt.interactions) {
            bool anyAd=false; for (auto& a : in.motives) if (a.min||a.max) anyAd=true;
            if (!anyAd) continue;               // only autonomy-advertised interactions
            // honour the guard tree on a fresh object (e.g. "Turn Off" needs the TV on)
            if (in.testBHAV) {
                const Chunk* bc = obj.get("BHAV", in.testBHAV);
                if (bc) { Bhav b; if (b.parse(*bc)) { VM vm; vm.obj=&obj; if (sim) for(int i=0;i<16;i++) vm.simMotives[i]=sim->motives[i]; if (vm.run(b)!=VM::R_TRUE) continue; } }
            }
            LotInteraction li;
            li.object = objName;
            li.name = (haveNames && in.ttaIndex < names.strings.size()) ? names.strings[in.ttaIndex] : "(?)";
            li.actionBHAV = in.actionBHAV; li.threshold = in.autonomyThreshold; li.ads = in.motives;
            // de-dup identical (object,name) pairs from multiple TTABs
            bool dup=false; for (auto& e : out) if (e.object==li.object && e.name==li.name) { dup=true; break; }
            if (!dup) out.push_back(std::move(li));
        }
        break; // first TTAB is the main menu
    }
}

} // namespace st
