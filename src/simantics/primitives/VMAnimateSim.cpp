/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims SimsVille/SimsAntics/primitives/VMAnimateSim.cs
#include "simantics/primitives/VMAnimateSim.h"
#include <cstring>
#include <memory>
#include <stdexcept>
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMMemory.h"
#include "simantics/engine/VMSimanticsException.h"
#include "simantics/engine/scopes/VMVariableScope.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/model/VMAnimationState.h"
#include "simantics/VM.h"
#include "simantics/model/VMStackObjectVariable.h"
#include "simantics/model/VMPersonDataVariable.h"
#include "simantics/model/VMPersonDataVariable.h"
#include "content/ContentManager.h"
#include "vitaboy/Animation.h"
#include "vitaboy/Appearance.h"
#include "core/io.h"

namespace FSO { namespace SimAntics { namespace Primitives {
using namespace FSO::SimAntics::Engine;
using namespace FSO::SimAntics::Engine::Utils;
using namespace FSO::SimAntics::Engine::Scopes;
using namespace FSO::SimAntics::Model;
using FSO::Vitaboy::Animation;
using FSO::Vitaboy::SimHandGesture;

namespace {
// C# `new VMAnimationState(animation, ...)` dereferences the animation; a null one threw a NullReferenceException.
std::shared_ptr<VMAnimationState> NewAnimState(Animation* animation, bool backwards, VMStackFrame* context) {
    if (animation == nullptr) throw VMSimanticsException("Animate Sim: animation not found.", context);
    return std::make_shared<VMAnimationState>(animation, backwards);
}
}

VMPrimitiveExitCode VMAnimateSim::Execute(VMStackFrame* context, VMPrimitiveOperand* args) {
    auto operand = static_cast<VMAnimateSimOperand*>(args);
    auto avatar = dynamic_cast<VMAvatar*>(context->Caller); // C#: (VMAvatar)context.Caller
    if (avatar == nullptr) throw VMSimanticsException("Animate Sim: caller is not an avatar (invalid cast).", context);

    Animation* animation = nullptr;

    // TS1(sims): FreeSO — the animation id can come from a parameter.
    uint16_t id = operand->IDFromParam() ? (uint16_t)context->Args.at(operand->AnimationID) : operand->AnimationID;
    // TS1(sims): FreeSO plays TS1 animations at 30/25 speed (and twice that when hurrying).
    float ts1Speed = context->VM()->TS1 ? 30 / 25.0f : 1.0f;
    auto speedFor = [&](std::shared_ptr<VMAnimationState>& st) {
        st->Speed = ts1Speed;
        if (avatar->GetValue(VMStackObjectVariable::WalkStyle) == 1 && operand->Hurryable()) st->Speed *= 2;
    };

    if (id == 0) { //reset
        if (context->VM()->TS1 && operand->Mode() == 3) { avatar->CarryAnimationState = nullptr; return VMPrimitiveExitCode::GOTO_TRUE; } // FreeSO
        if (avatar->GetPersonData(VMPersonDataVariable::HeadSeekState) == 1) avatar->SetPersonData(VMPersonDataVariable::HeadSeekState, 4); // FreeSO
        avatar->Animations.clear();
        auto posture = avatar->GetPersonData(VMPersonDataVariable::Posture);

        if (posture != 1 && posture != 2) posture = 3; //sit and kneel are 1 and 2, 0 is stand but in walk animations it's 3.
        //todo: swimming??

        animation = FSO::Content::Content::Get()->AvatarAnimations->Get(avatar->WalkAnimations[posture] + ".anim");
        if (animation == nullptr && context->VM()->TS1) return VMPrimitiveExitCode::GOTO_TRUE; // FreeSO
        auto state = NewAnimState(animation, operand->PlayBackwards(), context);
        state->Speed = ts1Speed;
        state->Loop = true;
        avatar->Animations.push_back(state);

        if (avatar->GetSlot(0) != nullptr) //if we're carrying something, set carry animation to default carry.
        {
            avatar->CarryAnimationState = NewAnimState(FSO::Content::Content::Get()->AvatarAnimations->Get("a2o-rarm-carry-loop.anim"), false, context);
        }
        else avatar->CarryAnimationState = nullptr;
        return VMPrimitiveExitCode::GOTO_TRUE;
    }

    auto source = operand->Source;
    animation = VMMemory::GetAnimation(context, source, id);
    if (animation == nullptr) {
        // TS1(sims): FreeSO returns true *next tick*. Returning at once let loops around a missing animation
        // (child Toy Box 'Play' -> global 'Exit fun object') spin 500k primitives in one tick and reset the Sim.
        return context->VM()->TS1 ? VMPrimitiveExitCode::GOTO_TRUE_NEXT_TICK : VMPrimitiveExitCode::GOTO_TRUE;
    }

    if (operand->Mode() == 1 && context->VM()->TS1) { // TS1(sims): FreeSO mode 1 — start a looping animation, don't wait
        avatar->Animations.clear();
        auto state = NewAnimState(animation, operand->PlayBackwards(), context);
        speedFor(state);
        state->Loop = true;
        avatar->Animations.push_back(state);
        if (avatar->Avatar != nullptr) {
            avatar->Avatar->LeftHandGesture = SimHandGesture::Idle;
            avatar->Avatar->RightHandGesture = SimHandGesture::Idle;
        }
        return VMPrimitiveExitCode::GOTO_TRUE;
    }

    if (operand->Mode() == 3) //stop standard carry, then play and wait
        avatar->CarryAnimationState = nullptr;

    if (operand->Mode() == 0 || operand->Mode() == 3) //Play and Wait
    {
        /** Are we starting the animation or progressing it? **/
        if (avatar->CurrentAnimationState() == nullptr || avatar->CurrentAnimationState()->Anim != animation)
        {
            /** Start it **/
            avatar->Animations.clear();
            auto astate = NewAnimState(animation, operand->PlayBackwards(), context);
            speedFor(astate);
            avatar->Animations.push_back(astate);

            if (avatar->Avatar != nullptr) {
                avatar->Avatar->LeftHandGesture = SimHandGesture::Idle;
                avatar->Avatar->RightHandGesture = SimHandGesture::Idle;
            }
            return VMPrimitiveExitCode::CONTINUE_NEXT_TICK;
        }
        else
        {
            auto cAnim = avatar->CurrentAnimationState();

            //SPECIAL CASE: if we are ending the animation, and the number of events run < expected events
            //forcefully run those events, with id as their event number. (required for bath drain)
            if (cAnim->EndReached)
            {
                while (cAnim->EventsRun < operand->ExpectedEventCount)
                {
                    cAnim->EventQueue.push_back(cAnim->EventsRun++);
                }
            }

            if (cAnim->EventQueue.size() > 0) //favor events over end. do not want to miss any.
            {
                auto code = cAnim->EventQueue[0];
                cAnim->EventQueue.erase(cAnim->EventQueue.begin());
                if (operand->StoreFrameInLocal())
                    VMMemory::SetVariable(context, VMVariableScope::Local, operand->LocalEventNumber, code);
                else
                    VMMemory::SetVariable(context, VMVariableScope::Parameters, 0, code);
                return VMPrimitiveExitCode::GOTO_FALSE;
            }
            else if (cAnim->EndReached)
            {
                avatar->Animations.clear();
                return VMPrimitiveExitCode::GOTO_TRUE;
            }
            else
            {
                return VMPrimitiveExitCode::CONTINUE_NEXT_TICK;
            }
        }
    }
    else if (operand->Mode() == 2) //set custom carry animation
    {
        avatar->CarryAnimationState = NewAnimState(animation, false, context);
        return VMPrimitiveExitCode::GOTO_TRUE;
    }
    else return VMPrimitiveExitCode::GOTO_TRUE;
}

void VMAnimateSimOperand::Read(const uint8_t* bytes) {
    st::Reader io(bytes, 8, st::Endian::Little);
    AnimationID = io.u16();
    LocalEventNumber = io.u8();
    _pad = io.u8();
    Source = (VMAnimationScope)io.u8();
    Flags = io.u8();
    ExpectedEventCount = io.u8();
}

void VMAnimateSimOperand::Write(uint8_t* bytes) {
    // C#: BinaryWriter over new MemoryStream(bytes) (little endian, fixed 8 bytes)
    st::Writer io(st::Endian::Little);
    io.WriteUInt16(AnimationID);
    io.WriteByte(LocalEventNumber);
    io.WriteByte((uint8_t)0);
    io.WriteByte((uint8_t)Source);
    io.WriteByte(Flags);
    io.WriteByte(ExpectedEventCount);
    auto& buf = io.ToArray();
    if (buf.size() > 8) throw std::runtime_error("Memory stream is not expandable.");
    std::memcpy(bytes, buf.data(), buf.size());
}

std::string VMAnimateSimOperand::ToString() const {
    // C# Enum.ToString(): the member name, or the number when undefined.
    std::string src;
    switch (Source) {
        case VMAnimationScope::Object: src = "Object"; break;
        case VMAnimationScope::Global: src = "Global"; break;
        case VMAnimationScope::PersonStock: src = "PersonStock"; break;
        case VMAnimationScope::Misc: src = "Misc"; break;
        default: src = std::to_string((int)Source); break;
    }
    return "Animate Sim (id " + std::to_string(AnimationID) + " from " + src + ")";
}

}}}
