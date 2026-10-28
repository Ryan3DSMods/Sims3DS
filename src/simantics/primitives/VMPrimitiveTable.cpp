/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Generated from _reg_batch*.inc — registers every translated primitive
// (C#: the AddPrimitive calls in the VMContext constructor).
#include "simantics/VMContext.h"
#include "simantics/engine/VMPrimitiveRegistration.h"
#include "simantics/primitives/VMAnimateSim.h"
#include "simantics/primitives/VMFindBestAction.h"
#include "simantics/primitives/VMGetDirectionTo.h"
#include "simantics/primitives/VMPlaySound.h"
#include "simantics/primitives/VMSetBalloonHeadline.h"
#include "simantics/primitives/VMStopAllSounds.h"
#include "simantics/primitives/VMSysLog.h"
#include "simantics/primitives/VMTestObjectType.h"
#include "simantics/primitives/VMTransferFunds.h"
#include "simantics/primitives/VMSleep.h"
#include "simantics/primitives/VMExpression.h"
#include "simantics/primitives/VMDrop.h"
#include "simantics/primitives/VMChangeSuitOrAccessory.h"
#include "simantics/primitives/VMRefresh.h"
#include "simantics/primitives/VMFindLocationFor.h"
#include "simantics/primitives/VMRunFunctionalTree.h"
#include "simantics/primitives/VMGotoRelativePosition.h"
#include "simantics/primitives/VMNotifyOutOfIdle.h"
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
#include "simantics/primitives/VMGenericTSOCall.h"
#include "simantics/primitives/VMGetDistanceTo.h"
#include "simantics/primitives/VMPushInteraction.h"
#include "simantics/primitives/VMFindBestObjectForFunction.h"
#include "simantics/primitives/VMBreakPoint.h"
#include "simantics/primitives/VMSpecialEffect.h"
#include "simantics/primitives/VMCreateObjectInstance.h"
#include "simantics/primitives/VMGotoRoutingSlot.h"
#include "simantics/primitives/VMGetTerrainInfo.h"
#include "simantics/primitives/VMDialogSemiGlobalStrings.h"
#include "simantics/primitives/VMGrab.h"
#include "simantics/primitives/VMInventoryOperations.h"
#include "simantics/primitives/VMOnlineJobsCall.h"
#include "simantics/primitives/VMReach.h"
#include "simantics/primitives/VMRelationship.h"
#include "simantics/primitives/VMRemoveObjectInstance.h"
#include "simantics/primitives/VMRunTreeByName.h"
#include "simantics/primitives/VMSnap.h"
#include "simantics/primitives/VMGosubFoundAction.h"

