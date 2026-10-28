// FSO.SimAntics.Engine.Primitives.VMGotoRoutingSlot / VMGotoRoutingSlotOperand (primitives/VMGotoRoutingSlot.cs)
/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
#pragma once
#include <cstdint>
#include <string>
#include "simantics/engine/VMPrimitiveHandler.h"
#include "simantics/engine/scopes/VMSlotScope.h"

namespace FSO { namespace SimAntics { namespace Primitives {

class VMGotoRoutingSlot : public Engine::VMPrimitiveHandler {
public:
    Engine::VMPrimitiveExitCode Execute(Engine::VMStackFrame* context, Engine::VMPrimitiveOperand* args) override;
};

class VMGotoRoutingSlotOperand : public Engine::VMPrimitiveOperand {
public:
    uint16_t Data = 0;
    Engine::Scopes::VMSlotScope Type = Engine::Scopes::VMSlotScope::StackVariable;
    uint8_t Flags = 0;

    bool NoFailureTrees() const { return (Flags & 1) > 0; }
    void NoFailureTrees(bool value) {
        if (value) Flags |= 1;
        else Flags &= 254;
    }

    // VMPrimitiveOperand Members
    void Read(const uint8_t* bytes) override;
    void Write(uint8_t* bytes) override;

    std::string ToString() const;
};

}}}
