/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Engine.Primitives.VMRandomNumber (primitives/VMRandomNumber.cs)
#include "simantics/primitives/VMRandomNumber.h"
#include <cstring>
#include <string>
#include "core/io.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMThread.h"
#include "simantics/engine/VMMemory.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"

namespace FSO { namespace SimAntics { namespace Primitives {

using namespace FSO::SimAntics::Engine;
using FSO::SimAntics::Engine::Utils::VMMemory;
using FSO::SimAntics::Engine::Scopes::VMVariableScope;

namespace {
// C# string.Trim('\0')
std::string TrimNul(const std::string& s)
{
    size_t b = s.find_first_not_of('\0');
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of('\0');
    return s.substr(b, e - b + 1);
}
}

VMPrimitiveExitCode VMRandomNumber::Execute(VMStackFrame* context, VMPrimitiveOperand* args)
{
    auto operand = static_cast<VMRandomNumberOperand*>(args);
    auto rangeValue = (uint16_t)VMMemory::GetVariable(context, operand->RangeScope, operand->RangeData);

    if (TrimNul(context->Routine->Rti.Name) == "spread - m") { }
    auto result = context->VM()->Context->NextRandom(rangeValue);
    VMMemory::SetVariable(context, operand->DestinationScope, operand->DestinationData, (int16_t)result);
    return VMPrimitiveExitCode::GOTO_TRUE;
}

void VMRandomNumberOperand::Read(const uint8_t* bytes)
{
    st::Reader io(bytes, 8, st::Endian::Little);
    DestinationData = io.ReadInt16();
    DestinationScope = (VMVariableScope)io.ReadUInt16();
    RangeData = io.ReadInt16();
    RangeScope = (VMVariableScope)io.ReadUInt16();
}

void VMRandomNumberOperand::Write(uint8_t* bytes)
{
    st::Writer io(st::Endian::Little);
    io.WriteInt16(DestinationData);
    io.WriteUInt16((uint16_t)DestinationScope);
    io.WriteInt16(RangeData);
    io.WriteUInt16((uint16_t)RangeScope);
    auto& data = io.ToArray();
    std::memcpy(bytes, data.data(), data.size() < 8 ? data.size() : 8);
}

}}}
