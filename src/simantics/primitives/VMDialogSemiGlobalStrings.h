/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Primitives.VMDialogSemiGlobalStrings (primitives/VMDialogSemiGlobalStrings.cs)
// Operand: VMDialogOperand (core, simantics/engine/VMDialogOperand.h).
#pragma once
#include "simantics/engine/VMPrimitiveHandler.h"
#include "simantics/engine/VMDialogOperand.h"

namespace FSO { namespace SimAntics { namespace Primitives {

class VMDialogSemiGlobalStrings : public Engine::VMPrimitiveHandler
{
public:
    Engine::VMPrimitiveExitCode Execute(Engine::VMStackFrame* context, Engine::VMPrimitiveOperand* args) override;
};

}}}
