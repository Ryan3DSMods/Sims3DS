/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Primitives.VMChangeActionString
#include "simantics/engine/VMChangeActionString.h"
#include "simantics/engine/VMDialogHandler.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMThread.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/VMContext.h"
namespace FSO { namespace SimAntics { namespace Primitives {
using namespace FSO::SimAntics::Engine;
using FSO::Files::Formats::IFF::Chunks::STR;

VMPrimitiveExitCode VMChangeActionString::Execute(VMStackFrame* context, VMPrimitiveOperand* args) {
    auto operand = (VMChangeActionStringOperand*)args;
    STR* table = nullptr;
    switch (operand->Scope) {
        case 0:
            table = context->ScopeResource()->Get<STR>(operand->StringTable);
            break;
        case 1:
            table = (context->Callee->SemiGlobal == nullptr) ? nullptr : context->Callee->SemiGlobal->Get<STR>(operand->StringTable);
            break;
        case 2:
            table = context->Global()->Resource->Get<STR>(operand->StringTable);
            break;
    }

    if (table != nullptr) {
        std::string s;
        bool ok = table->TryGetString(operand->StringID - 1, STR::DefaultLangCode, s);
        auto newName = VMDialogHandler::ParseDialogString(context, s, table, ok);
        if (context->Thread->IsCheck && context->Thread->ActionStrings != nullptr) {
            VMPieMenuInteraction item;
            item.Name = newName;
            item.Param0 = context->StackObjectID(); // TS1(sims): FreeSO StackObjectID (may be a neighbour id)
            context->Thread->ActionStrings->push_back(item);
        } else if (context->Thread->ActiveAction() != nullptr)
            context->Thread->ActiveAction()->Name = newName; // TS1(sims): FreeSO ActiveAction (FreeSims: Queue[0])
    }
    return VMPrimitiveExitCode::GOTO_TRUE;
}
}}}
