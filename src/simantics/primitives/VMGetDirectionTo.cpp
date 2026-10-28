/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims SimsVille/SimsAntics/primitives/VMGetDirectionTo.cs
#include "simantics/primitives/VMGetDirectionTo.h"
#include <cmath>
#include <cstring>
#include <stdexcept>
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMMemory.h"
#include "simantics/engine/VMSimanticsException.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/VM.h"
#include "common/utils/DirectionUtils.h"
#include "core/io.h"

namespace FSO { namespace SimAntics { namespace Primitives {
using namespace FSO::SimAntics::Engine;
using namespace FSO::SimAntics::Engine::Utils;
using namespace FSO::SimAntics::Engine::Scopes;
using FSO::Common::Utils::DirectionUtils;

VMPrimitiveExitCode VMGetDirectionTo::Execute(VMStackFrame* context, VMPrimitiveOperand* args) {
    auto operand = static_cast<VMGetDirectionToOperand*>(args);

    auto obj2 = context->StackObject;
    VMEntity* obj1;
    //todo: wrong flag below?
    if ((operand->Flags & 1) == 0) obj1 = context->Caller;
    else obj1 = context->VM()->GetObjectById(VMMemory::GetVariable(context, operand->ObjectScope, operand->OScopeData));

    // C# would throw a NullReferenceException here (caught by the thread); keep that behaviour instead of crashing.
    if (obj1 == nullptr || obj2 == nullptr) throw VMSimanticsException("Get Direction To: null object reference.", context);

    auto pos1 = obj1->Position();
    auto pos2 = obj2->Position();

    auto direction = DirectionUtils::Normalize(std::atan2((double)(pos2.x - pos1.x), (double)(pos1.y - pos2.y)));

    // C# Math.Round is round-half-to-even; std::nearbyint with the default rounding mode matches.
    auto result = std::nearbyint((DirectionUtils::PosMod(direction, M_PI * 2) / M_PI) * 4);

    VMMemory::SetVariable(context, operand->ResultOwner, operand->ResultData, (int16_t)result);

    return VMPrimitiveExitCode::GOTO_TRUE;
}

void VMGetDirectionToOperand::Read(const uint8_t* bytes) {
    st::Reader io(bytes, 8, st::Endian::Little);
    ResultData = io.s16();
    ResultOwner = (VMVariableScope)io.u16();
    Flags = io.u8();
    ObjectScope = (VMVariableScope)io.u8();
    OScopeData = io.s16();

    if ((Flags & 1) == 0) {
        ObjectScope = VMVariableScope::MyObject;
        OScopeData = 11;
    }
    Flags |= 1;
}

void VMGetDirectionToOperand::Write(uint8_t* bytes) {
    // C#: BinaryWriter over new MemoryStream(bytes) (little endian, fixed 8 bytes)
    st::Writer io(st::Endian::Little);
    io.WriteInt16(ResultData);
    io.WriteUInt16((uint16_t)ResultOwner);
    io.WriteByte(Flags);
    io.WriteByte((uint8_t)ObjectScope);
    io.WriteInt16(OScopeData);
    auto& buf = io.ToArray();
    if (buf.size() > 8) throw std::runtime_error("Memory stream is not expandable.");
    std::memcpy(bytes, buf.data(), buf.size());
}

}}}
