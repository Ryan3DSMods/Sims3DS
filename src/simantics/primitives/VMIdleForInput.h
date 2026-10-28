/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Primitives.VMIdleForInput (primitives/VMIdleForInput.cs)
#pragma once
#include <cstdint>
#include "simantics/engine/VMPrimitiveHandler.h"

namespace FSO { namespace SimAntics { namespace Primitives {

class VMIdleForInput : public Engine::VMPrimitiveHandler
{
public:
    //TODO: Behaviour for being notified out of idle and interaction canceling
    Engine::VMPrimitiveExitCode Execute(Engine::VMStackFrame* context, Engine::VMPrimitiveOperand* args) override;
};

class VMIdleForInputOperand : public Engine::VMPrimitiveOperand
{
public:
    int16_t StackVarToDec = 0;
    uint16_t AllowPush = 0;

    void Read(const uint8_t* bytes) override;
    void Write(uint8_t* bytes) override;
};

}}}
