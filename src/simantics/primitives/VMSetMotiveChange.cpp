/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Primitives.VMSetMotiveChange (primitives/VMSetMotiveChange.cs)
#include "simantics/primitives/VMSetMotiveChange.h"
#include <cstring>
#include "core/io.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMThread.h"
#include "simantics/engine/VMMemory.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"

namespace FSO { namespace SimAntics { namespace Primitives {

using namespace FSO::SimAntics::Engine;
using FSO::SimAntics::Engine::Utils::VMMemory;
using FSO::SimAntics::Engine::Scopes::VMVariableScope;

VMPrimitiveExitCode VMSetMotiveChange::Execute(VMStackFrame* context, VMPrimitiveOperand* args)
{
    auto operand = static_cast<VMSetMotiveChangeOperand*>(args);
    auto avatar = static_cast<VMAvatar*>(context->Caller);

    if (operand->Once()) { }

    if (operand->ClearAll())
    {
        avatar->ClearMotiveChanges();
    }
    else
    {
        auto rate = VMMemory::GetVariable(context, (VMVariableScope)operand->DeltaOwner, operand->DeltaData);
        auto MaxValue = VMMemory::GetVariable(context, (VMVariableScope)operand->MaxOwner, operand->MaxData);
        if (operand->Once()) {
            int16_t motive = avatar->GetMotiveData(operand->Motive);

            if (((rate > 0) && (motive > MaxValue)) || ((rate < 0) && (motive < MaxValue))) { return VMPrimitiveExitCode::GOTO_TRUE; }
            // ^ we're already over, do nothing. (do NOT clamp)

            motive += rate;
            if (((rate > 0) && (motive > MaxValue)) || ((rate < 0) && (motive < MaxValue))) { motive = MaxValue; }
            avatar->SetMotiveData(operand->Motive, motive);
        }
        else avatar->SetMotiveChange(operand->Motive, rate, MaxValue);
    }

    return VMPrimitiveExitCode::GOTO_TRUE;
}

void VMSetMotiveChangeOperand::Read(const uint8_t* bytes)
{
    st::Reader io(bytes, 8, st::Endian::Little);

    DeltaOwner = (VMVariableScope)io.ReadByte();
    MaxOwner = (VMVariableScope)io.ReadByte();
    Motive = (Model::VMMotive)io.ReadByte();
    Flags = (VMSetMotiveChangeFlags)io.ReadByte();

    DeltaData = io.ReadInt16();
    MaxData = io.ReadInt16();
}

void VMSetMotiveChangeOperand::Write(uint8_t* bytes)
{
    st::Writer io(st::Endian::Little);
    io.WriteByte((uint8_t)DeltaOwner);
    io.WriteByte((uint8_t)MaxOwner);
    io.WriteByte((uint8_t)Motive);
    io.WriteByte((uint8_t)Flags);
    io.WriteInt16(DeltaData);
    io.WriteInt16(MaxData);
    auto& data = io.ToArray();
    std::memcpy(bytes, data.data(), data.size() < 8 ? data.size() : 8);
}

}}}
