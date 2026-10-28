/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Primitives.VMSnap (primitives/VMSnap.cs)
#include "simantics/primitives/VMSnap.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include "core/io.h"
#include "common/utils/DirectionUtils.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMMemory.h"
#include "simantics/engine/VMSlotParser.h"
#include "simantics/engine/VMSimanticsException.h"
#include "simantics/engine/scopes/VMSlotScope.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/entities/VMMultitileGroup.h"
#include "simantics/engine/VMThread.h"
#include "simantics/engine/VMQueuedAction.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"

namespace FSO { namespace SimAntics { namespace Primitives {
using namespace FSO::SimAntics::Engine;
using FSO::SimAntics::Engine::Utils::VMMemory;
using FSO::SimAntics::Engine::Scopes::VMSlotScope;
using FSO::Files::Formats::IFF::Chunks::SLOTItem;
using FSO::Files::Formats::IFF::Chunks::SLOTFlags;
using FSO::LotView::Model::LotTilePos;
using FSO::Common::Utils::DirectionUtils;
using DirectionT = FSO::LotView::Model::Direction;

VMPrimitiveExitCode VMSnap::Execute(VMStackFrame* context, VMPrimitiveOperand* args)
{
    auto operand = static_cast<VMSnapOperand*>(args);
    auto avatar = context->Caller; //todo, can sometimes be an object?? see roaches object tile movement, snaps to its own routing slot
    auto obj = context->StackObject;

    SLOTItem* slot = nullptr;
    SLOTItem inFrontSlot; // storage for the C# `new SLOTItem { ... }` below

    switch (operand->Mode)
    {
        case VMSnapSlotScope::StackVariable:
            slot = VMMemory::GetSlot(context, VMSlotScope::StackVariable, operand->Index);
            break;
        case VMSnapSlotScope::BeContained:
            if (context->StackObject == nullptr) throw VMSimanticsException("Object reference not set to an instance of an object.", context);
            return (context->StackObject->PlaceInSlot(context->Caller, 0, true, context->VM()->Context)) ? VMPrimitiveExitCode::GOTO_TRUE : VMPrimitiveExitCode::GOTO_FALSE;
        case VMSnapSlotScope::InFront:
            inFrontSlot.Type = 3;
            inFrontSlot.Standing = 1;
            inFrontSlot.MinProximity = 16;
            inFrontSlot.Rsflags = SLOTFlags::NORTH;
            slot = &inFrontSlot;
            break;
        case VMSnapSlotScope::Literal:
            slot = VMMemory::GetSlot(context, VMSlotScope::Literal, operand->Index);
            break;
        case VMSnapSlotScope::Global:
            slot = VMMemory::GetSlot(context, VMSlotScope::Global, operand->Index);
            break;
    }

    if (operand->Mode != VMSnapSlotScope::BeContained)
    {
        // TS1(sims): FreeSO — no such slot: false (FreeSims threw; e.g. Hot Date 'Flies' snap past the slot table).
        if (slot == nullptr) return VMPrimitiveExitCode::GOTO_FALSE;
        VMSlotParser parser(slot);
        auto locations = parser.FindAvaliableLocations(obj, context->VM()->Context, avatar);
        if (slot->SnapTargetSlot > -1)
        {
            if (!context->StackObject->PlaceInSlot(context->Caller, slot->SnapTargetSlot, true, context->VM()->Context)) return VMPrimitiveExitCode::GOTO_FALSE;
            if (locations.size() > 0) avatar->RadianDirection(((slot->Rsflags & SLOTFlags::SnapToDirection) > 0) ? locations[0].RadianDirection : avatar->RadianDirection());
        }
        else
        {
            if (locations.size() > 0)
            {
                if (!SetPosition(avatar, locations[0].Position,
                    ((slot->Rsflags & SLOTFlags::SnapToDirection) > 0) ? locations[0].RadianDirection : avatar->RadianDirection(),
                    context->VM()->Context))
                {
                    // FIX(sims): FreeSO - the 'shoo' flag (2) pushes Shoo (GOTO object 0x7C4, interaction 3) onto a Sim
                    // standing where we snap to, unless one is already queued. Without it a Sim snapping out of the
                    // toilet ('number1') retried forever when a visitor waited in front of it: Ginia Kat (Old Town 56)
                    // stayed upstairs on the toilet and starved.
                    if ((operand->Flags & 2) != 0) {
                        auto blocker = dynamic_cast<VMAvatar*>(context->VM()->GetObjectById(avatar->GetValue(Model::VMStackObjectVariable::PrimitiveResultID)));
                        if (blocker != nullptr && blocker != avatar && blocker->Thread != nullptr) {
                            bool shooed = false;
                            for (auto& q : blocker->Thread->Queue)
                                if (q->Callee != nullptr && q->Callee->Object->GUID == 0x000007C4 && q->InteractionNumber == 3) shooed = true;
                            if (!shooed) {
                                auto grp = context->VM()->Context->CreateObjectInstance(0x000007C4, LotTilePos(blocker->Position()), DirectionT::NORTH, false);
                                if (grp != nullptr && !grp->Objects.empty()) grp->Objects[0]->PushUserInteraction(3, blocker, context->VM()->Context);
                            }
                        }
                    }
                    return VMPrimitiveExitCode::GOTO_FALSE;
                }
            }
            else
            {
                // FIX(sims) H1: FreeSO - "no valid goals" answers primitive result 2. CarGlobals 'drive away' ends when its
                // snap forward reports 2 (the car left the lot); without it parked cars honked at the lot edge for hours
                // (forever when they started at 22:00-23:59: 'hour > start hour + 1' never comes) and jammed the curb.
                if (parser.FailCode == Model::Routing::VMRouteFailCode::NoValidGoals) avatar->SetValue(Model::VMStackObjectVariable::PrimitiveResult, 2);
                avatar->SetValue(Model::VMStackObjectVariable::PrimitiveResultID, (parser.Blocker == nullptr) ? (int16_t)0 : parser.Blocker->ObjectID);
                return VMPrimitiveExitCode::GOTO_FALSE;
            }
        }
    }
    return VMPrimitiveExitCode::GOTO_TRUE;
}

bool VMSnap::SetPosition(VMEntity* entity, const LotTilePos& pos, DirectionT dir, VMContext* context)
{
    // C# Math.Round is round-half-to-even: std::nearbyint under the default rounding mode.
    return SetPosition(entity, pos, (float)(std::nearbyint(std::log((double)(int)dir) / std::log(2.0)) * (M_PI / 4)), context);
}

bool VMSnap::SetPosition(VMEntity* entity, const LotTilePos& pos, float radDir, VMContext* context)
{
    auto posChange = entity->SetPosition(pos, (DirectionT)(1 << ((int32_t)std::nearbyint(DirectionUtils::PosMod(radDir, (double)((float)M_PI * 2)) / (M_PI / 4)) % 8)), context);
    if (posChange.Status != Model::VMPlacementError::Success)
    {
        // FIX(sims) H1: FreeSO - moving off the lot answers primitive result 2 ('drive away': the car is gone).
        if (posChange.Status == Model::VMPlacementError::LocationOutOfBounds) entity->SetValue(Model::VMStackObjectVariable::PrimitiveResult, 2);
        entity->SetValue(Model::VMStackObjectVariable::PrimitiveResultID, (posChange.Object == nullptr) ? (int16_t)0 : posChange.Object->ObjectID);
        return false;
    }
    if (dynamic_cast<VMAvatar*>(entity) != nullptr) entity->RadianDirection(radDir);
    return true;
}

void VMSnapOperand::Read(const uint8_t* bytes)
{
    st::Reader io(bytes, 8, st::Endian::Little);
    Index = io.u16();
    Mode = (VMSnapSlotScope)io.u16();
    Flags = io.u8();
}

void VMSnapOperand::Write(uint8_t* bytes)
{
    st::Writer io(st::Endian::Little);
    io.WriteUInt16(Index);
    io.WriteUInt16((uint16_t)Mode);
    io.WriteByte(Flags);
    auto& buf = io.ToArray();
    std::copy(buf.begin(), buf.begin() + std::min<size_t>(buf.size(), OPERAND_SIZE), bytes);
}

}}}
