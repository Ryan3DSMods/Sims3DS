/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
#include "simantics/primitives/VMSleep.h"
#include "simantics/primitives/VMExpression.h"
#include "simantics/primitives/VMDrop.h"
#include "simantics/primitives/VMChangeSuitOrAccessory.h"
#include "simantics/primitives/VMRefresh.h"
#include "simantics/primitives/VMFindLocationFor.h"
#include "simantics/primitives/VMRunFunctionalTree.h"
#include "simantics/primitives/VMGotoRelativePosition.h"
#include "simantics/primitives/VMNotifyOutOfIdle.h"
#include "simantics/primitives/VMAnimateSim.h"
//--REG--
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
