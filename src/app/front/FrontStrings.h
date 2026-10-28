/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims front end — the game's own texts for the front end (GameData/UIText.iff STR# tables, English),
// kept raw (with their %s / %d placeholders and newlines). Only the tables used here are kept in memory.
//   128 PickFamilyStrs   131 EvictModeStrs   132 MoveInModeStrs   134 NghRollover     145 optionstrs
//   151 NghbBtnTips      152 DefaultDialogButtons                 153 AppStrings      154 MiscStrings
//   155 splashprogress   169/170/173/174 Downtown/Vacation/Studio Town/Magic Town screen tooltips
//   212/213 Go Downtown / Vacation dialogs   225 change-button failure tooltips   260/261 visit mode
#pragma once
#include <map>
#include <string>
#include <vector>

namespace simapp { namespace front {

class FrontStrings {
public:
    void Load(const std::string& gameDataDir);
    /// The string, or `fallback` when the table/index is missing or empty.
    std::string Get(int table, int index, const char* fallback = "") const;
    bool Loaded() const { return !_Strings.empty(); }
    static FrontStrings& Get();

private:
    std::map<int, std::string> _Strings; // table * 10000 + index
};

/// printf-style substitution of %s / %d in order (TS1 strings use both), Latin-1.
std::string Format(const std::string& fmt, const std::vector<std::string>& args);
/// "§1,234": '$' with the original font (its Simoleon glyph), the Latin-1 section sign otherwise.
std::string Simoleons(int v);

}}