namespace FSO { namespace SimAntics {
void RegisterDefaultPrimitives(VMContext* ctx) {
    // ---- _reg_batch1.inc ----
{
    auto reg = new Engine::VMPrimitiveRegistration(new Primitives::VMGetDirectionTo());
    reg->Opcode = 12;
    reg->Name = "get_direction_to";
    reg->OperandModel = Engine::VMPrimitiveRegistration::Model<Primitives::VMGetDirectionToOperand>();
    ctx->AddPrimitive(reg);
}
{
    auto reg = new Engine::VMPrimitiveRegistration(new Primitives::VMPlaySound());
    reg->Opcode = 23;
    reg->Name = "play_sound";
    reg->OperandModel = Engine::VMPrimitiveRegistration::Model<Primitives::VMPlaySoundOperand>();
    ctx->AddPrimitive(reg);
}
{
    auto reg = new Engine::VMPrimitiveRegistration(new Primitives::VMTransferFunds());
    reg->Opcode = 25;
    reg->Name = "transfer_funds";
    reg->OperandModel = Engine::VMPrimitiveRegistration::Model<Primitives::VMTransferFundsOperand>();
    ctx->AddPrimitive(reg);
}
{
    auto reg = new Engine::VMPrimitiveRegistration(new Primitives::VMSysLog());
    reg->Opcode = 30;
    reg->Name = "syslog";
    reg->OperandModel = Engine::VMPrimitiveRegistration::Model<Primitives::VMSysLogOperand>();
    ctx->AddPrimitive(reg);
}
{
    auto reg = new Engine::VMPrimitiveRegistration(new Primitives::VMTestObjectType());
    reg->Opcode = 32;
    reg->Name = "test_object_type";
    reg->OperandModel = Engine::VMPrimitiveRegistration::Model<Primitives::VMTestObjectTypeOperand>();
    ctx->AddPrimitive(reg);
}
{
    auto reg = new Engine::VMPrimitiveRegistration(new Primitives::VMSetBalloonHeadline());
    reg->Opcode = 41;
    reg->Name = "set_balloon_headline";
    reg->OperandModel = Engine::VMPrimitiveRegistration::Model<Primitives::VMSetBalloonHeadlineOperand>();
    ctx->AddPrimitive(reg);
}
{
    auto reg = new Engine::VMPrimitiveRegistration(new Primitives::VMAnimateSim());
    reg->Opcode = 44;
    reg->Name = "animate";
    reg->OperandModel = Engine::VMPrimitiveRegistration::Model<Primitives::VMAnimateSimOperand>();
    ctx->AddPrimitive(reg);
}
{
    auto reg = new Engine::VMPrimitiveRegistration(new Primitives::VMStopAllSounds());
    reg->Opcode = 48;
    reg->Name = "stop_all_sounds";
    reg->OperandModel = Engine::VMPrimitiveRegistration::Model<Primitives::VMStopAllSoundsOperand>();
    ctx->AddPrimitive(reg);
}
{
    auto reg = new Engine::VMPrimitiveRegistration(new Primitives::VMFindBestAction());
    reg->Opcode = 65;
    reg->Name = "find_best_action";
    reg->OperandModel = Engine::VMPrimitiveRegistration::Model<Primitives::VMFindBestActionOperand>();
    ctx->AddPrimitive(reg);
}

    // ---- _reg_batch2.inc ----
{
    auto reg = new Engine::VMPrimitiveRegistration(new Primitives::VMSleep());
    reg->Opcode = 0;
    reg->Name = "sleep";
    reg->OperandModel = Engine::VMPrimitiveRegistration::Model<Primitives::VMSleepOperand>();
    ctx->AddPrimitive(reg);
}
{
    auto reg = new Engine::VMPrimitiveRegistration(new Primitives::VMExpression());
    reg->Opcode = 2;
    reg->Name = "expression";
    reg->OperandModel = Engine::VMPrimitiveRegistration::Model<Primitives::VMExpressionOperand>();
    ctx->AddPrimitive(reg);
}
{
    auto reg = new Engine::VMPrimitiveRegistration(new Primitives::VMDrop());
    reg->Opcode = 5;
    reg->Name = "drop";
    reg->OperandModel = Engine::VMPrimitiveRegistration::Model<Primitives::VMDropOperand>();
    ctx->AddPrimitive(reg);
}
{
    auto reg = new Engine::VMPrimitiveRegistration(new Primitives::VMChangeSuitOrAccessory());
    reg->Opcode = 6;
    reg->Name = "change_suit_or_accessory";
    reg->OperandModel = Engine::VMPrimitiveRegistration::Model<Primitives::VMChangeSuitOrAccessoryOperand>();
    ctx->AddPrimitive(reg);
}
{
    auto reg = new Engine::VMPrimitiveRegistration(new Primitives::VMRefresh());
    reg->Opcode = 7;
    reg->Name = "refresh";
    reg->OperandModel = Engine::VMPrimitiveRegistration::Model<Primitives::VMRefreshOperand>();
    ctx->AddPrimitive(reg);
}
{
    auto reg = new Engine::VMPrimitiveRegistration(new Primitives::VMFindLocationFor());
    reg->Opcode = 16;
    reg->Name = "find_location_for";
    reg->OperandModel = Engine::VMPrimitiveRegistration::Model<Primitives::VMFindLocationForOperand>();
    ctx->AddPrimitive(reg);
}
{
    auto reg = new Engine::VMPrimitiveRegistration(new Primitives::VMRunFunctionalTree());
    reg->Opcode = 20;
    reg->Name = "run_functional_tree";
    reg->OperandModel = Engine::VMPrimitiveRegistration::Model<Primitives::VMRunFunctionalTreeOperand>();
    ctx->AddPrimitive(reg);
}
{
    auto reg = new Engine::VMPrimitiveRegistration(new Primitives::VMGotoRelativePosition());
    reg->Opcode = 27;
    reg->Name = "goto_relative";
    reg->OperandModel = Engine::VMPrimitiveRegistration::Model<Primitives::VMGotoRelativePositionOperand>();
    ctx->AddPrimitive(reg);
}
{
    auto reg = new Engine::VMPrimitiveRegistration(new Primitives::VMNotifyOutOfIdle());
    reg->Opcode = 49;
    reg->Name = "stackobj_notify_out_of_idle";
    reg->OperandModel = Engine::VMPrimitiveRegistration::Model<Primitives::VMAnimateSimOperand>();
    ctx->AddPrimitive(reg);
}

    // ---- _reg_batch3.inc ----
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

    // ---- _reg_batch4.inc ----
{
    auto reg = new FSO::SimAntics::Engine::VMPrimitiveRegistration(new FSO::SimAntics::Primitives::VMGenericTSOCall());
    reg->Opcode = 1;
    reg->Name = "generic_sims_online_call";
    reg->OperandModel = FSO::SimAntics::Engine::VMPrimitiveRegistration::Model<FSO::SimAntics::Primitives::VMGenericTSOCallOperand>();
    ctx->AddPrimitive(reg);
}
{
    auto reg = new FSO::SimAntics::Engine::VMPrimitiveRegistration(new FSO::SimAntics::Primitives::VMGetDistanceTo());
    reg->Opcode = 11;
    reg->Name = "get_distance_to";
    reg->OperandModel = FSO::SimAntics::Engine::VMPrimitiveRegistration::Model<FSO::SimAntics::Primitives::VMGetDistanceToOperand>();
    ctx->AddPrimitive(reg);
}
{
    auto reg = new FSO::SimAntics::Engine::VMPrimitiveRegistration(new FSO::SimAntics::Primitives::VMPushInteraction());
    reg->Opcode = 13;
    reg->Name = "push_interaction";
    reg->OperandModel = FSO::SimAntics::Engine::VMPrimitiveRegistration::Model<FSO::SimAntics::Primitives::VMPushInteractionOperand>();
    ctx->AddPrimitive(reg);
}
{
    auto reg = new FSO::SimAntics::Engine::VMPrimitiveRegistration(new FSO::SimAntics::Primitives::VMFindBestObjectForFunction());
    reg->Opcode = 14;
    reg->Name = "find_best_object_for_function";
    reg->OperandModel = FSO::SimAntics::Engine::VMPrimitiveRegistration::Model<FSO::SimAntics::Primitives::VMFindBestObjectForFunctionOperand>();
    ctx->AddPrimitive(reg);
}
{
    auto reg = new FSO::SimAntics::Engine::VMPrimitiveRegistration(new FSO::SimAntics::Primitives::VMBreakPoint());
    reg->Opcode = 15;
    reg->Name = "breakpoint";
    reg->OperandModel = FSO::SimAntics::Engine::VMPrimitiveRegistration::Model<FSO::SimAntics::Primitives::VMBreakPointOperand>();
    ctx->AddPrimitive(reg);
}
{
    auto reg = new FSO::SimAntics::Engine::VMPrimitiveRegistration(new FSO::SimAntics::Primitives::VMSpecialEffect());
    reg->Opcode = 35;
    reg->Name = "special_effect";
    reg->OperandModel = FSO::SimAntics::Engine::VMPrimitiveRegistration::Model<FSO::SimAntics::Primitives::VMSpecialEffectOperand>();
    ctx->AddPrimitive(reg);
}
{
    auto reg = new FSO::SimAntics::Engine::VMPrimitiveRegistration(new FSO::SimAntics::Primitives::VMCreateObjectInstance());
    reg->Opcode = 42;
    reg->Name = "create_object_instance";
    reg->OperandModel = FSO::SimAntics::Engine::VMPrimitiveRegistration::Model<FSO::SimAntics::Primitives::VMCreateObjectInstanceOperand>();
    ctx->AddPrimitive(reg);
}
{
    auto reg = new FSO::SimAntics::Engine::VMPrimitiveRegistration(new FSO::SimAntics::Primitives::VMGotoRoutingSlot());
    reg->Opcode = 45;
    reg->Name = "goto_routing_slot";
    reg->OperandModel = FSO::SimAntics::Engine::VMPrimitiveRegistration::Model<FSO::SimAntics::Primitives::VMGotoRoutingSlotOperand>();
    ctx->AddPrimitive(reg);
}
{
    auto reg = new FSO::SimAntics::Engine::VMPrimitiveRegistration(new FSO::SimAntics::Primitives::VMGetTerrainInfo());
    reg->Opcode = 63;
    reg->Name = "get_terrain_info";
    reg->OperandModel = FSO::SimAntics::Engine::VMPrimitiveRegistration::Model<FSO::SimAntics::Primitives::VMGetTerrainInfoOperand>();
    ctx->AddPrimitive(reg);
}

    // ---- _reg_batch5.inc ----
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
    // ---- TS1(sims): The Sims 1 opcodes FreeSims does not register (see VMGosubFoundAction.h) ----
{
    // TS1 primitive 3 "Find Best Interaction" (PersonGlobals 'try autonomy'): the translated VMFindBestAction.
    auto reg = new Engine::VMPrimitiveRegistration(new Primitives::VMFindBestAction());
    reg->Opcode = 3;
    reg->Name = "find_best_interaction";
    reg->OperandModel = Engine::VMPrimitiveRegistration::Model<Primitives::VMFindBestActionOperand>();
    ctx->AddPrimitive(reg);
}
{
    // TS1 primitive 30 "Gosub Found Action" replaces FreeSims' TSO "syslog" (a no-op) in this TS1-only port.
    auto reg = new Engine::VMPrimitiveRegistration(new Primitives::VMGosubFoundAction());
    reg->Opcode = 30;
    reg->Name = "gosub_found_action";
    reg->OperandModel = Engine::VMPrimitiveRegistration::Model<Primitives::VMGosubFoundActionOperand>();
    ctx->AddPrimitive(reg);
}
}
}}
