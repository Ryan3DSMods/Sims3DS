/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// TS1(sims): registers The Sims 1 primitives that FreeSims lacks (FreeSO has them), on top of
// RegisterDefaultPrimitives. Called from the VMContext constructor.
//   25 Alter Budget        -> VMTS1Budget (TSO transfer funds for non-TS1 VMs)
//   51 Manage Inventory    -> VMTS1InventoryOperations (Hot Date and later; FreeSims left 51 unregistered = "true")
//   9 Burn, 21 Show String, 33 Find 5 Worst Motives -> VMTS1MiscPrimitives
//   19 Make New Character -> VMTS1MakeNewCharacter (babies, clones, pets; FreeSO)
//   10 Tutorial, 34 UI Effect -> no-ops that answer true (tutorial highlights / UI flashes; no game state)
#include "simantics/VMContext.h"
#include "simantics/engine/VMPrimitiveRegistration.h"
#include "simantics/engine/VMPrimitiveOperand.h"
#include "simantics/primitives/VMTS1InventoryOperations.h"
#include "simantics/primitives/VMTS1Budget.h"
#include "simantics/primitives/VMTS1MakeNewCharacter.h"
#include "simantics/primitives/VMTS1MiscPrimitives.h"

namespace FSO { namespace SimAntics {
namespace Primitives {
// TS1(sims): "Tutorial" (10) and "UI Effect" (34) only drive the help system's UI highlights.
class VMTS1UINoOp : public Engine::VMPrimitiveHandler {
public:
    Engine::VMPrimitiveExitCode Execute(Engine::VMStackFrame*, Engine::VMPrimitiveOperand*) override { return Engine::VMPrimitiveExitCode::GOTO_TRUE; }
};
}

void RegisterTS1ExpansionPrimitives(VMContext* ctx) {
    {
        auto reg = new Engine::VMPrimitiveRegistration(new Primitives::VMTS1Budget());
        reg->Opcode = 25;
        reg->Name = "transfer_funds";
        reg->OperandModel = Engine::VMPrimitiveRegistration::Model<Primitives::VMTransferFundsOperand>();
        ctx->AddPrimitive(std::unique_ptr<Engine::VMPrimitiveRegistration>(reg));
    }
    {
        auto reg = new Engine::VMPrimitiveRegistration(new Primitives::VMTS1InventoryOperations());
        reg->Opcode = 51;
        reg->Name = "manage_inventory";
        reg->OperandModel = Engine::VMPrimitiveRegistration::Model<Primitives::VMTS1InventoryOperationsOperand>();
        ctx->AddPrimitive(std::unique_ptr<Engine::VMPrimitiveRegistration>(reg));
    }
    {
        auto reg = new Engine::VMPrimitiveRegistration(new Primitives::VMTS1MakeNewCharacter());
        reg->Opcode = 19;
        reg->Name = "make_new_character";
        reg->OperandModel = Engine::VMPrimitiveRegistration::Model<Primitives::VMTS1MakeNewCharacterOperand>();
        ctx->AddPrimitive(std::unique_ptr<Engine::VMPrimitiveRegistration>(reg));
    }
    {
        auto reg = new Engine::VMPrimitiveRegistration(new Primitives::VMBurn());
        reg->Opcode = 9; reg->Name = "burn";
        reg->OperandModel = Engine::VMPrimitiveRegistration::Model<Primitives::VMBurnOperand>();
        ctx->AddPrimitive(std::unique_ptr<Engine::VMPrimitiveRegistration>(reg));
    }
    {
        auto reg = new Engine::VMPrimitiveRegistration(new Primitives::VMShowString());
        reg->Opcode = 21; reg->Name = "show_string";
        reg->OperandModel = Engine::VMPrimitiveRegistration::Model<Primitives::VMShowStringOperand>();
        ctx->AddPrimitive(std::unique_ptr<Engine::VMPrimitiveRegistration>(reg));
    }
    {
        auto reg = new Engine::VMPrimitiveRegistration(new Primitives::VMFind5WorstMotives());
        reg->Opcode = 33; reg->Name = "find_5_worst_motives";
        reg->OperandModel = Engine::VMPrimitiveRegistration::Model<Primitives::VMNoOperand>();
        ctx->AddPrimitive(std::unique_ptr<Engine::VMPrimitiveRegistration>(reg));
    }
    for (uint16_t op : { (uint16_t)10, (uint16_t)34 }) {
        if (ctx->Primitives[op] != nullptr) continue;
        auto reg = new Engine::VMPrimitiveRegistration(new Primitives::VMTS1UINoOp());
        reg->Opcode = op;
        reg->Name = (op == 10) ? "tutorial" : "ui_effect";
        ctx->AddPrimitive(std::unique_ptr<Engine::VMPrimitiveRegistration>(reg));
    }
}

}}
