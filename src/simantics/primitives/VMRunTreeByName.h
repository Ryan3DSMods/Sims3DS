/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Engine.Primitives.VMRunTreeByName / VMRunTreeByNameOperand (primitives/VMRunTreeByName.cs)
// NOTE: C# declares these in FSO.SimAntics.Engine.Primitives; the port keeps all primitives in FSO::SimAntics::Primitives.
#pragma once
#include <cstdint>
#include "simantics/engine/VMPrimitiveHandler.h"
#include "simantics/engine/VMPrimitiveOperand.h"

namespace FSO { namespace SimAntics { namespace Primitives {

class VMRunTreeByName : public Engine::VMPrimitiveHandler {
public:
    Engine::VMPrimitiveExitCode Execute(Engine::VMStackFrame* context, Engine::VMPrimitiveOperand* args) override;
};

class VMRunTreeByNameOperand : public Engine::VMPrimitiveOperand {
public:
    uint16_t StringTable = 0;
    uint8_t StringScope = 0;
    uint8_t Unused = 0;
    uint8_t StringID = 0;
    uint8_t Destination = 0;

    void Read(const uint8_t* bytes) override;
    void Write(uint8_t* bytes) override;
};

}}}
