/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims SimsVille/SimsAntics/primitives/VMSysLog.cs
#pragma once
#include <cstdint>
#include "simantics/engine/VMPrimitiveHandler.h"
#include "simantics/engine/VMPrimitiveOperand.h"

namespace FSO { namespace SimAntics { namespace Primitives {

// Logs a string to the console. Might make this functional again when we implement object development.

class VMSysLog : public Engine::VMPrimitiveHandler {
public:
    Engine::VMPrimitiveExitCode Execute(Engine::VMStackFrame* context, Engine::VMPrimitiveOperand* args) override;
};

class VMSysLogOperand : public Engine::VMPrimitiveOperand {
public:
    // VMPrimitiveOperand Members
    void Read(const uint8_t* bytes) override;
    void Write(uint8_t* bytes) override;
};

}}}
