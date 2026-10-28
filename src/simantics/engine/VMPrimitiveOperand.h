/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Engine.VMPrimitiveOperand
// Operands are decoded from the 8 operand bytes of a BHAV instruction. Read/Write always get a
// pointer to exactly 8 bytes (BHAVInstruction::Operand).
#pragma once
#include <cstdint>
#include "core/io.h"
namespace FSO { namespace SimAntics { namespace Engine {
class VMPrimitiveOperand {
public:
    virtual ~VMPrimitiveOperand() {}
    virtual void Read(const uint8_t* bytes) = 0;
    virtual void Write(uint8_t* bytes) = 0;
    static constexpr size_t OPERAND_SIZE = 8;
};
}}}
