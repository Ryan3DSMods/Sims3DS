/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Engine.Primitives.VMRemoveObjectInstance / VMRemoveObjectInstanceOperand (primitives/VMRemoveObjectInstance.cs)
// NOTE: C# declares these in FSO.SimAntics.Engine.Primitives; the port keeps all primitives in FSO::SimAntics::Primitives.
#pragma once
#include <cstdint>
#include "simantics/engine/VMPrimitiveHandler.h"
#include "simantics/engine/VMPrimitiveOperand.h"

namespace FSO { namespace SimAntics { namespace Primitives {

class VMRemoveObjectInstance : public Engine::VMPrimitiveHandler {
public:
    Engine::VMPrimitiveExitCode Execute(Engine::VMStackFrame* context, Engine::VMPrimitiveOperand* args) override;
};

class VMRemoveObjectInstanceOperand : public Engine::VMPrimitiveOperand {
public:
    int16_t Target = 0;
    uint8_t Flags = 0;

    // #region VMPrimitiveOperand Members
    void Read(const uint8_t* bytes) override;
    void Write(uint8_t* bytes) override;
    // #endregion

    bool ReturnImmediately() const
    {
        return ((Flags & 1) == 1);
    }
    void ReturnImmediately(bool value)
    {
        if (value) Flags |= 1;
        else Flags &= (uint8_t)~1;
    }

    bool CleanupAll() const
    {
        return ((Flags & 2) == 2);
    }
    void CleanupAll(bool value)
    {
        if (value) Flags |= 2;
        else Flags &= (uint8_t)~2;
    }
};

}}}
