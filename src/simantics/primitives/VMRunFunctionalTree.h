/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims SimsVille/SimsAntics/primitives/VMRunFunctionalTree.cs
// (C# namespace FSO.SimAntics.Engine.Primitives; placed in FSO::SimAntics::Primitives like the other primitives.)
#pragma once
#include <cstdint>
#include "simantics/engine/VMPrimitiveHandler.h"

namespace FSO { namespace SimAntics { namespace Primitives {

//See VMFindBestObjectForFunction for function map table.

class VMRunFunctionalTree : public Engine::VMPrimitiveHandler {
public:
    Engine::VMPrimitiveExitCode Execute(Engine::VMStackFrame* context, Engine::VMPrimitiveOperand* args) override;
};

class VMRunFunctionalTreeOperand : public Engine::VMPrimitiveOperand {
public:
    uint16_t Function = 0;
    uint8_t Flags = 0; //only flag is 1: change icon

    bool ChangeIcon() const { return (Flags & 1) > 0; }
    // C# setter (sic): `Flags = (byte)((Flags & 0xFE) | (ChangeIcon ? 1 : 0));` — reads the getter, not `value`.
    void ChangeIcon(bool value) { (void)value; Flags = (uint8_t)((Flags & 0xFE) | (ChangeIcon() ? 1 : 0)); }

    // region VMPrimitiveOperand Members
    void Read(const uint8_t* bytes) override;
    void Write(uint8_t* bytes) override;
    // endregion
};

}}}
