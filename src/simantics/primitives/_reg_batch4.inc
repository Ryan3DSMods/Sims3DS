/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
#include "simantics/primitives/VMGenericTSOCall.h"
#include "simantics/primitives/VMGetDistanceTo.h"
#include "simantics/primitives/VMPushInteraction.h"
#include "simantics/primitives/VMFindBestObjectForFunction.h"
#include "simantics/primitives/VMBreakPoint.h"
#include "simantics/primitives/VMSpecialEffect.h"
#include "simantics/primitives/VMCreateObjectInstance.h"
#include "simantics/primitives/VMGotoRoutingSlot.h"
#include "simantics/primitives/VMGetTerrainInfo.h"
//--REG--
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
