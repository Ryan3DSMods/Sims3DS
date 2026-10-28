/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Engine.VMDialogHandler — builds dialog text (with $ substitution commands) and signals the UI.
// The UI side is whatever is subscribed to VM::OnDialog (STUB(3ds): no dialog UI yet).
#pragma once
#include <string>
#include "simantics/_files_contract.h"
#include "simantics/engine/VMDialogOperand.h"
namespace FSO { namespace SimAntics { namespace Engine {
class VMStackFrame;
class VMDialogHandler {
public:
    static void ShowDialog(VMStackFrame* context, Primitives::VMDialogOperand* operand, FSO::Files::Formats::IFF::Chunks::STR* source);
    /// `hasInput == false` mirrors a C# null input ("Missing String!!!").
    static std::string ParseDialogString(VMStackFrame* context, const std::string& input, FSO::Files::Formats::IFF::Chunks::STR* source, bool hasInput = true);
private:
    //should use a Trie for this in future, for performance reasons
    static const char* const valid[26];
    static bool CommandSubstrValid(const std::string& command);
};
}}}
