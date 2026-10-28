/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims SimsVille/SimsAntics/primitives/VMDrop.cs
#include "simantics/primitives/VMDrop.h"
#include <cmath>
#include "core/io.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMMultitileGroup.h"
#include "simantics/model/VMPlacementError.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"

namespace FSO { namespace SimAntics { namespace Primitives {

using namespace FSO::SimAntics::Engine;
using FSO::LotView::Model::LotTilePos;
using FSO::SimAntics::Model::VMPlacementError;

const std::array<LotTilePos, 8> VMDrop::Positions = {{
    LotTilePos(0, -16, 0), //NORTH
    LotTilePos(16, -16, 0), //NORTHWEST
    LotTilePos(16, 0, 0), //WEST
    LotTilePos(16, 16, 0), //SOUTHWEST
    LotTilePos(0, 16, 0), //SOUTH
    LotTilePos(-16, 16, 0), //SOUTHEAST
    LotTilePos(-16, 0, 0), //EAST
    LotTilePos(-16, -16, 0) //NORTHEAST
}};

VMPrimitiveExitCode VMDrop::Execute(VMStackFrame* context, VMPrimitiveOperand* args)
{
    (void)args;
    auto obj = context->Caller;
    if (context->Caller->TotalSlots() == 0) return VMPrimitiveExitCode::GOTO_FALSE;
    auto drop = context->Caller->GetSlot(0);
    if (drop == nullptr) return VMPrimitiveExitCode::GOTO_FALSE;

    // C#: (int)Math.Round(Math.Log((double)obj.Direction, 2)) — Math.Round is round-half-even, like nearbyint.
    int32_t intDir = (int32_t)std::nearbyint(std::log((double)(int32_t)obj->Direction()) / std::log(2.0));
    LotTilePos basePos = LotTilePos::FromBigTile(obj->Position().TileX(), obj->Position().TileY(), obj->Position().Level);

    for (int32_t i = 0; i < (int32_t)Positions.size(); i++)
    {
        int32_t j = (i % 2 == 1) ? (((int32_t)Positions.size() - 1) - i / 2) : i / 2;

        auto posChange = drop->MultitileGroup->ChangePosition(basePos + Positions.at((j + intDir) % 8), obj->Direction(), context->VM()->Context);
        if (posChange.Status == VMPlacementError::Success)
        {
            return VMPrimitiveExitCode::GOTO_TRUE;
        }
    }

    return VMPrimitiveExitCode::GOTO_FALSE;
}

// region VMPrimitiveOperand Members
void VMDropOperand::Read(const uint8_t* bytes)
{
    st::Reader io(bytes, VMPrimitiveOperand::OPERAND_SIZE, st::Endian::Little);
    (void)io;
}

void VMDropOperand::Write(uint8_t* bytes) { (void)bytes; }
// endregion

}}}
