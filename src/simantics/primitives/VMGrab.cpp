/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Primitives.VMGrab (primitives/VMGrab.cs)
#include "simantics/primitives/VMGrab.h"
#include "core/io.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMSimanticsException.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"

namespace FSO { namespace SimAntics { namespace Primitives {
using namespace FSO::SimAntics::Engine;

VMPrimitiveExitCode VMGrab::Execute(VMStackFrame* context, VMPrimitiveOperand* args)
{
    auto operand = static_cast<VMGrabOperand*>(args);
    (void)operand;

    if (context->Caller == nullptr) throw VMSimanticsException("Object reference not set to an instance of an object.", context);
    return (context->Caller->PlaceInSlot(context->StackObject, 0, true, context->VM()->Context)) ? VMPrimitiveExitCode::GOTO_TRUE : VMPrimitiveExitCode::GOTO_FALSE;

    // return VMPrimitiveExitCode.GOTO_TRUE;
}

void VMGrabOperand::Read(const uint8_t* bytes)
{
    st::Reader io(bytes, 8, st::Endian::Little);
    (void)io;
}

void VMGrabOperand::Write(uint8_t* bytes) { (void)bytes; }

}}}
