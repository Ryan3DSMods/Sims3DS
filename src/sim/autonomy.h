/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Needs-driven autonomy: a Sim with motives, and the "find best action" scoring
// from the game (VMFindBestAction): each interaction advertises a per-motive
// effect range; score = max over motives of a linear interp against the Sim's
// current need, attenuated and thresholded. Low need -> high score.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "../formats/iff.h"

namespace st {

enum Motive : int { Energy=5, Comfort=6, Hunger=7, Hygiene=8, Bladder=9, Room=13, Social=14, Fun=15 };
const char* motiveName(int m);

struct Sim {
    std::string name = "Sim";
    int motives[16];           // -100..100, 0 neutral (same scale as the game)
    uint32_t rng = 12345;      // seeded so a run is reproducible
    Sim();
    void decay(int hours);     // per-hour needs decay
    int nextRandom(int n);     // 0..n-1 (the game adds ±15 jitter to scores)
};

struct LotInteraction {        // one interaction available on the lot
    std::string object;        // object name
    std::string name;          // pie-menu name
    uint16_t actionBHAV = 0;
    uint32_t threshold = 0;
    std::vector<MotiveAd> ads; // indexed by motive id
};

// Game's scoring for one interaction against a Sim (no distance attenuation
// here — single-room lot; no random jitter so results are reproducible).
int scoreInteraction(const Sim& s, const LotInteraction& it, int* bestMotive = nullptr);

// Pick the best interaction above its autonomy threshold (with the game's
// ±15 jitter); -1 if none.
int findBestAction(Sim& s, const std::vector<LotInteraction>& lot, int* outScore = nullptr);

// Apply the advertised effect (uses the max of each advertised range).
void performInteraction(Sim& s, const LotInteraction& it);

// Collect advertised interactions from an object IFF (by its TTAB/TTAs).
// `sim` (optional) supplies motives/person data for guards that read the caller's state.
void collectInteractions(IffFile& obj, const std::string& objName, std::vector<LotInteraction>& out, const Sim* sim = nullptr);

} // namespace st
