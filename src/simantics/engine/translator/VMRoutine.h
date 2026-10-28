/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.VMRoutine / VMFunctionRTI (engine/translator/VMRoutine.cs)
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "simantics/_files_contract.h"
#include "simantics/engine/translator/VMInstruction.h"
namespace FSO { namespace SimAntics {
class VM;
class VMFunctionRTI {
public:
    std::string Name;
};
class VMRoutine {
public:
    VMRoutine() {}
    FSO::SimAntics::VM* VM = nullptr;
    uint8_t Type = 0;
    std::vector<VMInstruction> Instructions;
    uint16_t Locals = 0;
    uint16_t Arguments = 0;
    uint16_t ID = 0;
    /** Run time info **/
    VMFunctionRTI Rti;
    FSO::Files::Formats::IFF::Chunks::BHAV* Chunk = nullptr;
    uint32_t RuntimeVer = 0;
};
}}
