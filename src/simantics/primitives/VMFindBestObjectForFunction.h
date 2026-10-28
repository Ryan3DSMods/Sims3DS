// FSO.SimAntics.Engine.Primitives.VMFindBestObjectForFunction / VMFindBestObjectForFunctionOperand
// (primitives/VMFindBestObjectForFunction.cs)
/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
#pragma once
#include <cstdint>
#include <array>
#include <unordered_set>
#include "simantics/engine/VMPrimitiveHandler.h"
#include "simantics/model/VMStackObjectVariable.h"

namespace FSO { namespace SimAntics { namespace Primitives {

class VMFindBestObjectForFunction : public Engine::VMPrimitiveHandler {
public:
    static std::unordered_set<uint32_t> SurfaceFunctions;

    static const std::array<uint32_t, 15> FunctionToEntryPoint;

    static const std::array<Model::VMStackObjectVariable, 15> ScoreVar;

    Engine::VMPrimitiveExitCode Execute(Engine::VMStackFrame* context, Engine::VMPrimitiveOperand* args) override;
};

class VMFindBestObjectForFunctionOperand : public Engine::VMPrimitiveOperand {
public:
    uint16_t Function = 0;

    // VMPrimitiveOperand Members
    void Read(const uint8_t* bytes) override;
    void Write(uint8_t* bytes) override;
};

}}}
