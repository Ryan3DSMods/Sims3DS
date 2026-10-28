/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims SimsVille/SimsAntics/primitives/VMSysLog.cs
#include "simantics/primitives/VMSysLog.h"
#include "simantics/engine/VMStackFrame.h"
#include "core/io.h"

namespace FSO { namespace SimAntics { namespace Primitives {

Engine::VMPrimitiveExitCode VMSysLog::Execute(Engine::VMStackFrame* /*context*/, Engine::VMPrimitiveOperand* /*args*/) {
    return Engine::VMPrimitiveExitCode::GOTO_TRUE;
}

void VMSysLogOperand::Read(const uint8_t* bytes) {
    st::Reader io(bytes, 8, st::Endian::Little);
    (void)io;
}

void VMSysLogOperand::Write(uint8_t* /*bytes*/) { }

}}}
