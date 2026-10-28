/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// TS1(sims): primitives FreeSims never registered (they ran as "true" and did nothing):
//   9  Burn                  - FreeSO VMBurn (stoves, fireplaces, candles, magic: starts / spreads fires)
//   21 Show String           - FreeSO VMShowString (a message balloon over a Sim; AnimTester debug object)
//   33 Find 5 Worst Motives  - the visitor's "I'm going home because..." (PedPortal 'show leave dialog', Hot Date
//                              nagging): Temp 0..4 = the caller's five lowest of energy, comfort, hunger, hygiene,
//                              bladder, room, social, fun (motive numbers 5..9, 13..15), worst first.
#pragma once
#include <cstdint>
#include "simantics/engine/VMPrimitiveHandler.h"
#include "simantics/engine/VMPrimitiveOperand.h"

namespace FSO { namespace SimAntics { namespace Primitives {

class VMBurn : public Engine::VMPrimitiveHandler {
public:
    static const uint32_t FIRE_GUID = 0x24C95F99; // Objects.far Fire.iff
    Engine::VMPrimitiveExitCode Execute(Engine::VMStackFrame* context, Engine::VMPrimitiveOperand* args) override;
};
enum class VMBurnType : uint8_t { StackObject = 0, FrontOfStackObject = 1, FloorsUnderStackObject = 2 };
class VMBurnOperand : public Engine::VMPrimitiveOperand {
public:
    VMBurnType Type = VMBurnType::StackObject;
    bool BurnBusyObjects = false;
    void Read(const uint8_t* bytes) override { Type = (VMBurnType)bytes[0]; BurnBusyObjects = bytes[1] > 0; }
    void Write(uint8_t* bytes) override { bytes[0] = (uint8_t)Type; bytes[1] = BurnBusyObjects ? 1 : 0; }
};

class VMShowString : public Engine::VMPrimitiveHandler {
public:
    Engine::VMPrimitiveExitCode Execute(Engine::VMStackFrame* context, Engine::VMPrimitiveOperand* args) override;
};
class VMShowStringOperand : public Engine::VMPrimitiveOperand {
public:
    uint16_t StringTable = 300;
    uint16_t StringID = 0;
    uint8_t Flags = 0;
    void Read(const uint8_t* b) override { StringTable = (uint16_t)(b[0] | (b[1] << 8)); StringID = (uint16_t)(b[2] | (b[3] << 8)); Flags = b[4]; }
    void Write(uint8_t* b) override { b[0] = (uint8_t)StringTable; b[1] = (uint8_t)(StringTable >> 8); b[2] = (uint8_t)StringID; b[3] = (uint8_t)(StringID >> 8); b[4] = Flags; }
};

class VMFind5WorstMotives : public Engine::VMPrimitiveHandler {
public:
    Engine::VMPrimitiveExitCode Execute(Engine::VMStackFrame* context, Engine::VMPrimitiveOperand* args) override;
};
class VMNoOperand : public Engine::VMPrimitiveOperand {
public:
    void Read(const uint8_t*) override {}
    void Write(uint8_t*) override {}
};

}}}
