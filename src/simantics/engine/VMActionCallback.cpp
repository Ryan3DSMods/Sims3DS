/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Engine.VMActionCallback
#include "simantics/engine/VMActionCallback.h"
#include "simantics/engine/VMQueuedAction.h"
#include "simantics/engine/VMThread.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/VM.h"
namespace FSO { namespace SimAntics { namespace Engine {
using namespace FSO::Files::Formats::IFF::Chunks;

VMActionCallback::VMActionCallback(FSO::SimAntics::VM* vm_, int16_t interactionNumber, VMEntity* target, VMEntity* stackObj, VMEntity* caller, bool paramAsObjectID, bool isTree) {
    this->type = 1;
    this->Target = target;
    this->Interaction = interactionNumber;
    this->SetParam = paramAsObjectID;
    this->StackObject = stackObj;
    this->vm = vm_;
    this->Caller = caller;
    this->IsTree = isTree;
}

void VMActionCallback::Run(VMEntity* cbOwner) {
    if (type == 1) {
        BHAV* bhav;
        FSO::Content::GameObject* CodeOwner = nullptr;
        uint16_t ActionID;
        TTABFlags ActionFlags;
        std::string ActionName = "";
        if (IsTree) {
            ActionFlags = TTABFlags::Leapfrog;
            ActionID = (uint16_t)Interaction;
        } else {
            auto it = Target->TreeTable->InteractionByIndex.find((uint8_t)Interaction);
            if (it == Target->TreeTable->InteractionByIndex.end()) return;
            auto Action = it->second;
            ActionID = Action->ActionFunction;
            ActionFlags = Action->Flags;
            ActionName = Target->TreeTableStrings->GetString((int32_t)Action->TTAIndex);
        }

        if (ActionID < 4096) { //global
            bhav = nullptr;
            //unimp as it has to access the context to get this.
        } else if (ActionID < 8192) { //local
            bhav = Target->Object->Resource->Get<BHAV>(ActionID);
        } else { //semi-global
            bhav = (Target->SemiGlobal == nullptr) ? nullptr : Target->SemiGlobal->Get<BHAV>(ActionID);
        }
        if (bhav == nullptr) return; //???
        if (IsTree) ActionName = bhav->ChunkLabel;

        CodeOwner = Target->Object;
        auto routine = vm->Assemble(bhav);
        std::vector<int16_t> args(4, 0);
        if (SetParam) args[0] = cbOwner->ObjectID;

        auto action = std::make_shared<VMQueuedAction>();
        action->Callee = Target;
        action->CodeOwner = CodeOwner;
        action->ActionRoutine = routine;
        action->Name = ActionName;
        action->StackObject = this->StackObject;
        action->Args = args;
        action->InteractionNumber = Interaction;
        action->Priority = (int16_t)VMQueuePriority::Maximum; //not sure if this is meant to be the case!
        action->Flags = ActionFlags;
        Caller->Thread->EnqueueAction(action);
    }
}
}}}
