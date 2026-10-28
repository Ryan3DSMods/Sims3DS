/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Primitives.VMSetBalloonHeadlineOperand / VMSetBalloonHeadlineOperandGroup
// (primitives/VMSetBalloonHeadline.cs). Owned by the core because VMRuntimeHeadline needs it;
// primitives/VMSetBalloonHeadline.* must include this.
#pragma once
#include <cstdint>
#include "simantics/engine/VMPrimitiveOperand.h"
namespace FSO { namespace SimAntics { namespace Primitives {
enum class VMSetBalloonHeadlineOperandGroup : int8_t {
    OldStyle = 0, Balloon = 1, Conversation = 2, Motive = 3, Relationship = 4, Headline = 5, Debug = 6,
    Algorithmic = 7, RouteFailure = 8, Progress = 9,
};
class VMSetBalloonHeadlineOperand : public Engine::VMPrimitiveOperand {
public:
    uint16_t Flags2 = 0;
    int8_t Index = 0;
    VMSetBalloonHeadlineOperandGroup Group = VMSetBalloonHeadlineOperandGroup::OldStyle;
    int16_t Duration = 0;
    uint8_t Type = 0;
    uint8_t Flags = 0;

    int32_t Algorithmic() const { return Flags2 >> 1; }
    bool OfStackOBJ() const { return (Flags2 & 0x1) > 0; }
    bool Inactive() const { return (Flags & 0x1) > 0; }
    bool Crossed() const { return (Flags & 0x2) > 0; }
    bool Backwards() const { return (Flags & 0x4) > 0; }
    bool DurationInLoops() const { return (Flags & 0x8) > 0; }
    bool Indexed() const { return (Flags & 0x10) > 0; }

    void Read(const uint8_t* bytes) override {
        st::Reader io(bytes, 8, st::Endian::Little);
        Flags2 = io.u16();
        Index = (int8_t)io.u8();
        Group = (VMSetBalloonHeadlineOperandGroup)(int8_t)io.u8();
        Duration = io.s16();
        Type = io.u8();
        Flags = io.u8();
    }
    void Write(uint8_t* bytes) override {
        bytes[0] = (uint8_t)(Flags2 & 0xFF); bytes[1] = (uint8_t)(Flags2 >> 8);
        bytes[2] = (uint8_t)Index; bytes[3] = (uint8_t)Group;
        bytes[4] = (uint8_t)(Duration & 0xFF); bytes[5] = (uint8_t)((Duration >> 8) & 0xFF);
        bytes[6] = Type; bytes[7] = Flags;
    }
};
}}}
