/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Primitives.VMLookTowards (primitives/VMLookTowards.cs)
#include "simantics/primitives/VMLookTowards.h"
#include <cmath>
#include <vector>
#include "core/io.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMSimanticsException.h"
#include "simantics/engine/VMThread.h"
#include "simantics/engine/VMRoutingFrame.h"
#include "simantics/engine/VMSlotParser.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/entities/VMMultitileGroup.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace FSO { namespace SimAntics { namespace Primitives {

using namespace FSO::SimAntics::Engine;
using FSO::LotView::Model::LotTilePos;
using FSO::Files::Formats::IFF::Chunks::SLOTFlags;

VMPrimitiveExitCode VMLookTowards::Execute(VMStackFrame* context, VMPrimitiveOperand* args)
{
    auto operand = static_cast<VMLookTowardsOperand*>(args);
    //TODO: primitive fails if object calls it
    VMAvatar* sim = static_cast<VMAvatar*>(context->Caller);

    auto result = VMFindLocationResult();
    result.Position = LotTilePos(sim->Position());

    LotTilePos pos = LotTilePos();
    // FIX(sims) QA4: a missing stack object was a null dereference (segfault); C# throws (the Sim resets).
    if (context->StackObject == nullptr && operand->Mode != VMLookTowardsMode::HeadTowardsObject && operand->Mode != VMLookTowardsMode::BodyTowardsCamera) throw VMSimanticsException("Object reference not set to an instance of an object.", context);
    switch (operand->Mode)
    {
        case VMLookTowardsMode::HeadTowardsObject:
            return VMPrimitiveExitCode::GOTO_TRUE; //TODO: turning head towards things, with head seek timeout
        case VMLookTowardsMode::BodyTowardsCamera:
            return VMPrimitiveExitCode::GOTO_TRUE; //does not work in TSO
        case VMLookTowardsMode::BodyTowardsStackObj:
            result.RadianDirection = (float)GetDirectionTo(sim->Position(), context->StackObject->Position());
            break;
        case VMLookTowardsMode::BodyAwayFromStackObj:
            result.RadianDirection = (float)GetDirectionTo(sim->Position(), context->StackObject->Position());
            result.RadianDirection = (float)std::fmod((result.RadianDirection + M_PI), (M_PI * 2));
            break;
        case VMLookTowardsMode::BodyTowardsAverageStackObj:
            for (auto obj : context->StackObject->MultitileGroup->Objects)
                pos = pos + obj->Position();
            pos = pos / (int)context->StackObject->MultitileGroup->Objects.size();
            result.RadianDirection = (float)GetDirectionTo(sim->Position(), pos);
            break;
        case VMLookTowardsMode::BodyAwayFromAverageStackObj:
            for (auto obj : context->StackObject->MultitileGroup->Objects)
                pos = pos + obj->Position();
            pos = pos / (int)context->StackObject->MultitileGroup->Objects.size();
            result.RadianDirection = (float)GetDirectionTo(sim->Position(), pos);
            result.RadianDirection = (float)std::fmod((result.RadianDirection + M_PI), (M_PI * 2));
            break;
    }

    if (context->Thread->IsCheck) return VMPrimitiveExitCode::GOTO_FALSE;
    auto pathFinder = context->Thread->PushNewRoutingFrame(context, false); //use the path finder to do the turn animation.
    pathFinder->InitRoutes(std::vector<VMFindLocationResult>{ result });

    return VMPrimitiveExitCode::CONTINUE;
}

double VMLookTowards::GetDirectionTo(const LotTilePos& pos1, const LotTilePos& pos2)
{
    return std::atan2((double)(pos2.x - pos1.x), (double)(-(pos2.y - pos1.y)));
}

SLOTFlags VMLookTowards::RadianToFlags(double rad)
{
    // C# Math.Round is banker's rounding (MidpointRounding.ToEven) -> std::nearbyint (default FE_TONEAREST).
    int result = (int)(std::nearbyint((rad / (M_PI * 2)) * 8) + 80) % 8; //for best results, make sure rad is >-pi and <pi
    return (SLOTFlags)(1 << result);
}

void VMLookTowardsOperand::Read(const uint8_t* bytes)
{
    st::Reader io(bytes, 8, st::Endian::Little);
    Mode = (VMLookTowardsMode)io.ReadByte();
}

void VMLookTowardsOperand::Write(uint8_t* bytes)
{
    bytes[0] = (uint8_t)Mode;
}

}}}
