/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Engine.Primitives.VMRandomNumber (primitives/VMRandomNumber.cs)
// Note: the C# file declares these in namespace FSO.SimAntics.Engine.Primitives; per the primitives brief all
// primitives live in FSO::SimAntics::Primitives here.
#pragma once
#include <cstdint>
#include "simantics/engine/VMPrimitiveHandler.h"
#include "simantics/engine/scopes/VMVariableScope.h"

namespace FSO { namespace SimAntics { namespace Primitives {

class VMRandomNumber : public Engine::VMPrimitiveHandler {
public:
    Engine::VMPrimitiveExitCode Execute(Engine::VMStackFrame* context, Engine::VMPrimitiveOperand* args) override;
};

class VMRandomNumberOperand : public Engine::VMPrimitiveOperand
{
public:
    int16_t DestinationData = 0;
    Engine::Scopes::VMVariableScope DestinationScope = (Engine::Scopes::VMVariableScope)0;
    int16_t RangeData = 0;
    Engine::Scopes::VMVariableScope RangeScope = (Engine::Scopes::VMVariableScope)0;

    void Read(const uint8_t* bytes) override;
    void Write(uint8_t* bytes) override;
};

}}}
