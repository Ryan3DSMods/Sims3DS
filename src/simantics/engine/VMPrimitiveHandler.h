/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Engine.VMPrimitiveHandler
#pragma once
#include <cstdio>
#include <string>
#include "simantics/engine/VMPrimitiveExitCode.h"
#include "simantics/engine/VMPrimitiveOperand.h"
namespace FSO { namespace SimAntics { namespace Engine {
class VMStackFrame;
class VMPrimitiveHandler {
public:
    virtual ~VMPrimitiveHandler() {}
    virtual VMPrimitiveExitCode Execute(VMStackFrame* context, VMPrimitiveOperand* operand) = 0;
protected:
    void Trace(const std::string& message) { fprintf(stderr, "%s\n", message.c_str()); }
};
}}}
