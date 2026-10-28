/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Primitives.VMDialogPrivateStrings (primitives/VMDialogPrivateStrings.cs)
// VMDialogOperand / VMDialogFlags / VMDialogType / VMDialogResult from the same C# file are owned by the core
// (simantics/engine/VMDialogOperand.h).
#pragma once
#include <cstdint>
#include "simantics/_files_contract.h"
#include "simantics/engine/VMPrimitiveHandler.h"
#include "simantics/engine/VMDialogOperand.h"

namespace FSO { namespace SimAntics { namespace Primitives {

class VMDialogPrivateStrings : public Engine::VMPrimitiveHandler
{
public:
    static const int32_t DIALOG_MAX_WAITTIME;

    Engine::VMPrimitiveExitCode Execute(Engine::VMStackFrame* context, Engine::VMPrimitiveOperand* args) override;

    static Engine::VMPrimitiveExitCode ExecuteGeneric(Engine::VMStackFrame* context, Engine::VMPrimitiveOperand* args,
                                                      FSO::Files::Formats::IFF::Chunks::STR* table);
};

}}}
