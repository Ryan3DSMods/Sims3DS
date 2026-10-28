// FSO.SimAntics.Primitives.VMSpecialEffect / VMSpecialEffectOperand (primitives/VMSpecialEffect.cs)
/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
#pragma once
#include <cstdint>
#include "simantics/engine/VMPrimitiveHandler.h"

namespace FSO { namespace SimAntics { namespace Primitives {

class VMSpecialEffect : public Engine::VMPrimitiveHandler {
public:
    Engine::VMPrimitiveExitCode Execute(Engine::VMStackFrame* context, Engine::VMPrimitiveOperand* args) override;
};

class VMSpecialEffectOperand : public Engine::VMPrimitiveOperand {
public:
    uint16_t Timeout = 0;
    int8_t Size = 0;
    int8_t Zoom = 0;
    int8_t Flags = 0;
    int8_t StrIndex = 0;

    // VMPrimitiveOperand Members
    void Read(const uint8_t* bytes) override;
    void Write(uint8_t* bytes) override;
};

}}}
