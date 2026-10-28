// FSO.SimAntics.Engine.Primitives.VMPushInteraction (primitives/VMPushInteraction.cs)
/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
#include "simantics/primitives/VMPushInteraction.h"
#include <cstring>
#include <algorithm>
#include "core/io.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMThread.h"
#include "simantics/engine/VMQueuedAction.h"
#include "simantics/engine/VMSimanticsException.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/model/VMPersonDataVariable.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"

namespace FSO { namespace SimAntics { namespace Primitives {
using namespace FSO::SimAntics::Engine;
using FSO::Files::Formats::IFF::Chunks::TTABFlags;

VMPrimitiveExitCode VMPushInteraction::Execute(VMStackFrame* context, VMPrimitiveOperand* args) {
    auto operand = static_cast<VMPushInteractionOperand*>(args);
    VMEntity* interactionSource;

    if ((operand->Flags & (1 << 1)) > 0) interactionSource = context->VM()->GetObjectById((int16_t)context->Locals.at(operand->ObjectLocation));
    else interactionSource = context->VM()->GetObjectById((int16_t)context->Args.at(operand->ObjectLocation));

    int16_t priority = 0;
    VMQueueMode mode = VMQueueMode::Normal;
    switch (operand->Priority) {
        case VMPushPriority::Inherited: {
            int16_t oldPrio = 1;
            if (context->ActionTree) {
                // TS1(sims): FreeSO — the caller's current priority (person data), not Queue[0]'s.
                auto ava = dynamic_cast<VMAvatar*>(context->Caller);
                oldPrio = (ava != nullptr) ? ava->GetPersonData(Model::VMPersonDataVariable::Priority) : (int16_t)1;
            }
            priority = std::max((int16_t)1, oldPrio); break;
        }
        case VMPushPriority::Maximum:
            priority = (int16_t)VMQueuePriority::Maximum; break;
        case VMPushPriority::Autonomous:
            priority = (int16_t)VMQueuePriority::Autonomous; break;
        case VMPushPriority::UserDriven:
            priority = (int16_t)VMQueuePriority::UserDriven; break;
        case VMPushPriority::ParentIdle:
            priority = (int16_t)VMQueuePriority::ParentIdle; mode = VMQueueMode::ParentIdle; break;
        case VMPushPriority::ParentExit:
            priority = (int16_t)VMQueuePriority::ParentExit; mode = VMQueueMode::ParentExit; break;
        case VMPushPriority::Idle:
            priority = (int16_t)VMQueuePriority::Idle; mode = VMQueueMode::Idle; break;
    }

    if (interactionSource == nullptr) return VMPrimitiveExitCode::GOTO_FALSE;
    auto action = interactionSource->GetAction(operand->Interaction, context->StackObject, context->VM()->Context);
    if (action == nullptr) return VMPrimitiveExitCode::GOTO_FALSE;
    if (operand->UseCustomIcon()) action->IconOwner(context->VM()->GetObjectById((int16_t)context->Locals.at(operand->IconLocation)));
    action->Mode = mode;
    action->Priority = priority;
    action->Flags |= TTABFlags::FSOSkipPermissions; // TS1(sims): FreeSO (FreeSims: MustRun, which is TS1AllowDogs in TS1 data)
    if (operand->PushTailContinuation()) action->Flags |= TTABFlags::FSOPushTail;
    if (operand->PushHeadContinuation()) action->Flags |= TTABFlags::FSOPushHead;

    // FIX(sims): flag 0x08 (unknown to FreeSO/FreeSims; set on 'exit person B' Wait pushes, 'Tell Everyone',
    // 'Get Group Excited', 'Push Sit', ...) is treated as "not if already queued": the same interaction of the same
    // object already waiting in the target's queue is not pushed again. Without it every Hot Date social left one more
    // 'Wait' in the townie's queue (21 after a date) and the townie stood still long after the player left.
    if ((operand->Flags & 8) != 0 && context->StackObject != nullptr && context->StackObject->Thread != nullptr) {
        auto thread = context->StackObject->Thread.get();
        for (size_t i = 0; i < thread->Queue.size(); i++) {
            auto& q = thread->Queue[i];
            if ((int)i > thread->ActiveQueueBlock && !q->Cancelled && q->Callee == action->Callee && q->InteractionNumber == action->InteractionNumber)
                return VMPrimitiveExitCode::GOTO_TRUE;
        }
    }
    if (context->StackObject != nullptr)
        context->StackObject->Thread->EnqueueAction(action);


    return VMPrimitiveExitCode::GOTO_TRUE;
}

void VMPushInteractionOperand::Read(const uint8_t* bytes) {
    st::Reader io(bytes, OPERAND_SIZE, st::Endian::Little);
    Interaction = io.u8();
    ObjectLocation = io.u8();
    Priority = (VMPushPriority)io.u8();
    Flags = io.u8();
    IconLocation = io.u8();
}

void VMPushInteractionOperand::Write(uint8_t* bytes) {
    st::Writer io(st::Endian::Little);
    io.WriteByte(Interaction);
    io.WriteByte(ObjectLocation);
    io.WriteByte((uint8_t)Priority);
    io.WriteByte(Flags);
    io.WriteByte(IconLocation);
    auto& a = io.ToArray();
    std::memcpy(bytes, a.data(), std::min(a.size(), OPERAND_SIZE));
}

}}}
