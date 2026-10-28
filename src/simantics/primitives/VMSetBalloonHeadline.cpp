/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims SimsVille/SimsAntics/primitives/VMSetBalloonHeadline.cs
#include "simantics/primitives/VMSetBalloonHeadline.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMThread.h"
#include "simantics/engine/VMSimanticsException.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/model/VMRuntimeHeadline.h"
#include "simantics/VM.h"

namespace FSO { namespace SimAntics { namespace Primitives {
using namespace FSO::SimAntics::Engine;
using namespace FSO::SimAntics::Model;

VMPrimitiveExitCode VMSetBalloonHeadline::Execute(VMStackFrame* context, VMPrimitiveOperand* args) {
    auto operand = static_cast<VMSetBalloonHeadlineOperand*>(args);
    auto obj = (operand->OfStackOBJ()) ? context->StackObject : context->Caller;
    // C# would throw a NullReferenceException here (caught by the thread).
    if (obj == nullptr) throw VMSimanticsException("Set Balloon Headline: null object reference.", context);

    if (obj->HeadlineRenderer != nullptr) obj->HeadlineRenderer->Dispose();

    if (operand->Index == -1 || operand->Duration == 0) {
        obj->Headline.reset();
        obj->HeadlineRenderer.reset();
    } else {
        VMEntity* icon = nullptr;
        int32_t index = operand->Index;
        if (operand->Group == VMSetBalloonHeadlineOperandGroup::Algorithmic)
            icon = (index < 2) ? context->StackObject : context->VM()->GetObjectById(context->Locals.at(operand->Algorithmic()));
        else if (operand->Indexed())
            index += context->Thread->TempRegisters[0];
        // C#: assigning the new headline before the renderer; the old renderer referenced the old
        // headline, so drop it first to keep the pointer it holds valid until it is gone.
        obj->HeadlineRenderer.reset();
        obj->Headline.reset(new VMRuntimeHeadline(*operand, obj, icon, (int8_t)index));
        obj->HeadlineRenderer.reset(context->VM()->Headline->Get(obj->Headline.get()));
    }
    return VMPrimitiveExitCode::GOTO_TRUE;
}

}}}
