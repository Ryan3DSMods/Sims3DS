/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Primitives.VMDropOnto (primitives/VMDropOnto.cs)
#pragma once
#include <cstdint>
#include "simantics/engine/VMPrimitiveHandler.h"

namespace FSO { namespace SimAntics { namespace Primitives {

class VMDropOnto : public Engine::VMPrimitiveHandler
{
public:
    Engine::VMPrimitiveExitCode Execute(Engine::VMStackFrame* context, Engine::VMPrimitiveOperand* args) override;
};

class VMDropOntoOperand : public Engine::VMPrimitiveOperand
{
public:
    uint16_t SrcSlotMode = 0;
    uint16_t SrcSlotNum = 0;
    uint16_t DestSlotMode = 0;
    uint16_t DestSlotNum = 0;

    void Read(const uint8_t* bytes) override;
    void Write(uint8_t* bytes) override;
};

}}}
