/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Primitives.VMReach (primitives/VMReach.cs)
#include "simantics/primitives/VMReach.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include "core/io.h"
#include "content/ContentManager.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMSimanticsException.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/model/VMAnimationState.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"

namespace FSO { namespace SimAntics { namespace Primitives {
using namespace FSO::SimAntics::Engine;
using FSO::Files::Formats::IFF::Chunks::SLOT;
using FSO::Files::Formats::IFF::Chunks::SLOTItem;

namespace {
// C# `entity.Slots.Slots[0][index]` (Dictionary + List indexers): throws instead of reading out of range.
SLOTItem* SlotZeroAt(VMEntity* entity, int32_t index, VMStackFrame* context)
{
    if (entity->Slots == nullptr) throw VMSimanticsException("Object reference not set to an instance of an object.", context);
    auto it = entity->Slots->Slots.find(0);
    if (it == entity->Slots->Slots.end()) throw VMSimanticsException("The given key was not present in the dictionary.", context);
    if (index < 0 || index >= (int32_t)it->second.size()) throw VMSimanticsException("Index was out of range.", context);
    return &it->second[index];
}

// C# (int)Math.Round((slot.Height != 5) ? SLOT.HeightOffsets[slot.Height - 1] : slot.Offset.Z)
int32_t SlotHeight(SLOTItem* slot, VMStackFrame* context)
{
    double value;
    if (slot->Height != 5)
    {
        if (slot->Height - 1 < 0 || slot->Height - 1 >= 9) throw VMSimanticsException("Index was outside the bounds of the array.", context);
        value = SLOT::HeightOffsets[slot->Height - 1];
    }
    else value = slot->Offset.Z;
    return (int32_t)std::nearbyint(value); // Math.Round: round half to even
}
}

VMPrimitiveExitCode VMReach::Execute(VMStackFrame* context, VMPrimitiveOperand* args)
{
    auto operand = static_cast<VMReachOperand*>(args);
    auto completedPickup = false;

    int32_t height;

    if (operand->Mode == 0)
    { //reach to stack object
        // FIX(sims) QA4: the object was deleted while the Sim walked to it (another Sim ate / bussed / cleaned the same
        // meal: FoodHD 'get food' routes to the plate, then reaches): fail the reach instead of resetting the Sim. The
        // trees branch on false ('get food' -> false -> 'drop food'). FreeSO/FreeSims would throw here.
        if (context->StackObject == nullptr && context->VM()->TS1) return VMPrimitiveExitCode::GOTO_FALSE;
        if (context->StackObject == nullptr) throw VMSimanticsException("Object reference not set to an instance of an object.", context);
        auto container = context->StackObject->Container;
        if (container == context->Caller) completedPickup = true;
        if (container == nullptr || dynamic_cast<VMAvatar*>(container) != nullptr)
        {
            height = 0;
        }
        else
        {
            auto slot = SlotZeroAt(container, context->StackObject->ContainerSlot, context);
            if (slot != nullptr)
            {
                height = SlotHeight(slot, context);
            }
            else height = 0;
        }
       // height = 4; //todo: get slot height
    }
    else if (operand->Mode == 1)
    {
        if (context->StackObject == nullptr && context->VM()->TS1) return VMPrimitiveExitCode::GOTO_FALSE; // FIX(sims) QA4: see above
        if (context->StackObject == nullptr) throw VMSimanticsException("Object reference not set to an instance of an object.", context);
        auto slotNum = context->Args.at(operand->SlotParam);
        auto slot = SlotZeroAt(context->StackObject, slotNum, context);
        if (slot != nullptr)
        {
            height = SlotHeight(slot, context);
        }
        else return VMPrimitiveExitCode::GOTO_FALSE;
    }
    else
    {
        //reach to mouth is unimplemented so no, also none others exist after
        throw VMSimanticsException("Reach to mouth not implemented!", context);
    }

    std::string animationName;
    if (height < 2) animationName = "a2o-reach-floorht.anim";
    else if (height < 4) animationName = "a2o-reach-seatht.anim";
    else animationName = "a2o-reach-tableht.anim";

    auto animation = FSO::Content::Content::Get()->AvatarAnimations->Get(animationName);
    if (animation == nullptr) {
        return VMPrimitiveExitCode::ERROR;
    }
    auto avatar = dynamic_cast<VMAvatar*>(context->Caller); // C#: (VMAvatar)context.Caller
    if (avatar == nullptr) throw VMSimanticsException("Specified cast is not valid.", context);

    /** Are we starting the animation or progressing it? **/
    if (avatar->CurrentAnimationState() == nullptr || (avatar->CurrentAnimationState()->Anim != animation && !completedPickup))
    { //start the grab!

        /** Start it **/

        avatar->Animations.clear();
        avatar->Animations.push_back(std::make_shared<Model::VMAnimationState>(animation, false));

        avatar->Avatar->LeftHandGesture = FSO::Vitaboy::SimHandGesture::Idle;
        avatar->Avatar->RightHandGesture = FSO::Vitaboy::SimHandGesture::Idle;
        failed = false;
        return VMPrimitiveExitCode::CONTINUE_NEXT_TICK;
    }
    else
    {
        if (avatar->CurrentAnimationState()->EndReached)
        {
            avatar->Animations.clear();
            return VMPrimitiveExitCode::GOTO_TRUE;
        }
        else if (avatar->CurrentAnimationState()->EventQueue.size() > 0)
        {

            if (avatar->CurrentAnimationState()->EventQueue[0] == 0)
            {
                //do the grab/drop
                if (operand->Mode == 0)
                { //pick up stack object. no drop condition
                    if (context->Caller->GetSlot(0) == nullptr)
                    {
                        context->Caller->PlaceInSlot(context->StackObject, 0, true, context->VM()->Context);
                    }
                    else
                    {
                        failed = true;
                    }
                }
                else if (operand->Mode == 1)
                { //grab or drop, depending on if we're holding something
                    auto holding = context->Caller->GetSlot(0);
                    auto slotNum = context->Args.at(operand->SlotParam);

                    if (holding == nullptr)
                    { //grab
                        auto item = context->StackObject->GetSlot(slotNum);
                        if (item != nullptr)
                        {
                            if (!context->Caller->PlaceInSlot(item, 0, true, context->VM()->Context)) failed = true;
                        }
                        else failed = true; //can't grab from an empty space
                    }
                    else //drop
                    {
                        auto itemTest = context->StackObject->GetSlot(slotNum);
                        if (itemTest == nullptr)
                        {
                            if (!context->StackObject->PlaceInSlot(holding, slotNum, true, context->VM()->Context)) failed = true;
                        }
                        else failed = true; //can't drop in an occupied space
                    }
                }
            }
            // C# re-evaluates avatar.CurrentAnimationState here too (the grab/drop above may have touched the animation list)
            if (avatar->CurrentAnimationState() == nullptr) throw VMSimanticsException("Object reference not set to an instance of an object.", context);
            auto& queue = avatar->CurrentAnimationState()->EventQueue;
            if (queue.empty()) throw VMSimanticsException("Index was out of range.", context);
            queue.erase(queue.begin());
            return VMPrimitiveExitCode::CONTINUE_NEXT_TICK;
        }
        else
        {
            return VMPrimitiveExitCode::CONTINUE_NEXT_TICK;
        }
    }
}

void VMReachOperand::Read(const uint8_t* bytes)
{
    st::Reader io(bytes, 8, st::Endian::Little);
    Mode = io.u16();
    GrabOrDrop = io.u16();
    SlotParam = io.u16();
}

void VMReachOperand::Write(uint8_t* bytes)
{
    st::Writer io(st::Endian::Little);
    io.WriteUInt16(Mode);
    io.WriteUInt16(GrabOrDrop);
    io.WriteUInt16(SlotParam);
    auto& buf = io.ToArray();
    std::copy(buf.begin(), buf.begin() + std::min<size_t>(buf.size(), OPERAND_SIZE), bytes);
}

}}}
