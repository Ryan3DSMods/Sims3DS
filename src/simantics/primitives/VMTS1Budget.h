// FSO.SimAntics.Primitives.VMTS1Budget (FreeSO tso.simantics/Primitives/VMTS1Budget.cs)
/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// TS1(sims): The Sims 1 primitive 25 "Alter Budget". Like TSO transfer funds (same operand), but it only
// adds to / takes from the family budget. FreeSims registers the TSO VMTransferFunds (object budgets through the
// TSO global link) for opcode 25; in TS1 the money is the lot family's, kept in globals 13/14 "Budget Mod/Div 10000"
// while the family is on its lot (VMTS1FamilyLoader) and in FAMI.Budget. Used by the expansion packs' shops, food
// stands, tips, rentals, Vacation tokens, Superstar pay, Makin' Magic...
#pragma once
#include "simantics/primitives/VMTransferFunds.h"

namespace FSO { namespace SimAntics { class VM; } }
namespace FSO { namespace SimAntics { namespace Primitives {

//like tso transfer funds, but really scaled back (only deduct from maxis)
class VMTS1Budget : public Engine::VMPrimitiveHandler {
public:
    Engine::VMPrimitiveExitCode Execute(Engine::VMStackFrame* context, Engine::VMPrimitiveOperand* args) override;
    /// TS1(sims): the lot family's budget (globals 13/14 when a family is on the lot, else FAMI.Budget).
    static int32_t GetBudget(FSO::SimAntics::VM* vm);
    static void SetBudget(FSO::SimAntics::VM* vm, int32_t budget);
private:
    VMTransferFunds TSO; // non-TS1 VMs keep FreeSims' behaviour
};

}}}
