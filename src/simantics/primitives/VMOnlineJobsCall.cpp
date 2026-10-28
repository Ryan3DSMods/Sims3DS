/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Primitives.VMOnlineJobsCall (primitives/VMOnlineJobsCall.cs)
#include "simantics/primitives/VMOnlineJobsCall.h"
#include "core/io.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/VM.h"

namespace FSO { namespace SimAntics { namespace Primitives {
using namespace FSO::SimAntics::Engine;

VMPrimitiveExitCode VMOnlineJobsCall::Execute(VMStackFrame* context, VMPrimitiveOperand* args)
{
    auto operand = static_cast<VMOnlineJobsCallOperand*>(args);

    switch (operand->Call)
    {
        case VMOnlineJobsCallMode::SetControllerID:
            context->VM()->SetGlobalValue(21, (context->StackObject == nullptr) ? (int16_t)0 : context->StackObject->ObjectID);
            break;
        default:
            break;
    }

    return VMPrimitiveExitCode::GOTO_TRUE;
}

void VMOnlineJobsCallOperand::Read(const uint8_t* bytes)
{
    st::Reader io(bytes, 8, st::Endian::Little);
    Call = (VMOnlineJobsCallMode)io.u8();
}

void VMOnlineJobsCallOperand::Write(uint8_t* bytes)
{
    bytes[0] = (uint8_t)Call;
}

}}}
