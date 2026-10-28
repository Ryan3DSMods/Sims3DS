/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Engine.VMTranslator
#include "simantics/engine/translator/VMTranslator.h"
#include "simantics/engine/VMSubRoutineOperand.h"
#include "simantics/engine/VMPrimitiveRegistration.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"
namespace FSO { namespace SimAntics { namespace Engine {

std::unique_ptr<VMRoutine> VMTranslator::Assemble(FSO::SimAntics::VM* vm, FSO::Files::Formats::IFF::Chunks::BHAV* bhav) {
    auto context = vm->Context;
    std::unique_ptr<VMRoutine> routine(new VMRoutine());
    routine->Locals = bhav->Locals;
    routine->Arguments = bhav->Args;
    routine->Type = bhav->Type;
    routine->ID = bhav->ChunkID;
    routine->Chunk = bhav;
    routine->VM = vm;
    routine->Rti.Name = bhav->ChunkLabel;
    routine->RuntimeVer = bhav->RuntimeVer;

    routine->Instructions.resize(bhav->Instructions.size());
    for (size_t i = 0; i < bhav->Instructions.size(); i++) {
        auto& bhavInstruction = bhav->Instructions[i];
        VMInstruction& instruction = routine->Instructions[i];
        instruction.Index = (uint8_t)i;
        instruction.Opcode = bhavInstruction.Opcode;
        instruction.Operand = nullptr;
        instruction.FalsePointer = bhavInstruction.FalsePointer;
        instruction.TruePointer = bhavInstruction.TruePointer;
        instruction.Breakpoint = bhavInstruction.Breakpoint;
        instruction.Function = routine.get();

        /** Routine call **/
        if (instruction.Opcode >= 256) {
            auto operand = new Primitives::VMSubRoutineOperand();
            operand->Read(bhavInstruction.Operand.data());
            instruction.Operand.reset(operand);
        } else {
            auto primitive = context->Primitives[instruction.Opcode].get();
            if (primitive != nullptr) {
                if (primitive->OperandModel) {
                    VMPrimitiveOperand* operand = primitive->OperandModel();
                    operand->Read(bhavInstruction.Operand.data());
                    instruction.Operand.reset(operand);
                }
            }
        }
    }
    return routine;
}

}}}
