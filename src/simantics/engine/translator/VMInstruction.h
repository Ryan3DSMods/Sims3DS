/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.VMInstruction / VMInstructionRTI (engine/translator/VMInstruction.cs)
#pragma once
#include <cstdint>
#include <string>
#include <memory>
#include "simantics/engine/VMPrimitiveOperand.h"
namespace FSO { namespace SimAntics {
class VMRoutine;
class VMInstructionRTI {
public:
    std::string Description;
    std::string Comment;
};
class VMInstruction {
public:
    VMRoutine* Function = nullptr;
    uint16_t Opcode = 0;
    uint8_t TruePointer = 0;
    uint8_t FalsePointer = 0;
    uint8_t Index = 0;
    std::unique_ptr<Engine::VMPrimitiveOperand> Operand;
    bool Breakpoint = false;
    /** Runtime info **/
    VMInstructionRTI Rti;
};
}}
