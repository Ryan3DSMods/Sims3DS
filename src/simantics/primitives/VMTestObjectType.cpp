/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims SimsVille/SimsAntics/primitives/VMTestObjectType.cs
#include "simantics/primitives/VMTestObjectType.h"
#include <cstring>
#include <stdexcept>
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMMemory.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/VM.h"
#include "core/io.h"

namespace FSO { namespace SimAntics { namespace Primitives {
using namespace FSO::SimAntics::Engine;
using namespace FSO::SimAntics::Engine::Utils;
using namespace FSO::SimAntics::Engine::Scopes;

VMPrimitiveExitCode VMTestObjectType::Execute(VMStackFrame* context, VMPrimitiveOperand* args) {
    auto operand = static_cast<VMTestObjectTypeOperand*>(args);
    auto objectID = VMMemory::GetVariable(context, operand->IdOwner, operand->IdData);

    auto obj = context->VM()->GetObjectById(objectID);
    //var obj = context.StackObject;
    if (obj == nullptr) {
        return VMPrimitiveExitCode::ERROR;
    }

    if (obj->Object->GUID == operand->GUID) return VMPrimitiveExitCode::GOTO_TRUE; //is my guid same?
    else if (obj->MasterDefinition != nullptr && (obj->MasterDefinition->GUID == operand->GUID)) return VMPrimitiveExitCode::GOTO_TRUE; //is master guid same?
    else return VMPrimitiveExitCode::GOTO_FALSE;
}

void VMTestObjectTypeOperand::Read(const uint8_t* bytes) {
    st::Reader io(bytes, 8, st::Endian::Little);
    GUID = io.u32();
    IdData = io.s16();
    IdOwner = (VMVariableScope)io.u8();
}

void VMTestObjectTypeOperand::Write(uint8_t* bytes) {
    // C#: BinaryWriter over new MemoryStream(bytes) (little endian, fixed 8 bytes)
    st::Writer io(st::Endian::Little);
    io.WriteUInt32(GUID);
    io.WriteInt16(IdData);
    io.WriteByte((uint8_t)IdOwner);
    auto& buf = io.ToArray();
    if (buf.size() > 8) throw std::runtime_error("Memory stream is not expandable.");
    std::memcpy(bytes, buf.data(), buf.size());
}

}}}
