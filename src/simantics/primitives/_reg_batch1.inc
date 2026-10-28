/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
#include "simantics/primitives/VMAnimateSim.h"
#include "simantics/primitives/VMFindBestAction.h"
#include "simantics/primitives/VMGetDirectionTo.h"
#include "simantics/primitives/VMPlaySound.h"
#include "simantics/primitives/VMSetBalloonHeadline.h"
#include "simantics/primitives/VMStopAllSounds.h"
#include "simantics/primitives/VMSysLog.h"
#include "simantics/primitives/VMTestObjectType.h"
#include "simantics/primitives/VMTransferFunds.h"
//--REG--
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
