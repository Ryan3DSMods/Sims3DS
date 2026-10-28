// FSO.SimAntics.Engine.Primitives.VMCreateObjectInstance / VMCreateObjectInstanceOperand / VMCreateObjectPosition
// (primitives/VMCreateObjectInstance.cs)
/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
#pragma once
#include <cstdint>
#include "simantics/engine/VMPrimitiveHandler.h"

namespace FSO { namespace SimAntics { namespace Primitives {

class VMCreateObjectInstance : public Engine::VMPrimitiveHandler {
public:
    Engine::VMPrimitiveExitCode Execute(Engine::VMStackFrame* context, Engine::VMPrimitiveOperand* args) override;
};

enum class VMCreateObjectPosition : int32_t {
    InFrontOfMe = 0,
    OnTopOfMe = 1,
    InMyHand = 2,
    InFrontOfStackObject = 3,
    InSlot0OfStackObject = 4,
    UnderneathMe = 5,
    OutOfWorld = 6,
    BelowObjectInStackParam0 = 7,
    BelowObjectInLocal = 8,
    NextToMeInDirectionOfLocal = 9
};

class VMCreateObjectInstanceOperand : public Engine::VMPrimitiveOperand {
public:
    uint32_t GUID = 0;
    VMCreateObjectPosition Position = VMCreateObjectPosition::InFrontOfMe;
    uint8_t Flags = 0;
    uint8_t LocalToUse = 0;
    uint8_t InteractionCallback = 0;

    void Read(const uint8_t* bytes) override;
    void Write(uint8_t* bytes) override;

    bool PassObjectIds() const { return (Flags & 2) == 2; }
    void PassObjectIds(bool value) {
        if (value) Flags |= 2;
        else Flags &= (uint8_t)~2;
    }

    bool PassTemp0() const { return (Flags & 16) == 16; }
    void PassTemp0(bool value) {
        if (value) Flags |= 16;
        else Flags &= (uint8_t)~16;
    }
};

}}}
