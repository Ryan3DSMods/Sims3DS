/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims SimsVille/SimsAntics/primitives/VMTestObjectType.cs
#pragma once
#include <cstdint>
#include "simantics/engine/VMPrimitiveHandler.h"
#include "simantics/engine/VMPrimitiveOperand.h"
#include "simantics/engine/scopes/VMVariableScope.h"

namespace FSO { namespace SimAntics { namespace Primitives {

class VMTestObjectType : public Engine::VMPrimitiveHandler {
public:
    Engine::VMPrimitiveExitCode Execute(Engine::VMStackFrame* context, Engine::VMPrimitiveOperand* args) override;
};

class VMTestObjectTypeOperand : public Engine::VMPrimitiveOperand {
public:
    uint32_t GUID = 0;
    int16_t IdData = 0;
    Engine::Scopes::VMVariableScope IdOwner = (Engine::Scopes::VMVariableScope)0;

    // VMPrimitiveOperand Members
    void Read(const uint8_t* bytes) override;
    void Write(uint8_t* bytes) override;
};

}}}
