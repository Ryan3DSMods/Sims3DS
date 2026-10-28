/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Primitives.VMIdleForInput (primitives/VMIdleForInput.cs)
#include "simantics/primitives/VMIdleForInput.h"
#include <cstring>
#include "core/io.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMThread.h"
#include "simantics/engine/VMMemory.h"
#include "simantics/engine/VMSimanticsException.h"
#include "simantics/engine/scopes/VMVariableScope.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"

namespace FSO { namespace SimAntics { namespace Primitives {

using namespace FSO::SimAntics::Engine;
using FSO::SimAntics::Engine::Utils::VMMemory;

VMPrimitiveExitCode VMIdleForInput::Execute(VMStackFrame* context, VMPrimitiveOperand* args) //TODO: Behaviour for being notified out of idle and interaction canceling
{
    auto operand = static_cast<VMIdleForInputOperand*>(args);

    // TS1(sims): FreeSO — decrement first (the C# scheduler may skip ticks; here every tick runs, so it is 1).
    if (operand->StackVarToDec >= 0 && operand->StackVarToDec < (int)context->Args.size()) context->Args[operand->StackVarToDec] -= 1;
    else throw VMSimanticsException("Idle For Input: parameter out of range", context);

    //if we're main, attempt to run a queued interaction. We just idle if this fails.
    // TS1(sims): FreeSO — in TS1 an action tree may Allow Push too (nested interactions, e.g. social responses).
    if (operand->AllowPush == 1 && (context->VM()->TS1 || !context->ActionTree) && context->Thread->AttemptPush())
    {
        return VMPrimitiveExitCode::CONTINUE; //control handover
        //TODO: does this forcefully end the rest of the idle? (force a true return, must loop back to run again)
    }

    // TS1(sims): FreeSO — notified when the running interaction is to be cancelled, or (main) anything is queued.
    if ((context->ActionTree && context->Thread->ActiveAction() != nullptr && context->Thread->ActiveAction()->Cancelled) || (!context->ActionTree && context->Thread->Queue.size() > 0))
    {
        context->Caller->SetFlag(VMEntityFlags::NotifiedByIdleForInput, true);
        return VMPrimitiveExitCode::GOTO_TRUE;
    }

    if (context->Thread->Interrupt)
    {
        context->Thread->Interrupt = false;

        return VMPrimitiveExitCode::GOTO_TRUE;
    }

    if (context->Args[operand->StackVarToDec] < 0)
    {
        return VMPrimitiveExitCode::GOTO_TRUE;
    }
    else
    {
        return VMPrimitiveExitCode::CONTINUE_NEXT_TICK; //interactions must be checked every tick
    }
}

void VMIdleForInputOperand::Read(const uint8_t* bytes)
{
    st::Reader io(bytes, 8, st::Endian::Little);
    StackVarToDec = io.ReadInt16();
    AllowPush = io.ReadUInt16();
}

void VMIdleForInputOperand::Write(uint8_t* bytes)
{
    st::Writer io(st::Endian::Little);
    io.WriteInt16(StackVarToDec);
    io.WriteUInt16(AllowPush);
    auto& data = io.ToArray();
    std::memcpy(bytes, data.data(), data.size() < 8 ? data.size() : 8);
}

}}}
