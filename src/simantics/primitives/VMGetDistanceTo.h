// FSO.SimAntics.Primitives.VMGetDistanceTo / VMGetDistanceToOperand (primitives/VMGetDistanceTo.cs)
/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
#pragma once
#include <cstdint>
#include "simantics/engine/VMPrimitiveHandler.h"
#include "simantics/engine/scopes/VMVariableScope.h"

namespace FSO { namespace SimAntics { namespace Primitives {

class VMGetDistanceTo : public Engine::VMPrimitiveHandler {
public:
    Engine::VMPrimitiveExitCode Execute(Engine::VMStackFrame* context, Engine::VMPrimitiveOperand* args) override;
};

class VMGetDistanceToOperand : public Engine::VMPrimitiveOperand {
public:
    uint16_t TempNum = 0;
    uint8_t Flags = 0;
    Engine::Scopes::VMVariableScope ObjectScope = (Engine::Scopes::VMVariableScope)0;
    int16_t OScopeData = 0;

    // VMPrimitiveOperand Members
    void Read(const uint8_t* bytes) override;
    void Write(uint8_t* bytes) override;
};

}}}
