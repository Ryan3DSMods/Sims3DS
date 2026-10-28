/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// TS1(sims): The Sims 1 primitive 30 "Gosub Found Action" (Behavior.iff STR# 139 'Primitives').
// No FreeSims source: FreeSims registers its TSO "syslog" at opcode 30 and never registers TS1's
// "Find Best Interaction" (opcode 3) at all, so TS1 sims never chose anything autonomously.
//
// The Sims 1 person tree 'try autonomy' (PersonGlobals #8233) is:
//    Find Best Interaction (3) -> [false: return false] -> ... -> Gosub Found Action (30) -> true/false branch
// Find Best Interaction is the translated VMFindBestAction (queues the best scored interaction on the caller);
// Gosub Found Action then runs that queued interaction as an action tree *from the calling frame*, exactly like
// "Idle for Input (allow push)" hands control to a queued interaction. Now translated from FreeSO's
// VMGosubFoundAction: when the interaction's tree returns, the primitive runs again next tick (TS1 ignores the
// interaction's result, per FreeSO's notes on TS1 saves); with nothing queued it takes its false branch.
#pragma once
#include <cstdint>
#include "simantics/engine/VMPrimitiveHandler.h"
#include "simantics/engine/VMPrimitiveOperand.h"

namespace FSO { namespace SimAntics { namespace Primitives {

class VMGosubFoundAction : public Engine::VMPrimitiveHandler {
public:
    Engine::VMPrimitiveExitCode Execute(Engine::VMStackFrame* context, Engine::VMPrimitiveOperand* args) override;
};

class VMGosubFoundActionOperand : public Engine::VMPrimitiveOperand {
public:
    void Read(const uint8_t* bytes) override;
    void Write(uint8_t* bytes) override;
};

}}}
