/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims SimsVille/SimsAntics/primitives/VMRunFunctionalTree.cs
#include "simantics/engine/VMSimanticsException.h"
#include "simantics/primitives/VMRunFunctionalTree.h"
#include <cstring>
#include <memory>
#include <algorithm>
#include "core/io.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMThread.h"
#include "simantics/engine/VMQueuedAction.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/model/VMBHAVOwnerPair.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"
#include "simantics/primitives/VMFindBestObjectForFunction.h"

namespace FSO { namespace SimAntics { namespace Primitives {

using namespace FSO::SimAntics::Engine;

VMPrimitiveExitCode VMRunFunctionalTree::Execute(VMStackFrame* context, VMPrimitiveOperand* args)
{
    auto operand = static_cast<VMRunFunctionalTreeOperand*>(args);

    auto entry = VMFindBestObjectForFunction::FunctionToEntryPoint.at(operand->Function); // C# array index throws when out of range
    auto ent = context->StackObject;
    // FIX(sims): a null stack object crashed the app (segfault seen on Downtown house 24). FreeSO (TS1):
    // "if (ent == null || ent.Dead) return GOTO_FALSE" - e.g. Bar Drink 'Sit and Order' when 'find empty table' left
    // no table: the interaction just fails (was a reset per attempt).
    if (ent == nullptr || ent->Dead) return VMPrimitiveExitCode::GOTO_FALSE;
    if (entry < 0 || (size_t)entry >= ent->EntryPoints.size()) throw VMSimanticsException("Run Functional Tree: entry point out of range", context);
    if (ent->EntryPoints[entry].ActionFunction != 0)
    {
        bool Execute;
        if (ent->EntryPoints[entry].ConditionFunction != 0) //check if we can definitely execute this...
        {
            auto Behavior = ent->GetBHAVWithOwner(ent->EntryPoints[entry].ConditionFunction, context->VM()->Context);
            if (Behavior != nullptr)
            {
                auto checkFrame = std::make_shared<VMStackFrame>();
                checkFrame->Caller = context->Caller;
                checkFrame->Callee = ent;
                checkFrame->CodeOwner = Behavior->owner;
                checkFrame->StackObject = ent;
                checkFrame->Routine = context->VM()->Assemble(Behavior->bhav);
                checkFrame->Args = std::vector<int16_t>(4);
                Execute = (VMThread::EvaluateCheck(context->VM()->Context, context->Caller, checkFrame) == VMPrimitiveExitCode::RETURN_TRUE);
            }
            else Execute = true;
        }
        else
        {
            Execute = true;
        }

        if (Execute)
        {
            //push it onto our stack, except now the stack object owns our soul!
            auto tree = ent->GetBHAVWithOwner(ent->EntryPoints[entry].ActionFunction, context->VM()->Context);
            if (tree == nullptr) return VMPrimitiveExitCode::GOTO_FALSE; //does not exist
            auto routine = context->VM()->Assemble(tree->bhav);
            auto childFrame = std::make_shared<VMStackFrame>();
            childFrame->Routine = routine;
            childFrame->Caller = context->Caller;
            childFrame->Callee = ent;
            childFrame->CodeOwner = tree->owner;
            childFrame->StackObject = ent;
            childFrame->ActionTree = context->ActionTree;
            if (operand->Flags > 0 && context->ActionTree && context->Thread->ActiveAction() != nullptr) context->Thread->ActiveAction()->IconOwner(context->StackObject); // TS1(sims): FreeSO ActiveAction
            childFrame->Args = std::vector<int16_t>(routine->Arguments);
            context->Thread->Push(childFrame);
            return VMPrimitiveExitCode::CONTINUE;
        }
        else
        {
            return VMPrimitiveExitCode::GOTO_FALSE;
        }
    }
    else
    {
        return VMPrimitiveExitCode::GOTO_FALSE;
    }
}

// region VMPrimitiveOperand Members
void VMRunFunctionalTreeOperand::Read(const uint8_t* bytes)
{
    st::Reader io(bytes, VMPrimitiveOperand::OPERAND_SIZE, st::Endian::Little);
    Function = io.ReadUInt16();
    Flags = io.ReadByte();
}

void VMRunFunctionalTreeOperand::Write(uint8_t* bytes)
{
    st::Writer io(st::Endian::Little);
    io.WriteUInt16(Function);
    io.WriteByte(Flags);
    std::memcpy(bytes, io.ToArray().data(), std::min(io.ToArray().size(), VMPrimitiveOperand::OPERAND_SIZE));
}
// endregion

}}}
