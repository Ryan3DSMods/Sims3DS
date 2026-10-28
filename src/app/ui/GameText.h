/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims app — the extra game strings the live/buy/build panels use, read from the game's own files:
// GameData/UIText.iff (STR# 133 RegularModeStrs, 134?, 137 PlacementErrs, 138 VCtlTips, 139 BldTips,
// 145 optionstrs, 150 BuyModeCatalogSortTips, 160 CatalogRatings, 200-207 Function Sub Sorts,
// 210 Function Sub Sort Extra Strings) and GameData/Live.iff (STR# 130 Motives, 131 Personality,
// 132 Relationships, 133 HouseSubpanelLabels, 134 ModeTips, 136 JobSubpanelLabels, 137 JobSubpanelPopupText).
// TS1(sims): FreeSims reads TSO's .cst string tables (GameFacade.Strings.GetString("137", ...)); these are
// the TS1 equivalents. English fallbacks keep the UI readable without the files.
#pragma once
#include <map>
#include <string>

namespace simapp {

class GameText {
public:
    static GameText& Get();
    void Load(const std::string& gameDataDir);
    /// UIText.iff string (raw, hotkey suffix stripped when strip = true).
    std::string UI(int table, int index, const char* fallback, bool strip = true) const;
    /// Live.iff string.
    std::string Live(int table, int index, const char* fallback) const;
    /// TS1 placement error text for a FreeSims VMPlacementError (TSO cst 137 order differs from TS1 STR# 137).
    std::string PlacementError(int vmPlacementError) const;
    bool Loaded() const { return !_UI.empty(); }

private:
    std::map<int, std::string> _UI, _Live; // table * 10000 + index
};

}
