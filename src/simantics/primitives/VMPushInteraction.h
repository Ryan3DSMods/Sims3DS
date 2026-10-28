// FSO.SimAntics.Engine.Primitives.VMPushInteraction / VMPushInteractionOperand / VMPushPriority
// (primitives/VMPushInteraction.cs)
/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
#pragma once
#include <cstdint>
#include "simantics/engine/VMPrimitiveHandler.h"

namespace FSO { namespace SimAntics { namespace Primitives {

class VMPushInteraction : public Engine::VMPrimitiveHandler {
public:
    Engine::VMPrimitiveExitCode Execute(Engine::VMStackFrame* context, Engine::VMPrimitiveOperand* args) override;
};

enum class VMPushPriority : uint8_t {
    Inherited = 0,
    Maximum = 1,
    Autonomous = 2,
    UserDriven = 3,
    ParentIdle = 4,
    ParentExit = 5,
    Idle = 6
};

class VMPushInteractionOperand : public Engine::VMPrimitiveOperand {
public:
    uint8_t Interaction = 0;
    uint8_t ObjectLocation = 0;
    VMPushPriority Priority = VMPushPriority::Inherited;
    uint8_t Flags = 0;
    uint8_t IconLocation = 0;

    bool UseCustomIcon() const { return (Flags & 1) > 0; }

    bool PushHeadContinuation() const { return (Flags & 4) > 0; }
    void PushHeadContinuation(bool value) {
        if (value) Flags |= 4;
        else Flags &= (uint8_t)~4;
    }

    bool PushTailContinuation() const { return (Flags & 128) > 0; }
    void PushTailContinuation(bool value) {
        if (value) Flags |= 128;
        else Flags &= (uint8_t)~128;
    }

    // VMPrimitiveOperand Members
    void Read(const uint8_t* bytes) override;
    void Write(uint8_t* bytes) override;
};

}}}
