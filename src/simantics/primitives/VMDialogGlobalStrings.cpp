/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Primitives.VMDialogGlobalStrings (primitives/VMDialogGlobalStrings.cs)
#include "simantics/primitives/VMDialogGlobalStrings.h"
#include "simantics/primitives/VMDialogPrivateStrings.h"
#include "simantics/engine/VMStackFrame.h"
#include "content/ContentManager.h"

namespace FSO { namespace SimAntics { namespace Primitives {

using namespace FSO::SimAntics::Engine;
using FSO::Files::Formats::IFF::Chunks::STR;

VMPrimitiveExitCode VMDialogGlobalStrings::Execute(VMStackFrame* context, VMPrimitiveOperand* args)
{
    return VMDialogPrivateStrings::ExecuteGeneric(context, args, context->Global()->Resource->Get<STR>(301));
}

}}}
