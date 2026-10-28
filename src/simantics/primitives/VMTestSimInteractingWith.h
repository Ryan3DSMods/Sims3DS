/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Primitives.VMTestSimInteractingWith (primitives/VMTestSimInteractingWith.cs)
#pragma once
#include <cstdint>
#include "simantics/engine/VMPrimitiveHandler.h"

namespace FSO { namespace SimAntics { namespace Primitives {

class VMTestSimInteractingWith : public Engine::VMPrimitiveHandler
{
public:
    Engine::VMPrimitiveExitCode Execute(Engine::VMStackFrame* context, Engine::VMPrimitiveOperand* args) override;
};

class VMTestSimInteractingWithOperand : public Engine::VMPrimitiveOperand
{
public:
    void Read(const uint8_t* bytes) override;
    void Write(uint8_t* bytes) override;
};

}}}
