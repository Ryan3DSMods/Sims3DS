/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims app — see GameText.h
#include "app/ui/GameText.h"
#include "app/Font.h"
#include "files/formats/iff/IffFile.h"
#include "files/formats/iff/chunks/STR.h"
#include <algorithm>
#include <cstdio>

namespace simapp {

GameText& GameText::Get() {
    static GameText instance;
    return instance;
}

static void LoadTables(const std::string& path, const int* tables, int n, std::map<int, std::string>& out) {
    try {
        FSO::Files::Formats::IFF::IffFile iff(path);
        for (int k = 0; k < n; k++) {
            auto str = iff.Get<FSO::Files::Formats::IFF::Chunks::STR>((uint16_t)tables[k]);
            if (str == nullptr) continue;
            int cnt = std::min<int>((int)str->Length(), 200);
            for (int i = 0; i < cnt; i++) out[tables[k] * 10000 + i] = ToLatin1(str->GetString(i));
        }
    } catch (std::exception& e) { printf("[app] %s not loaded: %s\n", path.c_str(), e.what()); }
}

void GameText::Load(const std::string& gameDataDir) {
    if (!_UI.empty()) return;
    static const int ui[] = { 133, 137, 138, 139, 140, 145, 148, 150, 152, 160, 200, 201, 202, 203, 204, 205, 206, 207, 210 };
    static const int live[] = { 130, 131, 132, 133, 134, 136, 137 };
    LoadTables(gameDataDir + "/UIText.iff", ui, (int)(sizeof ui / sizeof ui[0]), _UI);
    LoadTables(gameDataDir + "/Live.iff", live, (int)(sizeof live / sizeof live[0]), _Live);
}

static std::string Strip(std::string s) {
    size_t k = s.rfind(" - ");
    if (k != std::string::npos && s.size() - k <= 5) s.resize(k);
    while (!s.empty() && (s.back() == '+' || s.back() == ' ')) s.pop_back();
    return s;
}

std::string GameText::UI(int table, int index, const char* fallback, bool strip) const {
    auto it = _UI.find(table * 10000 + index);
    if (it == _UI.end() || it->second.empty()) return fallback;
    return strip ? Strip(it->second) : it->second;
}

std::string GameText::Live(int table, int index, const char* fallback) const {
    auto it = _Live.find(table * 10000 + index);
    if (it == _Live.end() || it->second.empty() || it->second == "[F]") return fallback;
    return it->second;
}

std::string GameText::PlacementError(int e) const {
    // FreeSims VMPlacementError (TSO cst 137 order) -> TS1 UIText.iff STR# 137 index
    int idx = -1;
    if (e >= 0 && e <= 28) idx = e;
    else switch (e) {
        case 29: idx = 46; break; // MustPlaceOnWater
        case 30: idx = 29; break; // MustPlaceOnPool
        case 31: idx = 30; break; // MustBeOnFirstLevel
        case 32: idx = 31; break; // Floor2NeedsSupport
        case 33: idx = 32; break; // MustRemoveObjectsOnWall
        case 34: idx = 33; break; // MustBeAgainst2ndFloorWall
        case 35: idx = 34; break; // CannotDeleteObject
        case 36: idx = 36; break; // ObjectNotOwnedByYou
        case 37: idx = 35; break; // CounterHeight
        case 38: idx = 36; break; // CantBePickedup
        case 39: idx = 37; break; // InUse
        case 40: idx = 38; break; // CantBePickedupOutOfBounds
        case 41: idx = 39; break; // CantEffectFirstLevelFromSecondLevel
        case 42: idx = 40; break; // CantPlaceInAirOtherLevel
        case 43: idx = 41; break; // NotAllowedOnFloorOtherLevel
        case 44: idx = 42; break; // CannotPlaceComputerOnEndTable
        case 45: idx = 43; break;
        case 46: idx = 44; break;
        case 47: idx = 45; break;
        default: break;
    }
    static const char* fallback[] = { "Location out of bounds", "Level out of bounds", "Must be at center of tile",
        "Must be in corner", "Can't be in corner", "Must be against wall", "Can't intersect wall", "Must be over hole in floor!",
        "Must be outside", "Must be placed inside", "Can't intersect other objects" };
    if (idx < 0) return "Can't place here";
    return UI(137, idx, idx <= 10 ? fallback[idx] : "Can't place here", false);
}

}
