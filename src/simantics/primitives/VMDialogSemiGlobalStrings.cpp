/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Primitives.VMDialogSemiGlobalStrings (primitives/VMDialogSemiGlobalStrings.cs)
#include "simantics/primitives/VMDialogSemiGlobalStrings.h"
#include "simantics/primitives/VMDialogPrivateStrings.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMSimanticsException.h"
#include "simantics/entities/VMEntity.h"
#include "content/ContentManager.h"

namespace FSO { namespace SimAntics { namespace Primitives {

using namespace FSO::SimAntics::Engine;
using FSO::Files::Formats::IFF::Chunks::STR;

VMPrimitiveExitCode VMDialogSemiGlobalStrings::Execute(VMStackFrame* context, VMPrimitiveOperand* args)
{
    // C# would throw a NullReferenceException for a callee without a semiglobal (caught by the thread).
    if (context->Callee == nullptr || context->Callee->SemiGlobal == nullptr)
        throw VMSimanticsException("Object reference not set to an instance of an object.", context);
    return VMDialogPrivateStrings::ExecuteGeneric(context, args, context->Callee->SemiGlobal->Get<STR>(301));
}

}}}
