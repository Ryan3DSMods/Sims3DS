// FSO.SimAntics.Engine.Primitives.VMCreateObjectInstance (primitives/VMCreateObjectInstance.cs)
/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
#include "simantics/primitives/VMCreateObjectInstance.h"
#include <cstring>
#include <algorithm>
#include "core/io.h"
#include "world/model/LotTilePos.h"
#include "world/model/Direction.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMThread.h"
#include "simantics/engine/VMQueuedAction.h"
#include "simantics/engine/VMActionCallback.h"
#include "simantics/engine/VMSimanticsException.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMMultitileGroup.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"
#include "simantics/engine/VMTS1Neighbourhood.h"
#include "files/formats/iff/chunks/NBRS.h"

namespace FSO { namespace SimAntics { namespace Primitives {
using namespace FSO::SimAntics::Engine;
using FSO::LotView::Model::LotTilePos;
using FSO::LotView::Model::Direction;

namespace {
// C# NullReferenceException on member access of null -> exception the thread catches.
template<class T> T* NotNull(T* e, VMStackFrame* context) {
    if (e == nullptr) throw VMSimanticsException("NullReferenceException", context);
    return e;
}
}

VMPrimitiveExitCode VMCreateObjectInstance::Execute(VMStackFrame* context, VMPrimitiveOperand* args) {

    auto operand = static_cast<VMCreateObjectInstanceOperand*>(args);
    LotTilePos tpos = LotTilePos(LotTilePos::OUT_OF_WORLD);
    Direction dir;

    switch (operand->Position) {
        case VMCreateObjectPosition::UnderneathMe:
        case VMCreateObjectPosition::OnTopOfMe:
            tpos = LotTilePos(context->Caller->Position());
            dir = Direction::NORTH;
            break;
        case VMCreateObjectPosition::BelowObjectInLocal:
            tpos = LotTilePos(NotNull(context->VM()->GetObjectById((int16_t)context->Locals.at(operand->LocalToUse)), context)->Position());
            dir = Direction::NORTH;
            break;
        case VMCreateObjectPosition::BelowObjectInStackParam0:
            tpos = LotTilePos(NotNull(context->VM()->GetObjectById((int16_t)context->Args.at(0)), context)->Position());
            dir = Direction::NORTH;
            break;
        case VMCreateObjectPosition::OutOfWorld:
            dir = Direction::NORTH;
            break;
        case VMCreateObjectPosition::InSlot0OfStackObject:
        case VMCreateObjectPosition::InMyHand:
            dir = Direction::NORTH;
            //this object should start in slot 0 of the stack object!
            //we have to create it first tho so hold your horses
            break;
        case VMCreateObjectPosition::InFrontOfStackObject:
        case VMCreateObjectPosition::InFrontOfMe: {
            auto objp = NotNull((operand->Position == VMCreateObjectPosition::InFrontOfStackObject) ? context->StackObject : context->Caller, context);
            tpos = LotTilePos(objp->Position());
            switch (objp->Direction()) {
                case FSO::LotView::Model::Direction::SOUTH:
                    tpos.y += 16;
                    break;
                case FSO::LotView::Model::Direction::WEST:
                    tpos.x -= 16;
                    break;
                case FSO::LotView::Model::Direction::EAST:
                    tpos.x += 16;
                    break;
                case FSO::LotView::Model::Direction::NORTH:
                    tpos.y -= 16;
                    break;
            }
            dir = objp->Direction();
            break;
        }
        case VMCreateObjectPosition::NextToMeInDirectionOfLocal: {
            tpos = LotTilePos(context->Caller->Position());
            auto udir = context->Locals.at(operand->LocalToUse);
            dir = Direction::NORTH;
            switch (udir) {
                case 0:
                    dir = Direction::NORTH;
                    tpos.y -= 16;
                    break;
                case 2:
                    dir = Direction::EAST;
                    tpos.x += 16;
                    break;
                case 4:
                    dir = Direction::SOUTH;
                    tpos.y += 16;
                    break;
                case 6:
                    dir = Direction::WEST;
                    tpos.x -= 16;
                    break;
            }
            break;
        }
        default:
            throw VMSimanticsException("Where do I put this??", context);
    }

    // TS1(sims): FreeSO operand flags 4 "use neighbor" (create the Sim whose neighbour id is in the stack object:
    // visitors from the ped portal, the tutorial's Betty, babies / clones / pets made by Make New Character) and
    // 1 "no duplicate". FreeSims ignored both, so GUID 0 was created (nothing) and the visit failed.
    uint32_t guid = operand->GUID;
    if (context->VM()->TS1 && (operand->Flags & 4) == 4) {
        auto neigh = FSO::SimAntics::Engine::Utils::VMTS1Neighbourhood::GetNeighbor(context->StackObjectID());
        if (neigh == nullptr) return VMPrimitiveExitCode::GOTO_FALSE;
        guid = neigh->GUID;
    }
    if (context->VM()->TS1 && (operand->Flags & 1) == 1 && operand->Position != VMCreateObjectPosition::InMyHand
        && operand->Position != VMCreateObjectPosition::InSlot0OfStackObject) {
        auto objs = context->VM()->Context->ObjectQueries->GetObjectsAt(tpos);
        if (objs != nullptr) for (auto o : *objs) if (o->Object->OBJ->GUID == guid) return VMPrimitiveExitCode::GOTO_FALSE;
    }

    auto mobj = context->VM()->Context->CreateObjectInstance(guid, tpos, dir,
        (operand->PassObjectIds() && context->StackObject != nullptr) ? (context->StackObject->ObjectID) : (int16_t)0,
        (operand->PassTemp0()) ? (context->Thread->TempRegisters[0]) : (operand->PassObjectIds() ? context->Caller->ObjectID : (int16_t)0), false, false);

    if (mobj == nullptr) return VMPrimitiveExitCode::GOTO_FALSE;
    // FIX(sims): FreeSO uses the group's BaseObject (lead tile); FreeSims took Objects[0]. Multitile objects such as the
    // Hot Date taxi got the caller written into a non-lead tile ('Generate Cab': so attr 3 := me), so the cab's main
    // piece never found its passenger, honked and drove off - the Sim and their date could not take a cab home.
    auto obj = mobj->BaseObject();
    if (obj == nullptr) obj = mobj->Objects.at(0);

    if (operand->Position == VMCreateObjectPosition::InSlot0OfStackObject) NotNull(context->StackObject, context)->PlaceInSlot(obj, 0, true, context->VM()->Context);
    else if (operand->Position == VMCreateObjectPosition::InMyHand) context->Caller->PlaceInSlot(obj, 0, true, context->VM()->Context);

    if (operand->Position != VMCreateObjectPosition::OutOfWorld && obj->Position() == LotTilePos::OUT_OF_WORLD && obj->Container == nullptr) {
        obj->Delete(true, context->VM()->Context);
        return VMPrimitiveExitCode::GOTO_FALSE;
    }
    if ((operand->Flags & (1 << 6)) > 0) {
        int16_t interaction = operand->InteractionCallback;
        if (interaction == 254 && context->ActionTree) {
            // TS1(sims): FreeSO uses the active interaction (FreeSims: Queue[0]).
            if (context->Thread->ActiveAction() == nullptr) throw VMSimanticsException("ArgumentOutOfRangeException: Queue[0]", context);
            auto temp = context->Thread->ActiveAction()->InteractionNumber;
            if (temp == -1) throw VMSimanticsException("Set callback as 'this interaction' when queue item has no interaction number!", context);
            interaction = (int16_t)temp;
        } else if (interaction == 252 || interaction == 253) {
            interaction = context->Thread->TempRegisters[0];
        }
        //target is existing stack object. (where we get the interaction/tree from)
        VMActionCallback callback(context->VM(), interaction, context->StackObject, context->StackObject,
            context->Caller, true, (operand->InteractionCallback == 252));
        callback.Run(obj);
    }
    else context->StackObject = obj;

    return VMPrimitiveExitCode::GOTO_TRUE;
}

void VMCreateObjectInstanceOperand::Read(const uint8_t* bytes) {
    st::Reader io(bytes, OPERAND_SIZE, st::Endian::Little);
    GUID = io.u32();
    Position = (VMCreateObjectPosition)io.u8();
    Flags = io.u8();
    LocalToUse = io.u8();
    InteractionCallback = io.u8();
}

void VMCreateObjectInstanceOperand::Write(uint8_t* bytes) {
    st::Writer io(st::Endian::Little);
    io.WriteUInt32(GUID);
    io.WriteByte((uint8_t)Position);
    io.WriteByte(Flags);
    io.WriteByte(LocalToUse);
    io.WriteByte(InteractionCallback);
    auto& a = io.ToArray();
    std::memcpy(bytes, a.data(), std::min(a.size(), OPERAND_SIZE));
}

}}}
