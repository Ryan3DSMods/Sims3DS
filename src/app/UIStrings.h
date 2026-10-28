/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims app — labels from the game's own GameData/UIText.iff (STR# tables), with English fallbacks.
#pragma once
#include <string>
#include <memory>
#include <map>

namespace simapp {

class UIStrings {
public:
    ~UIStrings();
    void Load(const std::string& gameDataDir);
    /// STR# table/index from UIText.iff, or `fallback`. Hotkey suffixes (" - P", "  +") are stripped.
    std::string Get(int table, int index, const char* fallback) const;

    // the labels the app uses
    std::string Motive(int i) const;   // 0..7 in panel order: Hunger Comfort Hygiene Bladder / Energy Fun Social Room
    std::string Speed(int i) const;    // 0 pause, 1 normal, 2 high, 3 ultra
    std::string Walls(int mode) const; // 0 cutaway, 1 down, 2 up
    bool Loaded() const { return !Strings.empty(); }

private:
    // only the tables the app uses are kept (UIText.iff itself is ~2 MB in memory)
    std::map<int, std::string> Strings; // table * 10000 + index
};

}
