/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Engine.VMSuitProvider
// C# returns `object`: a ulong outfit id, a string ".apr" appearance name, or null. That is VMSuitResult here.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "simantics/engine/scopes/VMSuitScope.h"
namespace FSO { namespace SimAntics { class VMAvatar; namespace Engine {
class VMStackFrame;
struct VMSuitResult {
    enum class Kind { Null, OutfitID, AppearanceName } Type = Kind::Null;
    uint64_t OutfitID = 0;
    std::string AppearanceName;
    static VMSuitResult Outfit(uint64_t id) { VMSuitResult r; r.Type = Kind::OutfitID; r.OutfitID = id; return r; }
    static VMSuitResult Appearance(const std::string& s) { VMSuitResult r; r.Type = Kind::AppearanceName; r.AppearanceName = s; return r; }
    bool IsNull() const { return Type == Kind::Null; }
};
class VMSuitProvider {
public:
    static VMSuitResult GetSuit(VMStackFrame* context, Scopes::VMSuitScope scope, uint16_t id);
    // TS1(sims): FreeSO GetPersonSuitTS1 (see VMSuitProvider.cpp / content/TS1Clothing.h).
    static VMSuitResult GetPersonSuitTS1(FSO::SimAntics::VMAvatar* avatar, uint16_t id);
    /// The Sim's body strings (its character's STR# OBJD.BodyStringID), as text.
    static std::vector<std::string> BodyStringsOf(FSO::SimAntics::VMAvatar* avatar);
    /// Registers "mesh,BODY=texture" as a TS1 outfit with the hands of `bodyStrings` (17..22); 0 if empty.
    static uint64_t RegisterTS1Outfit(const std::string& bodyString, const std::vector<std::string>& bodyStrings);
private:
    static const int64_t JobOutfits[2][5][3];
};
}}}
