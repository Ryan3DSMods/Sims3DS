/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims SimsVille/SimsAntics/primitives/VMPlaySound.cs
#pragma once
#include <cstdint>
#include "simantics/engine/VMPrimitiveHandler.h"
#include "simantics/engine/VMPrimitiveOperand.h"

namespace FSO { namespace SimAntics { namespace Primitives {

class VMPlaySound : public Engine::VMPrimitiveHandler {
public:
    Engine::VMPrimitiveExitCode Execute(Engine::VMStackFrame* context, Engine::VMPrimitiveOperand* args) override;
};

class VMPlaySoundOperand : public Engine::VMPrimitiveOperand {
public:
    uint16_t EventID = 0;
    uint16_t Pad = 0;
    uint8_t Flags = 0;
    uint8_t Volume = 0;

    // VMPrimitiveOperand Members
    void Read(const uint8_t* bytes) override;
    void Write(uint8_t* bytes) override;

    bool NoPan() const { return (Flags & 8) == 8; }
    bool NoZoom() const { return (Flags & 4) == 4; }
    bool Loop() const { return (Flags & 1) == 1; }
    bool StackObjAsSource() const { return (Flags & 2) == 2; }
};

}}}
