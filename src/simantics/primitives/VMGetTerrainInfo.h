/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Primitives.VMGetTerrainInfo / VMGetTerrainInfoOperand (primitives/VMGetTerrainInfo.cs)
#pragma once
#include <cstdint>
#include <vector>
#include "simantics/engine/VMPrimitiveHandler.h"

namespace FSO { namespace SimAntics { namespace Primitives {

class VMGetTerrainInfo : public Engine::VMPrimitiveHandler {
public:
    Engine::VMPrimitiveExitCode Execute(Engine::VMStackFrame* context, Engine::VMPrimitiveOperand* args) override;
};

class VMGetTerrainInfoOperand : public Engine::VMPrimitiveOperand {
public:
    uint8_t Mode = 0;
    uint8_t Unknown1 = 0;
    uint8_t Flags = 0;
    std::vector<uint8_t> Unknown; // C# byte[] (null until Read; then 5 bytes)

    bool Interpolate() const { return (Flags & 1) > 0; }
    bool UseMe() const { return (Flags & 2) > 0; }

    // VMPrimitiveOperand Members
    void Read(const uint8_t* bytes) override;
    void Write(uint8_t* bytes) override;
};

}}}
