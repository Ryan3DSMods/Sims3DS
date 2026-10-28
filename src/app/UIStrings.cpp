/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
#include "app/UIStrings.h"
#include "app/Font.h"
#include "files/formats/iff/IffFile.h"
#include "files/formats/iff/chunks/STR.h"
#include <cstdio>
#include <algorithm>

namespace simapp {

UIStrings::~UIStrings() {}

void UIStrings::Load(const std::string& gameDataDir) {
    static const int tables[] = { 128, 133, 138, 145, 148, 152, 153, 160, 166 };
    try {
        FSO::Files::Formats::IFF::IffFile iff(gameDataDir + "/UIText.iff");
        for (int t : tables) {
            auto str = iff.Get<FSO::Files::Formats::IFF::Chunks::STR>((uint16_t)t);
            if (str == nullptr) continue;
            int n = std::min<int>((int)str->Length(), 200);
            for (int i = 0; i < n; i++) Strings[t * 10000 + i] = str->GetString(i);
        }
    } catch (std::exception& e) { printf("[app] UIText.iff not loaded: %s\n", e.what()); }
}

std::string UIStrings::Get(int table, int index, const char* fallback) const {
    auto it = Strings.find(table * 10000 + index);
    if (it == Strings.end()) return fallback;
    std::string s = it->second;
    if (s.empty()) return fallback;
    // strip hotkey hints: "Pause - P", "Zoom In  +", "Live Mode - F1"
    size_t k = s.rfind(" - ");
    if (k != std::string::npos && s.size() - k <= 5) s.resize(k);
    while (!s.empty() && (s.back() == '+' || s.back() == '-' || s.back() == ' ')) s.pop_back();
    size_t pct = s.find(": %d");
    if (pct != std::string::npos) s.resize(pct);
    return s.empty() ? std::string(fallback) : s;
}

std::string UIStrings::Motive(int i) const {
    // Help (STR#166) titles; Room from CatalogRatings (STR#160 "Room: %d")
    switch (i) {
        case 0: return Get(166, 38, "Hunger");
        case 1: return Get(166, 16, "Comfort");
        case 2: return Get(166, 40, "Hygiene");
        case 3: return Get(166, 4, "Bladder");
        case 4: return Get(166, 22, "Energy");
        case 5: return Get(166, 30, "Fun");
        case 6: return Get(166, 72, "Social");
        default: return Get(160, 6, "Room");
    }
}

std::string UIStrings::Speed(int i) const {
    switch (i) {
        case 0: return Get(138, 15, "Pause");
        case 1: return Get(138, 16, "Normal Speed");
        case 2: return Get(138, 17, "High Speed");
        default: return Get(138, 18, "Ultra Speed");
    }
}

std::string UIStrings::Walls(int mode) const {
    switch (mode) {
        case 0: return Get(138, 8, "Walls Cutaway");
        case 1: return Get(138, 9, "Walls Down");
        default: return Get(138, 7, "Walls Up");
    }
}

}
