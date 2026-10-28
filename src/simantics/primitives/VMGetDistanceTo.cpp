// FSO.SimAntics.Primitives.VMGetDistanceTo (primitives/VMGetDistanceTo.cs)
/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
#include "simantics/primitives/VMGetDistanceTo.h"
#include <cstring>
#include <cmath>
#include <algorithm>
#include "core/io.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMThread.h"
#include "simantics/engine/VMMemory.h"
#include "simantics/engine/VMSimanticsException.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/VM.h"

namespace FSO { namespace SimAntics { namespace Primitives {
using namespace FSO::SimAntics::Engine;
using FSO::SimAntics::Engine::Utils::VMMemory;
using FSO::SimAntics::Engine::Scopes::VMVariableScope;

VMPrimitiveExitCode VMGetDistanceTo::Execute(VMStackFrame* context, VMPrimitiveOperand* args) {
    auto operand = static_cast<VMGetDistanceToOperand*>(args);

    auto obj1 = context->StackObject;
    VMEntity* obj2;
    if ((operand->Flags & 1) == 0) obj2 = context->Caller;
    else obj2 = context->VM()->GetObjectById(VMMemory::GetVariable(context, (VMVariableScope)operand->ObjectScope, operand->OScopeData));

    // C# NullReferenceException on .Position -> exception the thread catches.
    if (obj1 == nullptr || obj2 == nullptr) throw VMSimanticsException("NullReferenceException", context);

    auto pos1 = obj1->Position();
    auto pos2 = obj2->Position();

    auto result = (int16_t)std::floor(std::sqrt(std::pow((double)(pos1.x - pos2.x), 2) + std::pow((double)(pos1.y - pos2.y), 2)) / 16.0);

    context->Thread->TempRegisters.at(operand->TempNum) = result;
    return VMPrimitiveExitCode::GOTO_TRUE;
}

void VMGetDistanceToOperand::Read(const uint8_t* bytes) {
    st::Reader io(bytes, OPERAND_SIZE, st::Endian::Little);
    TempNum = io.u16();
    Flags = io.u8();
    ObjectScope = (VMVariableScope)io.u8();
    OScopeData = io.s16();

    if ((Flags & 1) == 0) {
        ObjectScope = VMVariableScope::MyObject;
        OScopeData = 11;
    }
    Flags |= 1;
}

void VMGetDistanceToOperand::Write(uint8_t* bytes) {
    st::Writer io(st::Endian::Little);
    io.WriteUInt16(TempNum);
    io.WriteByte(Flags);
    io.WriteByte((uint8_t)ObjectScope);
    io.WriteInt16(OScopeData);
    auto& a = io.ToArray();
    std::memcpy(bytes, a.data(), std::min(a.size(), OPERAND_SIZE));
}

}}}
