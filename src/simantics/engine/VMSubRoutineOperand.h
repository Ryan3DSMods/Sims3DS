/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Primitives.VMSubRoutineOperand (primitives/VMSubRoutine.cs)
// Owned by the core because VMThread/VMTranslator need it. primitives/VMSubRoutine.* must include this.
#pragma once
#include <cstdint>
#include <array>
#include "simantics/engine/VMPrimitiveOperand.h"
namespace FSO { namespace SimAntics { namespace Primitives {
/// There isnt actually a private call handler, This is part of the thread.
class VMSubRoutineOperand : public Engine::VMPrimitiveOperand {
public:
    VMSubRoutineOperand() { Arguments.fill(0); }
    explicit VMSubRoutineOperand(const int16_t args[4]) { for (int i = 0; i < 4; i++) Arguments[i] = args[i]; }
    VMSubRoutineOperand(int16_t a0, int16_t a1, int16_t a2, int16_t a3) { Arguments = {a0, a1, a2, a3}; }
    std::array<int16_t, 4> Arguments;
    int16_t Arg0() const { return Arguments[0]; }  void Arg0(int16_t v) { Arguments[0] = v; }
    int16_t Arg1() const { return Arguments[1]; }  void Arg1(int16_t v) { Arguments[1] = v; }
    int16_t Arg2() const { return Arguments[2]; }  void Arg2(int16_t v) { Arguments[2] = v; }
    int16_t Arg3() const { return Arguments[3]; }  void Arg3(int16_t v) { Arguments[3] = v; }
    /** TS1(sims): FreeSO UseTemp0 — an argument of -1 means "pass the temp" only when the call is not
     ** (x, 0, 0, 0); a lone -1 in arg 0 is the literal -1. **/
    bool UseTemp0() const { return !(Arguments[1] == 0 && Arguments[2] == 0 && Arguments[3] == 0); }

    void Read(const uint8_t* bytes) override {
        st::Reader io(bytes, 8, st::Endian::Little);
        Arguments[0] = io.s16();
        Arguments[1] = io.s16();
        Arguments[2] = io.s16();
        Arguments[3] = io.s16();
    }
    void Write(uint8_t* bytes) override {
        for (int i = 0; i < 4; i++) { bytes[i * 2] = (uint8_t)(Arguments[i] & 0xFF); bytes[i * 2 + 1] = (uint8_t)((Arguments[i] >> 8) & 0xFF); }
    }
};
}}}
