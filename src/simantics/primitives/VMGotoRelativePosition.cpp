/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims SimsVille/SimsAntics/primitives/VMGotoRelativePosition.cs
#include "simantics/primitives/VMGotoRelativePosition.h"
#include <cstring>
#include <memory>
#include <algorithm>
#include "core/io.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMThread.h"
#include "simantics/engine/VMRoutingFrame.h"
#include "simantics/engine/VMSimanticsException.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMAvatar.h"

namespace FSO { namespace SimAntics { namespace Primitives {

using namespace FSO::SimAntics::Engine;
using FSO::LotView::Model::LotTilePos;
using FSO::Files::Formats::IFF::Chunks::SLOTItem;
using FSO::Files::Formats::IFF::Chunks::SLOTFlags;
using FSO::Files::Formats::IFF::Chunks::SLOTFacing;

VMPrimitiveExitCode VMGotoRelativePosition::Execute(VMStackFrame* context, VMPrimitiveOperand* args)
{
    auto operand = static_cast<VMGotoRelativePositionOperand*>(args);

    if (context->Thread->IsCheck) return VMPrimitiveExitCode::GOTO_FALSE;

    auto obj = context->StackObject;
    if (obj == nullptr) return VMPrimitiveExitCode::GOTO_FALSE;
    // C#: var avatar = (VMAvatar)context.Caller; (unused, but the cast throws InvalidCastException for non-avatars)
    auto avatar = dynamic_cast<VMAvatar*>(context->Caller);
    if (context->Caller != nullptr && avatar == nullptr)
        throw VMSimanticsException("Specified cast is not valid. (VMAvatar)", context);
    (void)avatar;

    if (obj->Position() == LotTilePos::OUT_OF_WORLD) return VMPrimitiveExitCode::GOTO_FALSE;

    // C# `new SLOTItem { ... }` stays alive through the routing frame's Slot reference; here the frame owns it.
    auto slotOwner = std::unique_ptr<SLOTItem>(new SLOTItem());
    auto& slot = *slotOwner;
    slot.Type = 3;
    slot.Standing = 1;

    // C# `1 << n` masks the shift count to 5 bits (Location % 8 may be negative).
    auto locBit = (int32_t)(1u << ((((int32_t)operand->Location) % 8) & 31));
    if (operand->Location != VMGotoRelativeLocation::OnTopOf) { //default slot is on top of
        slot.MinProximity = 16;
        slot.MaxProximity = 32; //diamond shaped
        slot.OptimalProximity = 16;
        if (operand->Location == VMGotoRelativeLocation::AnywhereNear) slot.Rsflags |= (SLOTFlags)255;
        else slot.Rsflags |= (SLOTFlags)locBit;
    }
    else
    {
        slot.MinProximity = 16;
        slot.MaxProximity = 24;
        slot.Rsflags |= (SLOTFlags)locBit;
    }

    if (operand->Direction == VMGotoRelativeDirection::AnyDirection) slot.Facing = SLOTFacing::FaceAnywhere; //TODO: verify. not sure where this came from?
    else slot.Facing = (SLOTFacing)operand->Direction;

    auto pathFinder = context->Thread->PushNewRoutingFrame(context, !operand->NoFailureTrees());
    pathFinder->OwnedSlot = std::move(slotOwner);
    auto success = pathFinder->InitRoutes(pathFinder->OwnedSlot.get(), context->StackObject);
    (void)success;

    return VMPrimitiveExitCode::CONTINUE;
}

// region VMPrimitiveOperand Members
void VMGotoRelativePositionOperand::Read(const uint8_t* bytes)
{
    st::Reader io(bytes, VMPrimitiveOperand::OPERAND_SIZE, st::Endian::Little);
    OldTrapCount = io.ReadUInt16();
    Location = (VMGotoRelativeLocation)((int8_t)io.ReadByte());
    Direction = (VMGotoRelativeDirection)((int8_t)io.ReadByte());
    RouteCount = io.ReadUInt16();
    Flags = (VMGotoRelativeFlags)io.ReadByte();
}

void VMGotoRelativePositionOperand::Write(uint8_t* bytes)
{
    st::Writer io(st::Endian::Little);
    io.WriteUInt16(OldTrapCount);
    io.WriteByte((uint8_t)Location);
    io.WriteByte((uint8_t)Direction);
    io.WriteUInt16(RouteCount);
    io.WriteByte((uint8_t)Flags);
    std::memcpy(bytes, io.ToArray().data(), std::min(io.ToArray().size(), VMPrimitiveOperand::OPERAND_SIZE));
}
// endregion

}}}
