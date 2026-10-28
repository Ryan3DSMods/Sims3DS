/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// TSO.SimsAntics.VMFreeWill (VMAI.cs) — FreeSims' simple visitor autonomy.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "simantics/_files_contract.h"
namespace FSO { namespace SimAntics {
class VM; class VMEntity; class VMAvatar;
class VMFreeWill {
public:
    FSO::SimAntics::VM* VM;
    std::vector<VMEntity*> Objects;
    std::vector<VMEntity*> Entities;
    std::vector<VMAvatar*> Visitors;
    VMEntity* Target = nullptr;
    VMEntity* Subject = nullptr;
    int32_t SelectedID = 0;

    explicit VMFreeWill(FSO::SimAntics::VM* vm);
    void CheckForUsableObjects(FSO::SimAntics::VM* vm);
    void RunAction(VMEntity* entity);
    void Tick(FSO::SimAntics::VM* vm);
private:
    FSO::Files::Formats::IFF::Chunks::TTAB* TreeTableSelected = nullptr;
    std::vector<std::string> interactionList;
    void SetSelected(VMEntity* entity, bool isPet);
    void ExecuteAction(VMEntity* avatar, VMEntity* target);
    int32_t RandNext(int32_t min, int32_t max); // C# new Random().Next(min, max) (exclusive max)
};
}}
