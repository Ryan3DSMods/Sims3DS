/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Primitives.VMTestSimInteractingWith (primitives/VMTestSimInteractingWith.cs)
#include "simantics/primitives/VMTestSimInteractingWith.h"
#include "core/io.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMThread.h"
#include "simantics/engine/VMQueuedAction.h"
#include "simantics/engine/VMSimanticsException.h"
#include "simantics/entities/VMEntity.h"

namespace FSO { namespace SimAntics { namespace Primitives {

using namespace FSO::SimAntics::Engine;

VMPrimitiveExitCode VMTestSimInteractingWith::Execute(VMStackFrame* context, VMPrimitiveOperand* args)
{
    (void)args;
    //if caller's active interaction is with stack object, return true.
    auto& callerStack = context->Caller->Thread->Stack;
    VMStackFrame* callerActive = callerStack.empty() ? nullptr : callerStack.back().get(); // C#: Stack.LastOrDefault()
    // TS1(sims): FreeSO — the caller's active interaction (FreeSims: Queue[0]).
    auto activeAction = context->Caller->Thread->ActiveAction();
    return (callerActive != nullptr && callerActive->ActionTree && activeAction != nullptr && activeAction->Callee == context->StackObject)
        ? VMPrimitiveExitCode::GOTO_TRUE : VMPrimitiveExitCode::GOTO_FALSE;
}

void VMTestSimInteractingWithOperand::Read(const uint8_t* bytes)
{
    st::Reader io(bytes, 8, st::Endian::Little);
    (void)io;
    //nothing! zip! zilch! nada!
}

void VMTestSimInteractingWithOperand::Write(uint8_t* bytes) { (void)bytes; }

}}}
