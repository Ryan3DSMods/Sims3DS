/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Engine.Primitives.VMRemoveObjectInstance (primitives/VMRemoveObjectInstance.cs)
#include "simantics/primitives/VMRemoveObjectInstance.h"
#include <algorithm>
#include "core/io.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"

namespace FSO { namespace SimAntics { namespace Primitives {
using namespace FSO::SimAntics::Engine;

VMPrimitiveExitCode VMRemoveObjectInstance::Execute(VMStackFrame* context, VMPrimitiveOperand* args)
{
    auto operand = static_cast<VMRemoveObjectInstanceOperand*>(args);
    VMEntity* obj;
    if (operand->Target == 0) obj = context->Caller;
    else obj = context->StackObject;

    // FIX(sims): FreeSO always deletes the whole multitile group (`obj?.Delete(true, ...)`). FreeSims passed the
    // CleanupAll flag, so a vehicle removing itself without it left its other tiles behind: every Old Town / community
    // visitor trolley (24 tiles) piled up out of world (15 trolleys = 360 orphan objects after 8 game hours).
    if (obj != nullptr)
        obj->Delete(true, context->VM()->Context);

    //if (obj == context.StackObject) context.StackObject = null;

    // FIX(sims): FreeSO "yield if we are going to delete" — an object that removes itself stops running this
    // tick (the expansion pack controllers remove themselves on lots they do not belong to and then idle forever;
    // without the yield their next sleep never yields and the thread hits the infinite loop guard).
    return (obj != nullptr && obj == context->Caller) ? VMPrimitiveExitCode::GOTO_TRUE_NEXT_TICK : VMPrimitiveExitCode::GOTO_TRUE;
}

void VMRemoveObjectInstanceOperand::Read(const uint8_t* bytes)
{
    st::Reader io(bytes, 8, st::Endian::Little);
    Target = io.s16();
    Flags = io.u8();
}

void VMRemoveObjectInstanceOperand::Write(uint8_t* bytes)
{
    st::Writer io(st::Endian::Little);
    io.WriteInt16(Target);
    io.WriteByte(Flags);
    auto& buf = io.ToArray();
    std::copy(buf.begin(), buf.begin() + std::min<size_t>(buf.size(), OPERAND_SIZE), bytes);
}

}}}
