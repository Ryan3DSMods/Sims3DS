// FSO.SimAntics.Engine.Primitives.VMFindBestObjectForFunction (primitives/VMFindBestObjectForFunction.cs)
/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
#include "simantics/primitives/VMFindBestObjectForFunction.h"
#include <cstring>
#include <cmath>
#include <climits>
#include <memory>
#include <algorithm>
#include "core/io.h"
#include "world/model/LotTilePos.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMThread.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/model/VMBHAVOwnerPair.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"

namespace FSO { namespace SimAntics { namespace Primitives {
using namespace FSO::SimAntics::Engine;
using FSO::SimAntics::Model::VMStackObjectVariable;
using FSO::LotView::Model::LotTilePos;

std::unordered_set<uint32_t> VMFindBestObjectForFunction::SurfaceFunctions = {
    14, 18, 20, 25
};

const std::array<uint32_t, 15> VMFindBestObjectForFunction::FunctionToEntryPoint = {
    18, //prepare food
    19, //cook food
    20, //flat surface
    21, //dispose
    22, //eat
    23, //pick up from slot
    24, //wash dish
    25, //eating surface
    26, //sit
    27, //stand
    14, //serving surface
    28, //clean
    16, //gardening
    17, //wash hands
    29 //repair
};

const std::array<VMStackObjectVariable, 15> VMFindBestObjectForFunction::ScoreVar = {
    VMStackObjectVariable::PrepValue,
    VMStackObjectVariable::CookValue,
    VMStackObjectVariable::SurfaceValue,
    VMStackObjectVariable::DisposeValue,
    VMStackObjectVariable::Invalid,
    VMStackObjectVariable::Invalid,
    VMStackObjectVariable::WashDishValue,
    VMStackObjectVariable::EatingSurfaceValue,
    VMStackObjectVariable::Invalid, //sit, may score using comfort value?
    VMStackObjectVariable::Invalid,
    VMStackObjectVariable::ServingSurfaceValue,
    VMStackObjectVariable::Invalid,
    VMStackObjectVariable::GardeningValue,
    VMStackObjectVariable::WashHandsValue,
    VMStackObjectVariable::Invalid
};

VMPrimitiveExitCode VMFindBestObjectForFunction::Execute(VMStackFrame* context, VMPrimitiveOperand* args) {
    auto operand = static_cast<VMFindBestObjectForFunctionOperand*>(args);

    auto& entities = context->VM()->Entities;

    int32_t bestScore = INT_MIN;
    VMEntity* bestObj = nullptr;

    // C# array indexing throws IndexOutOfRangeException; .at() throws std::out_of_range.
    auto entry = VMFindBestObjectForFunction::FunctionToEntryPoint.at(operand->Function);
    for (size_t i = 0; i < entities.size(); i++) {
        auto ent = entities[i];
        if (ent->ObjectData[(int)VMStackObjectVariable::LockoutCount] > 0) continue; //this object is not important!!!
        if (ent->EntryPoints.at(entry).ActionFunction != 0) {
            // FIX(sims): Unleashed's Sycamore/Willow tree parts list their *init* tree ('init Bottom') as both the
            // condition and the action of 'wash hands' (OBJf entry 17). Evaluated as a check with me = the Sim, it ran
            // the tree's init on the Sim (placement flags lost 'on floor', movement/flags changed): after washing hands
            // nearby, Ginia Kat could not snap off the toilet ('number1' snap -> NotAllowedOnFloor) and starved upstairs.
            // A function whose condition is the object's own init tree is not a real function: skip it.
            if (ent->EntryPoints[entry].ConditionFunction != 0 && !ent->EntryPoints.empty()
                && ent->EntryPoints[entry].ConditionFunction == ent->EntryPoints[0].ActionFunction) continue;
            bool Execute;
            if (ent->EntryPoints[entry].ConditionFunction != 0) {

                auto Behavior = ent->GetBHAVWithOwner(ent->EntryPoints[entry].ConditionFunction, context->VM()->Context);
                if (Behavior != nullptr) {
                    auto frame = std::make_shared<VMStackFrame>();
                    frame->Caller = context->Caller;
                    frame->Callee = ent;
                    frame->CodeOwner = Behavior->owner;
                    frame->StackObject = ent;
                    frame->Routine = context->VM()->Assemble(Behavior->bhav);
                    frame->Args = std::vector<int16_t>(4, 0);
                    auto test = VMThread::EvaluateCheck(context->VM()->Context, context->Caller, frame);

                    Execute = (test == VMPrimitiveExitCode::RETURN_TRUE);
                } else Execute = true;

            } else if (SurfaceFunctions.count(entry) > 0) {
                //ts1: surface functions that have no check tree rely on the engine to check there is nothing in slot 0.
                Execute = ent->GetSlot(0) == nullptr;
            } else {

                Execute = true;
            }

            if (Execute) {
                if (ent->IsInUse(context->VM()->Context, true)) continue;

                //calculate the score for this object.
                int32_t score = 0;
                if (ScoreVar[operand->Function] != VMStackObjectVariable::Invalid) {
                    score = ent->GetValue(ScoreVar[operand->Function]);
                    if (ScoreVar[operand->Function] == VMStackObjectVariable::DirtyLevel && score < 800) continue; //only clean "dirty" things.
                }

                LotTilePos posDiff = ent->Position() - context->Caller->Position();
                score -= (int32_t)std::sqrt((double)(posDiff.x * posDiff.x + posDiff.y * posDiff.y + (posDiff.Level * posDiff.Level * 900 * 256))) / 3;

                if (score > bestScore) {
                    bestScore = score;
                    bestObj = ent;
                }
            }
        }
    }

    if (bestObj != nullptr) {
        context->StackObject = bestObj;
        return VMPrimitiveExitCode::GOTO_TRUE;
    } else return VMPrimitiveExitCode::GOTO_FALSE; //couldn't find an object! :'(
}

void VMFindBestObjectForFunctionOperand::Read(const uint8_t* bytes) {
    st::Reader io(bytes, OPERAND_SIZE, st::Endian::Little);
    Function = io.u16();
}

void VMFindBestObjectForFunctionOperand::Write(uint8_t* bytes) {
    st::Writer io(st::Endian::Little);
    io.WriteUInt16(Function);
    auto& a = io.ToArray();
    std::memcpy(bytes, a.data(), std::min(a.size(), OPERAND_SIZE));
}

}}}
