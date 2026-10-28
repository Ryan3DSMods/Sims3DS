/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims SimsVille/SimsAntics/primitives/VMNotifyOutOfIdle.cs
#include "simantics/primitives/VMNotifyOutOfIdle.h"
#include "core/io.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMThread.h"
#include "simantics/entities/VMEntity.h"

namespace FSO { namespace SimAntics { namespace Primitives {

using namespace FSO::SimAntics::Engine;

VMPrimitiveExitCode VMNotifyOutOfIdle::Execute(VMStackFrame* context, VMPrimitiveOperand* args)
{
    (void)args;
    // FIX(sims) QA4: FreeSO `if (context.StackObject?.Thread != null)`. FreeSims dereferenced a missing stack object:
    // a segmentation fault on Downtown 24 (seed 1, 20 downtownies) when the Sim to wake had left the lot.
    if (context->StackObject != nullptr && context->StackObject->Thread != nullptr)
        context->StackObject->Thread->Interrupt = true;
    return VMPrimitiveExitCode::GOTO_TRUE;
}

// region VMPrimitiveOperand Members
void VMNotifyOutOfIdleOperand::Read(const uint8_t* bytes)
{
    st::Reader io(bytes, VMPrimitiveOperand::OPERAND_SIZE, st::Endian::Little);
    (void)io;
}

void VMNotifyOutOfIdleOperand::Write(uint8_t* bytes) { (void)bytes; }
// endregion

}}}
