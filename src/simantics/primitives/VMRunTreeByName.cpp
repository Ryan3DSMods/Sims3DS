/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Engine.Primitives.VMRunTreeByName (primitives/VMRunTreeByName.cs)
#include "simantics/primitives/VMRunTreeByName.h"
#include <algorithm>
#include <string>
#include "core/io.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMThread.h"
#include "simantics/engine/VMSubRoutineOperand.h"
#include "simantics/engine/VMSimanticsException.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/VM.h"

namespace FSO { namespace SimAntics { namespace Primitives {
using namespace FSO::SimAntics::Engine;
using FSO::Files::Formats::IFF::Chunks::STR;

VMPrimitiveExitCode VMRunTreeByName::Execute(VMStackFrame* context, VMPrimitiveOperand* args)
{
    auto operand = static_cast<VMRunTreeByNameOperand*>(args);
    if (context->StackObject == nullptr) return VMPrimitiveExitCode::GOTO_FALSE;

    std::string name;
    STR* res = nullptr;
    if (operand->StringScope == 1)
    {//global
        res = context->Global()->Resource->Get<STR>(operand->StringTable);
    }
    else
    {//local
        if (context->Routine->ID >= 8192 && context->ScopeResource()->SemiGlobal != nullptr) res = context->ScopeResource()->SemiGlobal->Get<STR>(operand->StringTable);
        if (res == nullptr) res = context->ScopeResource()->Get<STR>(operand->StringTable);
    }
    if (res == nullptr) return VMPrimitiveExitCode::GOTO_FALSE;
    // C# GetString returns null for a missing entry; TreeByName.ContainsKey(null) then throws (below).
    bool hasName = res->TryGetString(operand->StringID - 1, FSO::Files::Formats::IFF::Chunks::STRLangCode::Default, name);

    if (context->StackObject == nullptr) throw VMSimanticsException("Object reference not set to an instance of an object.", context); // FIX(sims) QA4: was a null dereference
    if (context->StackObject->TreeByName == nullptr) return VMPrimitiveExitCode::GOTO_FALSE;
    if (!hasName) throw VMSimanticsException("Value cannot be null.\nParameter name: key", context);
    auto it = context->StackObject->TreeByName->find(name);
    if (it != context->StackObject->TreeByName->end())
    {
        auto& tree = it->second;

        if (operand->Destination == 2)
        {
            context->Thread->ExecuteSubRoutine(context, tree.bhav, context->StackObject->Object, VMSubRoutineOperand(context->Thread->TempRegisters.data()));
            return VMPrimitiveExitCode::CONTINUE;
            //push onto my stack - acts like a subroutine.
        }
        else if (operand->Destination == 0)
        {
            // FIX(sims): C# context.Thread (the running thread: a check tree's temporary thread while a pie menu
            // is built, so the phone plugins' action strings land in the menu), not the caller's main thread.
            return context->Thread->RunInMyStack(tree.bhav, context->StackObject->Object, context->Thread->TempRegisters.data(), context->StackObject)
                ? VMPrimitiveExitCode::GOTO_TRUE : VMPrimitiveExitCode::GOTO_FALSE;
            //run in my stack
        }
        else
        {
            return context->StackObject->Thread->RunInMyStack(tree.bhav, context->StackObject->Object, context->Thread->TempRegisters.data(), context->StackObject)
                ? VMPrimitiveExitCode::GOTO_TRUE : VMPrimitiveExitCode::GOTO_FALSE;
            //run in stack obj's stack
        }
        //found it! now lets call the tree ;)
    }
    else
    {
        return VMPrimitiveExitCode::GOTO_FALSE;
    }
}

void VMRunTreeByNameOperand::Read(const uint8_t* bytes)
{
    st::Reader io(bytes, 8, st::Endian::Little);
    StringTable = io.u16();
    StringScope = io.u8();
    Unused = io.u8();
    StringID = io.u8();
    Destination = io.u8();
}

void VMRunTreeByNameOperand::Write(uint8_t* bytes)
{
    st::Writer io(st::Endian::Little);
    io.WriteUInt16(StringTable);
    io.WriteByte(StringScope);
    io.WriteByte(Unused);
    io.WriteByte(StringID);
    io.WriteByte(Destination);
    auto& buf = io.ToArray();
    std::copy(buf.begin(), buf.begin() + std::min<size_t>(buf.size(), OPERAND_SIZE), bytes);
}

}}}
