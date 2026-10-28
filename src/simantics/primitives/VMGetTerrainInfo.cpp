/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Primitives.VMGetTerrainInfo (primitives/VMGetTerrainInfo.cs)
#include "simantics/primitives/VMGetTerrainInfo.h"
#include <cstring>
#include <algorithm>
#include "core/io.h"
#include "world/model/LotTilePos.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMThread.h"
#include "simantics/engine/VMSimanticsException.h"
#include "simantics/entities/VMEntity.h"

namespace FSO { namespace SimAntics { namespace Primitives {
using namespace FSO::SimAntics::Engine;
using FSO::LotView::Model::LotTilePos;

VMPrimitiveExitCode VMGetTerrainInfo::Execute(VMStackFrame* context, VMPrimitiveOperand* args) {
    auto operand = static_cast<VMGetTerrainInfoOperand*>(args);
    auto obj = (operand->UseMe()) ? context->Caller : context->StackObject;
    // C# NullReferenceException on obj.Position -> exception the thread catches.
    if (obj == nullptr) throw VMSimanticsException("NullReferenceException", context);

    if (obj->Position() == LotTilePos::OUT_OF_WORLD) return VMPrimitiveExitCode::GOTO_FALSE;

    for (int i = 0; i < 5; i++) {
        if (operand->Unknown.at(i) != 0) { }
    }

    //TODO: all.

    switch (operand->Mode) {
        case 0: //four altitudes around stack object
            //3 is flat in pre-alpha. TODO when terrain is in progress.
            context->Thread->TempRegisters[0] = 3;
            context->Thread->TempRegisters[1] = 3;
            context->Thread->TempRegisters[2] = 3;
            context->Thread->TempRegisters[3] = 3;
            break;
        case 1: //slope at stack object
            //unverifiable since terrain tool is a little broken in pre-alpha
            //most likely altitude difference. +x slope down towards positive x, -x slope down towards -x.
            context->Thread->TempRegisters[0] = 0;
            context->Thread->TempRegisters[1] = 0;
            break;
        case 2: { //fixed coordinates of stack object

            //VERIFIED
            auto pos = obj->Position();
            context->Thread->TempRegisters[0] = pos.TileX(); //tile x
            context->Thread->TempRegisters[1] = (int16_t)(pos.x % 16); //sub-tile x
            context->Thread->TempRegisters[2] = pos.TileY(); //tile y
            context->Thread->TempRegisters[3] = (int16_t)(pos.y % 16); //sub-tile y
            context->Thread->TempRegisters[4] = pos.Level; //level

            break;
        }
        case 3: //grass height under stack object
            // 0-255, depending on terrain colour. (dead-alive)
            context->Thread->TempRegisters[0] = 0;
            //used for extra ball friction on (short) grass.
            break;
        case 4: //ceiling or roof exists above stack object. unused.
            context->Thread->TempRegisters[0] = 0;
            break;
    }
    return VMPrimitiveExitCode::GOTO_TRUE;
}

void VMGetTerrainInfoOperand::Read(const uint8_t* bytes) {
    st::Reader io(bytes, OPERAND_SIZE, st::Endian::Little);
    Mode = io.u8();
    Unknown1 = io.u8();
    Flags = io.u8();
    Unknown = io.bytes(5);
}

void VMGetTerrainInfoOperand::Write(uint8_t* bytes) {
    // C# writes only Mode, Unknown1 and Flags (Unknown is not written back).
    st::Writer io(st::Endian::Little);
    io.WriteByte(Mode);
    io.WriteByte(Unknown1);
    io.WriteByte(Flags);
    auto& a = io.ToArray();
    std::memcpy(bytes, a.data(), std::min(a.size(), OPERAND_SIZE));
}

}}}
