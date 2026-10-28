/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
#include "simantics/primitives/VMDialogSemiGlobalStrings.h"
#include "simantics/primitives/VMGrab.h"
#include "simantics/primitives/VMInventoryOperations.h"
#include "simantics/primitives/VMOnlineJobsCall.h"
#include "simantics/primitives/VMReach.h"
#include "simantics/primitives/VMRelationship.h"
#include "simantics/primitives/VMRemoveObjectInstance.h"
#include "simantics/primitives/VMRunTreeByName.h"
#include "simantics/primitives/VMSnap.h"
//--REG--
{
    auto reg = new Engine::VMPrimitiveRegistration(new Primitives::VMGrab());
    reg->Opcode = 4;
    reg->Name = "grab";
    reg->OperandModel = Engine::VMPrimitiveRegistration::Model<Primitives::VMGrabOperand>();
    ctx->AddPrimitive(reg);
}
{
    auto reg = new Engine::VMPrimitiveRegistration(new Primitives::VMRemoveObjectInstance());
    reg->Opcode = 18;
    reg->Name = "remove_object_instance";
    reg->OperandModel = Engine::VMPrimitiveRegistration::Model<Primitives::VMRemoveObjectInstanceOperand>();
    ctx->AddPrimitive(reg);
}
{
    auto reg = new Engine::VMPrimitiveRegistration(new Primitives::VMRelationship());
    reg->Opcode = 24;
    reg->Name = "old_relationship";
    reg->OperandModel = Engine::VMPrimitiveRegistration::Model<Primitives::VMOldRelationshipOperand>();
    ctx->AddPrimitive(reg);
}
{
    auto reg = new Engine::VMPrimitiveRegistration(new Primitives::VMRelationship());
    reg->Opcode = 26;
    reg->Name = "relationship";
    reg->OperandModel = Engine::VMPrimitiveRegistration::Model<Primitives::VMRelationshipOperand>();
    ctx->AddPrimitive(reg);
}
{
    auto reg = new Engine::VMPrimitiveRegistration(new Primitives::VMRunTreeByName());
    reg->Opcode = 28;
    reg->Name = "run_tree_by_name";
    reg->OperandModel = Engine::VMPrimitiveRegistration::Model<Primitives::VMRunTreeByNameOperand>();
    ctx->AddPrimitive(reg);
}
{
    auto reg = new Engine::VMPrimitiveRegistration(new Primitives::VMDialogSemiGlobalStrings());
    reg->Opcode = 39;
    reg->Name = "dialog_semiglobal";
    reg->OperandModel = Engine::VMPrimitiveRegistration::Model<Primitives::VMDialogOperand>();
    ctx->AddPrimitive(reg);
}
{
    auto reg = new Engine::VMPrimitiveRegistration(new Primitives::VMOnlineJobsCall());
    reg->Opcode = 40;
    reg->Name = "online_jobs_call";
    reg->OperandModel = Engine::VMPrimitiveRegistration::Model<Primitives::VMOnlineJobsCallOperand>();
    ctx->AddPrimitive(reg);
}
{
    auto reg = new Engine::VMPrimitiveRegistration(new Primitives::VMSnap());
    reg->Opcode = 46;
    reg->Name = "snap";
    reg->OperandModel = Engine::VMPrimitiveRegistration::Model<Primitives::VMSnapOperand>();
    ctx->AddPrimitive(reg);
}
{
    auto reg = new Engine::VMPrimitiveRegistration(new Primitives::VMReach());
    reg->Opcode = 47;
    reg->Name = "reach";
    reg->OperandModel = Engine::VMPrimitiveRegistration::Model<Primitives::VMReachOperand>();
    ctx->AddPrimitive(reg);
}
{
    auto reg = new Engine::VMPrimitiveRegistration(new Primitives::VMInventoryOperations());
    reg->Opcode = 67;
    reg->Name = "inventory_operations";
    reg->OperandModel = Engine::VMPrimitiveRegistration::Model<Primitives::VMInventoryOperationsOperand>();
    ctx->AddPrimitive(reg);
}
