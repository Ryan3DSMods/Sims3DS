/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims SimsVille/SimsAntics/primitives/VMRefresh.cs
#include "simantics/primitives/VMRefresh.h"
#include <cstring>
#include <algorithm>
#include "core/io.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMSimanticsException.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMGameObject.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"

namespace FSO { namespace SimAntics { namespace Primitives {

using namespace FSO::SimAntics::Engine;

VMPrimitiveExitCode VMRefresh::Execute(VMStackFrame* context, VMPrimitiveOperand* args)
{
    auto operand = static_cast<VMRefreshOperand*>(args);
    VMEntity* target = nullptr;
    switch (operand->TargetObject)
    {
        case 0:
            target = context->Caller;
            break;
        case 1:
            target = context->StackObject;
            break;
    }

    switch (operand->RefreshType)
    {
        case 0: //graphic
            if (dynamic_cast<VMGameObject*>(target) != nullptr)
            {
                auto TargObj = static_cast<VMGameObject*>(target);
                TargObj->RefreshGraphic();
            }
            break;
        case 1:
            // C# dereferences target here (NullReferenceException, caught by the thread) when TargetObject is unknown.
            if (target == nullptr) throw VMSimanticsException("Object reference not set to an instance of an object.", context);
            context->VM()->Context->RefreshLighting(context->VM()->Context->GetObjectRoom(target), true);
            if (dynamic_cast<VMGameObject*>(target) != nullptr) static_cast<VMGameObject*>(target)->RefreshLight();
            break;
        case 2: // TS1(sims): FreeSO — "area contribution" (Behavior.iff STR#212 'room score contribution')
            if (target == nullptr) throw VMSimanticsException("Object reference not set to an instance of an object.", context);
            context->VM()->Context->RefreshRoomScore(context->VM()->Context->GetObjectRoom(target));
            break;
    }

    return VMPrimitiveExitCode::GOTO_TRUE;
}

// region VMPrimitiveOperand Members
void VMRefreshOperand::Read(const uint8_t* bytes)
{
    st::Reader io(bytes, VMPrimitiveOperand::OPERAND_SIZE, st::Endian::Little);
    TargetObject = io.ReadInt16();
    RefreshType = io.ReadInt16();
}

void VMRefreshOperand::Write(uint8_t* bytes)
{
    st::Writer io(st::Endian::Little);
    io.WriteInt16(TargetObject);
    io.WriteInt16(RefreshType);
    std::memcpy(bytes, io.ToArray().data(), std::min(io.ToArray().size(), VMPrimitiveOperand::OPERAND_SIZE));
}
// endregion

}}}
