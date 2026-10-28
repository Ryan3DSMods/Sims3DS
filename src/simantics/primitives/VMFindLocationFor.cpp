/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims SimsVille/SimsAntics/primitives/VMFindLocationFor.cs
#include "simantics/primitives/VMFindLocationFor.h"
#include <cmath>
#include <cstring>
#include <algorithm>
#include "core/io.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMThread.h"
#include "simantics/engine/VMSimanticsException.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/model/VMPlacementError.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"
#include "simantics/VMArchitecture.h"

namespace FSO { namespace SimAntics { namespace Primitives {

using namespace FSO::SimAntics::Engine;
using FSO::LotView::Model::LotTilePos;
using DirectionT = FSO::LotView::Model::Direction;
using FSO::SimAntics::Model::VMPlacementError;

// TS1(sims): FreeSO's table. FreeSims' started at (16,0), two notches off, so 'along object vector' (mode 3)
// stepped *across* the object's facing: a car pool created next to the school bus parked in the lane beside it, on the
// bus door's routing slot, and children could not board (route result 8, blocker 'Car - Junky').
const std::array<LotTilePos, 8> VMFindLocationFor::DirectionVectors = {{
    LotTilePos(0, 16, 0),
    LotTilePos(-16, 16, 0),
    LotTilePos(-16, 0, 0),
    LotTilePos(-16, -16, 0),
    LotTilePos(0, -16, 0),
    LotTilePos(16, -16, 0),
    LotTilePos(16, 0, 0),
    LotTilePos(16, 16, 0),
}};

// C# dereferencing null throws NullReferenceException, which the thread catches; mirror that instead of crashing.
static VMEntity* NotNull(VMEntity* e, VMStackFrame* context)
{
    if (e == nullptr) throw VMSimanticsException("Object reference not set to an instance of an object.", context);
    return e;
}

VMPrimitiveExitCode VMFindLocationFor::Execute(VMStackFrame* context, VMPrimitiveOperand* args)
{
    auto operand = static_cast<VMFindLocationForOperand*>(args);
    auto refObj = (operand->UseLocalAsRef()) ? context->VM()->GetObjectById((int16_t)context->Locals.at(operand->Local)) : context->Caller;

    // FIX(sims) QA4: the object to place was deleted under the running tree (global 'do the drop' keeps the held
    // object's id in a local across a walk; FoodHD 'drop food' after another Sim took the meal): no location (false),
    // as for any object that cannot be placed, instead of resetting the Sim (QA3 Hick lot, QA4 Downtown janitor).
    if (context->StackObject == nullptr && context->VM()->TS1) return VMPrimitiveExitCode::GOTO_FALSE;
    auto obj = NotNull(context->StackObject, context);

    switch (operand->Mode)
    {
        case 0:
            //default
            if (FindLocationFor(obj, NotNull(refObj, context), context->VM()->Context)) return VMPrimitiveExitCode::GOTO_TRUE;
            else return VMPrimitiveExitCode::GOTO_FALSE;
        case 1:
            //out of world
            obj->SetPosition(LotTilePos::OUT_OF_WORLD, DirectionT::NORTH, context->VM()->Context);
            return VMPrimitiveExitCode::GOTO_TRUE;
        case 2: {
            //"smoke cloud" - halfway between callee and caller (is "caller" actually reference object?)
            auto smokePos = context->Callee->Position();
            smokePos = smokePos + context->Caller->Position();
            smokePos = smokePos / 2;
            smokePos = smokePos - LotTilePos(8, 8, 0); //smoke is 2x2... offset to center it.
            return (obj->SetPosition(smokePos, DirectionT::NORTH, context->VM()->Context).Status == VMPlacementError::Success) ?
                VMPrimitiveExitCode::GOTO_TRUE : VMPrimitiveExitCode::GOTO_FALSE;
        }
        case 3:
        case 4: {
            //along object vector
            // C#: (int)Math.Round(Math.Log((double)refObj.Direction, 2)) — Math.Round is round-half-even, like nearbyint.
            auto intDir = (int32_t)std::nearbyint(std::log((double)(int32_t)NotNull(refObj, context)->Direction()) / std::log(2.0));
            if (operand->Mode == 4) intDir = (intDir + 2) % 8; //lateral to object vector
            if (FindLocationVector(obj, refObj, context->VM()->Context, intDir)) return VMPrimitiveExitCode::GOTO_TRUE;
            else return VMPrimitiveExitCode::GOTO_FALSE;
        }
        case 5: {
            // FIX(sims): FreeSO mode 5 "random": 100 tries on random tiles of the lot at the reference object's level.
            // FreeSims had no mode 5, so the global 'Wander' (param 1+, e.g. the Unleashed skunk / raccoon NPCs) never
            // found a spot and those NPCs stood still.
            auto ctx = context->VM()->Context;
            auto arch = ctx->Architecture();
            auto ref = NotNull(refObj, context);
            if (arch == nullptr || arch->Width < 3 || arch->Height < 3) return VMPrimitiveExitCode::GOTO_FALSE;
            for (int i = 0; i < 100; i++) {
                auto loc = LotTilePos::FromBigTile((int16_t)(ctx->NextRandom((uint64_t)arch->Width - 2) + 1),
                    (int16_t)(ctx->NextRandom((uint64_t)arch->Height - 2) + 1), ref->Position().Level);
                if (obj->SetPosition(loc, DirectionT::NORTH, ctx).Status == VMPlacementError::Success) return VMPrimitiveExitCode::GOTO_TRUE;
            }
            return VMPrimitiveExitCode::GOTO_FALSE;
        }
    }

    return VMPrimitiveExitCode::GOTO_FALSE;
}

bool VMFindLocationFor::FindLocationVector(VMEntity* obj, VMEntity* refObj, VMContext* context, int32_t dir)
{
    LotTilePos step = DirectionVectors.at(dir); // C# array index throws when out of range
    for (int32_t i = 0; i < 32; i++)
    {
        if (obj->SetPosition(LotTilePos(refObj->Position()) + step * (i / 2),
            (DirectionT)(1 << (dir)), context).Status == VMPlacementError::Success)
            return true;
        if (i % 2 != 0)
        {
            if (obj->SetPosition(LotTilePos(refObj->Position()) - step * (i / 2),
                (DirectionT)(1 << (dir)), context).Status == VMPlacementError::Success)
                return true;
        }
    }
    return false;
}

bool VMFindLocationFor::FindLocationFor(VMEntity* obj, VMEntity* refObj, VMContext* context)
{
    for (int32_t i = 0; i < 10; i++)
    {
        if (i == 0)
        {
            for (int32_t j = 0; j < 4; j++)
            {
                if (obj->SetPosition(LotTilePos(refObj->Position()), (DirectionT)(1 << (j * 2)), context).Status == VMPlacementError::Success)
                    return true;
            }
        }
        else
        {
            LotTilePos bPos = refObj->Position();
            for (int32_t x = -i; x <= i; x++)
            {
                for (int32_t j = 0; j < 8; j++)
                {
                    if (obj->SetPosition(LotTilePos::FromBigTile((int16_t)(bPos.TileX() + x), (int16_t)(bPos.TileY() + ((j % 2) * 2 - 1) * i), bPos.Level),
                        (DirectionT)(1 << ((j / 2) * 2)), context).Status == VMPlacementError::Success)
                        return true;
                }
            }

            for (int32_t y = 1 - i; y < i; y++)
            {
                for (int32_t j = 0; j < 8; j++)
                {
                    if (obj->SetPosition(LotTilePos::FromBigTile((int16_t)(bPos.TileX() + ((j % 2) * 2 - 1) * i), (int16_t)(bPos.TileY() + y), bPos.Level),
                        (DirectionT)(1 << ((j / 2) * 2)), context).Status == VMPlacementError::Success)
                        return true;
                }
            }
        }
    }
    return false;
}

// region VMPrimitiveOperand Members
void VMFindLocationForOperand::Read(const uint8_t* bytes)
{
    st::Reader io(bytes, VMPrimitiveOperand::OPERAND_SIZE, st::Endian::Little);
    Mode = io.ReadByte();
    Local = io.ReadByte();
    Flags = io.ReadByte();
}

void VMFindLocationForOperand::Write(uint8_t* bytes)
{
    st::Writer io(st::Endian::Little);
    io.WriteByte(Mode);
    io.WriteByte(Local);
    io.WriteByte(Flags);
    std::memcpy(bytes, io.ToArray().data(), std::min(io.ToArray().size(), VMPrimitiveOperand::OPERAND_SIZE));
}
// endregion

}}}
