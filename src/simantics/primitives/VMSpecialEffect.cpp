// FSO.SimAntics.Primitives.VMSpecialEffect (primitives/VMSpecialEffect.cs)
/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
#include "simantics/primitives/VMSpecialEffect.h"
#include <cstring>
#include <algorithm>
#include "core/io.h"
#include "simantics/engine/VMStackFrame.h"

namespace FSO { namespace SimAntics { namespace Primitives {
using namespace FSO::SimAntics::Engine;

VMPrimitiveExitCode VMSpecialEffect::Execute(VMStackFrame* context, VMPrimitiveOperand* args) {
    auto operand = static_cast<VMSpecialEffectOperand*>(args);
    (void)operand;
    //TODO: Implement this, fridge has one when u have no money
    return VMPrimitiveExitCode::GOTO_TRUE;
}

void VMSpecialEffectOperand::Read(const uint8_t* bytes) {
    st::Reader io(bytes, OPERAND_SIZE, st::Endian::Little);
    Timeout = io.u16();
    Size = (int8_t)io.u8();
    Zoom = (int8_t)io.u8();
    Flags = (int8_t)io.u8();
    StrIndex = (int8_t)io.u8();
}

void VMSpecialEffectOperand::Write(uint8_t* bytes) {
    st::Writer io(st::Endian::Little);
    io.WriteUInt16(Timeout);
    io.WriteByte((uint8_t)Size);
    io.WriteByte((uint8_t)Zoom);
    io.WriteByte((uint8_t)Flags);
    io.WriteByte((uint8_t)StrIndex);
    auto& a = io.ToArray();
    std::memcpy(bytes, a.data(), std::min(a.size(), OPERAND_SIZE));
}

}}}
