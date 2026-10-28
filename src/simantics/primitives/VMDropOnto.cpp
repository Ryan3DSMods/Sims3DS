/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Primitives.VMDropOnto (primitives/VMDropOnto.cs)
#include "simantics/primitives/VMDropOnto.h"
#include <cstring>
#include "core/io.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMSimanticsException.h"
#include "simantics/engine/VMThread.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"

namespace FSO { namespace SimAntics { namespace Primitives {

using namespace FSO::SimAntics::Engine;

VMPrimitiveExitCode VMDropOnto::Execute(VMStackFrame* context, VMPrimitiveOperand* args)
{
    auto operand = static_cast<VMDropOntoOperand*>(args);
    auto src = (operand->SrcSlotMode == 1) ? (uint16_t)context->Args[operand->SrcSlotNum] : operand->SrcSlotNum;
    auto dest = (operand->DestSlotMode == 1) ? (uint16_t)context->Args[operand->DestSlotNum] : operand->DestSlotNum;

    auto item = context->Caller->GetSlot(src);
    if (item != nullptr)
    {
        // FIX(sims) QA4: a missing stack object was a null dereference (segfault); C# throws (the Sim resets).
        if (context->StackObject == nullptr) throw VMSimanticsException("Object reference not set to an instance of an object.", context);
        auto itemTest = context->StackObject->GetSlot(dest);
        if (itemTest == nullptr)
        {
            return (context->StackObject->PlaceInSlot(item, dest, true, context->VM()->Context)) ? VMPrimitiveExitCode::GOTO_TRUE : VMPrimitiveExitCode::GOTO_FALSE;
        }
        else return VMPrimitiveExitCode::GOTO_FALSE; //cannot replace items currently in slots
    }
    else return VMPrimitiveExitCode::GOTO_FALSE;
}

void VMDropOntoOperand::Read(const uint8_t* bytes)
{
    st::Reader io(bytes, 8, st::Endian::Little);
    SrcSlotMode = io.ReadUInt16();
    SrcSlotNum = io.ReadUInt16();
    DestSlotMode = io.ReadUInt16();
    DestSlotNum = io.ReadUInt16();
}

void VMDropOntoOperand::Write(uint8_t* bytes)
{
    st::Writer io(st::Endian::Little);
    io.WriteUInt16(SrcSlotMode);
    io.WriteUInt16(SrcSlotNum);
    io.WriteUInt16(DestSlotMode);
    io.WriteUInt16(DestSlotNum);
    auto& data = io.ToArray();
    std::memcpy(bytes, data.data(), data.size() < 8 ? data.size() : 8);
}

}}}
