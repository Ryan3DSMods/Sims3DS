/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Engine.VMStackFrame
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMThread.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/VMContext.h"
namespace FSO { namespace SimAntics { namespace Engine {
FSO::Content::GameObjectResource* VMStackFrame::CallerPrivate() const { return Caller->Object->Resource; }
FSO::Content::GameObjectResource* VMStackFrame::CalleePrivate() const { return Callee->Object->Resource; }
FSO::Content::GameObjectResource* VMStackFrame::StackObjPrivate() const { return StackObject->Object->Resource; }
FSO::Content::GameGlobal* VMStackFrame::Global() const { return Thread->Context->Globals; }
FSO::SimAntics::VM* VMStackFrame::VM() const { return Thread->Context->VM; }
// TS1(sims): see StackObjectRawID.
int16_t VMStackFrame::StackObjectID() const { return (StackObject != nullptr) ? StackObject->ObjectID : StackObjectRawID; }
}}}
