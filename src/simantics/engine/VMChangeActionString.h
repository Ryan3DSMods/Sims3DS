/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Primitives.VMChangeActionString (engine/VMChangeActionString.cs) — primitive 50.
#pragma once
#include "simantics/engine/VMPrimitiveHandler.h"
namespace FSO { namespace SimAntics { namespace Primitives {
class VMChangeActionString : public Engine::VMPrimitiveHandler {
public:
    Engine::VMPrimitiveExitCode Execute(Engine::VMStackFrame* context, Engine::VMPrimitiveOperand* args) override;
};
class VMChangeActionStringOperand : public Engine::VMPrimitiveOperand {
public:
    uint16_t StringTable = 0;
    uint16_t Scope = 0;
    uint8_t StringID = 0;
    void Read(const uint8_t* bytes) override {
        st::Reader io(bytes, 8, st::Endian::Little);
        StringTable = io.u16();
        Scope = io.u16();
        StringID = io.u8();
    }
    void Write(uint8_t* bytes) override {
        bytes[0] = (uint8_t)(StringTable & 0xFF); bytes[1] = (uint8_t)(StringTable >> 8);
        bytes[2] = (uint8_t)(Scope & 0xFF); bytes[3] = (uint8_t)(Scope >> 8);
        bytes[4] = StringID;
    }
};
}}}
