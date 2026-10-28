/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Engine.VMQueuedAction
#include "simantics/engine/VMQueuedAction.h"
namespace FSO { namespace SimAntics { namespace Engine {
std::shared_ptr<VMStackFrame> VMQueuedAction::ToStackFrame(VMEntity* caller) {
    auto frame = std::make_shared<VMStackFrame>();
    frame->Caller = caller;
    frame->Callee = Callee;
    frame->CodeOwner = CodeOwner;
    frame->Routine = ActionRoutine;
    frame->StackObject = StackObject;
    frame->ActionTree = true;
    if (Args.empty()) frame->Args.assign(4, 0); //always 4? i got crashes when i used the value provided by the routine, when for that same routine edith displayed 4 in the properties...
    else frame->Args = Args; //WARNING - if you use this, the args array MUST have the same number of elements the routine is expecting!
    return frame;
}
}}}
