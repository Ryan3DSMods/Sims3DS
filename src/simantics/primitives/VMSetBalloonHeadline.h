/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims SimsVille/SimsAntics/primitives/VMSetBalloonHeadline.cs
// VMSetBalloonHeadlineOperand and VMSetBalloonHeadlineOperandGroup are owned by the core
// (simantics/engine/VMSetBalloonHeadlineOperand.h) because VMRuntimeHeadline needs them.
#pragma once
#include <cstdint>
#include "simantics/engine/VMPrimitiveHandler.h"
#include "simantics/engine/VMSetBalloonHeadlineOperand.h"

namespace FSO { namespace SimAntics { namespace Primitives {

class VMSetBalloonHeadline : public Engine::VMPrimitiveHandler {
public:
    Engine::VMPrimitiveExitCode Execute(Engine::VMStackFrame* context, Engine::VMPrimitiveOperand* args) override;
};

}}}
