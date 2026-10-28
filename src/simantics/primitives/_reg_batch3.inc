/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
#include "simantics/primitives/VMRandomNumber.h"
#include "simantics/primitives/VMIdleForInput.h"
#include "simantics/primitives/VMLookTowards.h"
#include "simantics/primitives/VMSetMotiveChange.h"
#include "simantics/primitives/VMSetToNext.h"
#include "simantics/primitives/VMDialogPrivateStrings.h"
#include "simantics/primitives/VMTestSimInteractingWith.h"
#include "simantics/primitives/VMDialogGlobalStrings.h"
#include "simantics/primitives/VMDropOnto.h"
#include "simantics/primitives/VMInvokePlugin.h"
#include "simantics/engine/VMDialogOperand.h"
//--REG--
{
    auto reg = new ::FSO::SimAntics::Engine::VMPrimitiveRegistration(new ::FSO::SimAntics::Primitives::VMRandomNumber());
    reg->Opcode = 8;
    reg->Name = "random_number";
    reg->OperandModel = ::FSO::SimAntics::Engine::VMPrimitiveRegistration::Model<::FSO::SimAntics::Primitives::VMRandomNumberOperand>();
    ctx->AddPrimitive(reg);
}
{
    auto reg = new ::FSO::SimAntics::Engine::VMPrimitiveRegistration(new ::FSO::SimAntics::Primitives::VMIdleForInput());
    reg->Opcode = 17;
    reg->Name = "idle_for_input";
    reg->OperandModel = ::FSO::SimAntics::Engine::VMPrimitiveRegistration::Model<::FSO::SimAntics::Primitives::VMIdleForInputOperand>();
    ctx->AddPrimitive(reg);
}
{
    auto reg = new ::FSO::SimAntics::Engine::VMPrimitiveRegistration(new ::FSO::SimAntics::Primitives::VMLookTowards());
    reg->Opcode = 22;
    reg->Name = "look_towards";
    reg->OperandModel = ::FSO::SimAntics::Engine::VMPrimitiveRegistration::Model<::FSO::SimAntics::Primitives::VMLookTowardsOperand>();
    ctx->AddPrimitive(reg);
}
{
    auto reg = new ::FSO::SimAntics::Engine::VMPrimitiveRegistration(new ::FSO::SimAntics::Primitives::VMSetMotiveChange());
    reg->Opcode = 29;
    reg->Name = "set_motive_deltas";
    reg->OperandModel = ::FSO::SimAntics::Engine::VMPrimitiveRegistration::Model<::FSO::SimAntics::Primitives::VMSetMotiveChangeOperand>();
    ctx->AddPrimitive(reg);
}
{
    auto reg = new ::FSO::SimAntics::Engine::VMPrimitiveRegistration(new ::FSO::SimAntics::Primitives::VMSetToNext());
    reg->Opcode = 31;
    reg->Name = "set_to_next";
    reg->OperandModel = ::FSO::SimAntics::Engine::VMPrimitiveRegistration::Model<::FSO::SimAntics::Primitives::VMSetToNextOperand>();
    ctx->AddPrimitive(reg);
}
{
    auto reg = new ::FSO::SimAntics::Engine::VMPrimitiveRegistration(new ::FSO::SimAntics::Primitives::VMDialogPrivateStrings());
    reg->Opcode = 36;
    reg->Name = "dialog_private";
    reg->OperandModel = ::FSO::SimAntics::Engine::VMPrimitiveRegistration::Model<::FSO::SimAntics::Primitives::VMDialogOperand>();
    ctx->AddPrimitive(reg);
}
{
    auto reg = new ::FSO::SimAntics::Engine::VMPrimitiveRegistration(new ::FSO::SimAntics::Primitives::VMTestSimInteractingWith());
    reg->Opcode = 37;
    reg->Name = "test_sim_interacting_with";
    reg->OperandModel = ::FSO::SimAntics::Engine::VMPrimitiveRegistration::Model<::FSO::SimAntics::Primitives::VMTestSimInteractingWithOperand>();
    ctx->AddPrimitive(reg);
}
{
    auto reg = new ::FSO::SimAntics::Engine::VMPrimitiveRegistration(new ::FSO::SimAntics::Primitives::VMDialogGlobalStrings());
    reg->Opcode = 38;
    reg->Name = "dialog_global";
    reg->OperandModel = ::FSO::SimAntics::Engine::VMPrimitiveRegistration::Model<::FSO::SimAntics::Primitives::VMDialogOperand>();
    ctx->AddPrimitive(reg);
}
{
    auto reg = new ::FSO::SimAntics::Engine::VMPrimitiveRegistration(new ::FSO::SimAntics::Primitives::VMDropOnto());
    reg->Opcode = 43;
    reg->Name = "drop_onto";
    reg->OperandModel = ::FSO::SimAntics::Engine::VMPrimitiveRegistration::Model<::FSO::SimAntics::Primitives::VMDropOntoOperand>();
    ctx->AddPrimitive(reg);
}
{
    auto reg = new ::FSO::SimAntics::Engine::VMPrimitiveRegistration(new ::FSO::SimAntics::Primitives::VMInvokePlugin());
    reg->Opcode = 62;
    reg->Name = "invoke_plugin";
    reg->OperandModel = ::FSO::SimAntics::Engine::VMPrimitiveRegistration::Model<::FSO::SimAntics::Primitives::VMInvokePluginOperand>();
    ctx->AddPrimitive(reg);
}
