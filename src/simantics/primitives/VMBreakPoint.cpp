// FSO.SimAntics.Primitives.VMBreakPoint (primitives/VMBreakPoint.cs)
/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
#include "simantics/primitives/VMBreakPoint.h"
#include <cstring>
#include <algorithm>
#include "core/io.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMSimanticsException.h"
#include "simantics/engine/translator/VMRoutine.h"
#include "simantics/engine/translator/VMInstruction.h"

namespace FSO { namespace SimAntics { namespace Primitives {
using namespace FSO::SimAntics::Engine;
using FSO::SimAntics::Engine::Scopes::VMVariableScope;

VMPrimitiveExitCode VMBreakPoint::Execute(VMStackFrame* context, VMPrimitiveOperand* args) {
    //todo: check condition
    // FIX(sims): a breakpoint whose true branch is itself is Maxis' "stop here in the debugger" for a state the tree
    // cannot recover from (Unleashed Bunny Rabbit 'Sub - Setup for Move' when it cannot snap back). Looping on it only
    // reached the 500,000-primitive guard and the same object reset, a CPU hitch each time; reset at once instead.
    if (context->Routine != nullptr && context->InstructionPointer < context->Routine->Instructions.size()
        && context->Routine->Instructions[context->InstructionPointer].TruePointer == context->InstructionPointer)
        throw VMSimanticsException("Breakpoint reached (a tree stopped on an error state)", context);
    return VMPrimitiveExitCode::GOTO_TRUE;
}

void VMBreakPointOperand::Read(const uint8_t* bytes) {
    st::Reader io(bytes, OPERAND_SIZE, st::Endian::Little);
    Data = io.s16();
    Scope = (VMVariableScope)io.u16();
}

void VMBreakPointOperand::Write(uint8_t* bytes) {
    st::Writer io(st::Endian::Little);
    io.WriteInt16(Data);
    io.WriteUInt16((uint16_t)Scope);
    auto& a = io.ToArray();
    std::memcpy(bytes, a.data(), std::min(a.size(), OPERAND_SIZE));
}

}}}
