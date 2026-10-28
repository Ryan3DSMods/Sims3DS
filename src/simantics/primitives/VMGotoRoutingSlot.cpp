// FSO.SimAntics.Engine.Primitives.VMGotoRoutingSlot (primitives/VMGotoRoutingSlot.cs)
/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
#include "simantics/primitives/VMGotoRoutingSlot.h"
#include <cstring>
#include <algorithm>
#include "core/io.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMThread.h"
#include "simantics/engine/VMMemory.h"
#include "simantics/engine/VMRoutingFrame.h"
#include "simantics/entities/VMEntity.h"

namespace FSO { namespace SimAntics { namespace Primitives {
using namespace FSO::SimAntics::Engine;
using FSO::SimAntics::Engine::Utils::VMMemory;
using FSO::SimAntics::Engine::Scopes::VMSlotScope;

VMPrimitiveExitCode VMGotoRoutingSlot::Execute(VMStackFrame* context, VMPrimitiveOperand* args) {
    auto operand = static_cast<VMGotoRoutingSlotOperand*>(args);

    if (context->Thread->IsCheck) return VMPrimitiveExitCode::GOTO_FALSE;

    auto slot = VMMemory::GetSlot(context, operand->Type, operand->Data);
    if (slot == nullptr) return VMPrimitiveExitCode::GOTO_FALSE;
    auto obj = context->StackObject;
    if (obj == nullptr) return VMPrimitiveExitCode::GOTO_FALSE;
    auto avatar = context->Caller;
    (void)avatar;

    //Routing slots must be type 3.
    if (slot->Type == 3) {
        auto pathFinder = context->Thread->PushNewRoutingFrame(context, !operand->NoFailureTrees());
        auto success = pathFinder->InitRoutes(slot, context->StackObject);
        (void)success;

        return VMPrimitiveExitCode::CONTINUE;
    }
    else return VMPrimitiveExitCode::GOTO_FALSE;
}

void VMGotoRoutingSlotOperand::Read(const uint8_t* bytes) {
    st::Reader io(bytes, OPERAND_SIZE, st::Endian::Little);
    Data = io.u16();
    Type = (VMSlotScope)io.u16();
    Flags = io.u8();
}

void VMGotoRoutingSlotOperand::Write(uint8_t* bytes) {
    st::Writer io(st::Endian::Little);
    io.WriteUInt16(Data);
    io.WriteUInt16((uint16_t)Type);
    io.WriteByte(Flags);
    auto& a = io.ToArray();
    std::memcpy(bytes, a.data(), std::min(a.size(), OPERAND_SIZE));
}

std::string VMGotoRoutingSlotOperand::ToString() const {
    // C# Enum.ToString(): the member name, or the number when undefined.
    std::string typeName;
    switch (Type) {
        case VMSlotScope::StackVariable: typeName = "StackVariable"; break;
        case VMSlotScope::Literal: typeName = "Literal"; break;
        case VMSlotScope::Global: typeName = "Global"; break;
        default: typeName = std::to_string((int)Type); break;
    }
    return "Go To Routing Slot (" + typeName + ": #" + std::to_string(Data) + ")";
}

}}}
