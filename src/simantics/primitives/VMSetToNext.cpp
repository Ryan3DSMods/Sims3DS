/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Primitives.VMSetToNext (primitives/VMSetToNext.cs)
#include "simantics/primitives/VMSetToNext.h"
#include <cstring>
#include <vector>
#include "core/io.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMThread.h"
#include "simantics/engine/VMMemory.h"
#include "simantics/engine/VMSimanticsException.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMGameObject.h"
#include "simantics/entities/VMMultitileGroup.h"
#include "simantics/model/VMSetToNextCache.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/engine/VMTS1Neighbourhood.h"
#include "content/ContentManager.h"
#include "content/TS1NeighbourProvider.h"
#include "content/TS1JobProvider.h"

namespace FSO { namespace SimAntics { namespace Primitives {

using namespace FSO::SimAntics::Engine;
using FSO::SimAntics::Engine::Utils::VMMemory;
using FSO::LotView::Model::LotTilePos;

//position steps for object adjacent to object in local
const std::array<st::Point, 4> VMSetToNext::AdjStep =
{{
    st::Point(0, -1),
    st::Point(1, 0),
    st::Point(0, 1),
    st::Point(-1, 0),
}};

VMPrimitiveExitCode VMSetToNext::Execute(VMStackFrame* context, VMPrimitiveOperand* args)
{
    auto operand = static_cast<VMSetToNextOperand*>(args);
    auto targetValue = VMMemory::GetVariable(context, operand->TargetOwner, (int16_t)operand->TargetData);
    std::vector<VMEntity*>* entities = &context->VM()->Entities;

    VMEntity* Pointer = context->VM()->GetObjectById(targetValue);

    // FIX(sims): a search that starts over (target 0) after the thread already ran 20,000 primitives this tick is a tree
    // waiting for something to appear, e.g. the Old Town sax player / organ grinder 'Wander' looking for a non-NPC
    // audience (set to next person; at the end, start again) on a lot with no visitors yet. Wait for the next tick at
    // this instruction instead of spinning into the 500,000-primitive guard (374 resets per 3000 ticks on lot 71,
    // each a CPU hitch). Check trees (and RunInMyStack) are excluded; they keep their own limit.
    if (targetValue == 0 && !context->Thread->IsCheck && context->Thread->TicksThisFrame > VMThread::MAX_CHECK_LOOP_COUNT)
        return VMPrimitiveExitCode::CONTINUE_NEXT_TICK;

    //re-evaluation of what this actually does:
    //tries to find the next object id (from the previous) that meets a specific condition.
    //the previous object id is supplied via the target variable
    //
    //we should take the first result with object id > targetValue.

    if (operand->SearchType() == VMSetToNextSearchType::PartOfAMultipartTile) {
        auto target = context->VM()->GetObjectById(targetValue);
        if (target == nullptr || (!target->MultitileGroup->MultiTile)) return VMPrimitiveExitCode::GOTO_FALSE; //single part
        else
        {
            auto& group = target->MultitileGroup->Objects;
            bool found = false;
            int16_t bestID = 0;
            int16_t smallestID = 0;
            for (size_t i = 0; i < group.size(); i++)
            {
                auto temp = group[i];
                if (temp->ObjectID < smallestID || smallestID == 0) smallestID = temp->ObjectID;
                if (temp->ObjectID > targetValue)
                {
                    if ((!found) || (temp->ObjectID < bestID))
                    {
                        found = true;
                        bestID = temp->ObjectID;
                    }
                }
            }
            if (found)
            {
                VMMemory::SetVariable(context, operand->TargetOwner, (int16_t)operand->TargetData, bestID);
                return VMPrimitiveExitCode::GOTO_TRUE;
            }
            else
            {
                VMMemory::SetVariable(context, operand->TargetOwner, (int16_t)operand->TargetData, smallestID);
                return VMPrimitiveExitCode::GOTO_TRUE;
            }
        }
    }
    else if (operand->SearchType() == VMSetToNextSearchType::ObjectAdjacentToObjectInLocal)
    {
        VMEntity* anchor = context->VM()->GetObjectById((int16_t)context->Locals[operand->Local]);
        int ptrDir = -1;

        targetValue = 0;
        if (Pointer != nullptr)
        {
            if (anchor == nullptr) throw VMSimanticsException("Set To Next: adjacent anchor object is null", context); // C# NullReferenceException
            ptrDir = getAdjDir(anchor, Pointer);
            if (ptrDir == 3) return VMPrimitiveExitCode::GOTO_FALSE; //reached end
        }

        //iterate through all following dirs til we find an object
        for (int i = ptrDir + 1; i < 4; i++)
        {
            if (anchor == nullptr) throw VMSimanticsException("Set To Next: adjacent anchor object is null", context); // C# NullReferenceException
            auto off = AdjStep[i];
            auto adj = context->VM()->Context->SetToNextCache->GetObjectsAt(LotTilePos::FromBigTile(
                (int16_t)(anchor->Position().TileX() + off.X),
                (int16_t)(anchor->Position().TileY() + off.Y),
                anchor->Position().Level));

            if (adj != nullptr && adj->size() > 0)
            {
                //lists are ordered by object id. first is the smallest.
                VMMemory::SetVariable(context, operand->TargetOwner, (int16_t)operand->TargetData, (*adj)[0]->ObjectID);
                return VMPrimitiveExitCode::GOTO_TRUE;
            }
        }
        return VMPrimitiveExitCode::GOTO_FALSE;

    } else {

        // TS1(sims): The Sims 1 searches that walk the neighbourhood rather than the lot. FreeSims had the
        // provider calls commented out and returned 1/true, so "set random job" (Newspaper, Doll House, JobFinder)
        // looped until the instruction limit and reset. These run before (and independent of) the entity loop:
        // in the C# they sat inside it and were only reached when some lot object had an id > targetValue.
        if (context->VM()->TS1) {
            int16_t next = -1;
            bool handled = true;
            auto content = FSO::Content::Content::Get();
            auto hood = content->Neighborhood.get();
            switch (operand->SearchType())
            {
                case VMSetToNextSearchType::NeighborId: //next neighbour id
                    next = (hood == nullptr) ? (int16_t)-1 : hood->SetToNext(targetValue);
                    break;
                case VMSetToNextSearchType::NeighborOfType: //next neighbour id whose (default) object is of type GUID
                    next = (hood == nullptr) ? (int16_t)-1 : hood->SetToNext(targetValue, operand->GUID);
                    break;
                case VMSetToNextSearchType::Career: //next career (job type)
                    next = (!content->Jobs) ? (int16_t)-1 : content->Jobs->SetToNext(targetValue);
                    break;
                case VMSetToNextSearchType::ClosestHouse: //next house number, by distance from this lot
                    next = (hood == nullptr) ? (int16_t)-1 : hood->SetToNextClosestHouse(
                        context->VM()->GetGlobalValue(FSO::SimAntics::Engine::Utils::VMTS1Neighbourhood::GLOBAL_CURRENT_HOUSE), targetValue);
                    break;
                case VMSetToNextSearchType::FamilyMember: {
                    // next family member on the lot after the object in the target, wrapping around (the shipped
                    // trees start at an object and stop when they get back to it). Family = person data 61
                    // ("family number") of the target object, or of the caller.
                    auto famOf = [](VMEntity* e) -> int16_t {
                        auto a = dynamic_cast<VMAvatar*>(e);
                        return (a == nullptr) ? (int16_t)0 : a->GetPersonData((Model::VMPersonDataVariable)FSO::SimAntics::Engine::Utils::VMTS1Neighbourhood::PD_FAMILY_NUMBER);
                    };
                    int16_t family = famOf(Pointer);
                    if (family == 0) family = famOf(context->Caller);
                    VMEntity* firstFound = nullptr;
                    VMEntity* nextFound = nullptr;
                    if (family != 0) {
                        for (auto ent : context->VM()->Entities) { // sorted by object id
                            if (ent->Dead || dynamic_cast<VMAvatar*>(ent) == nullptr || famOf(ent) != family) continue;
                            if (firstFound == nullptr) firstFound = ent;
                            if (ent->ObjectID > targetValue) { nextFound = ent; break; }
                        }
                    }
                    if (nextFound == nullptr) nextFound = firstFound; //loop around
                    next = (nextFound == nullptr) ? (int16_t)-1 : nextFound->ObjectID;
                    break;
                }
                default:
                    handled = false;
                    break;
            }
            if (handled) {
                if (next < 0) return VMPrimitiveExitCode::GOTO_FALSE;
                VMMemory::SetVariable(context, operand->TargetOwner, (int16_t)operand->TargetData, next);
                return VMPrimitiveExitCode::GOTO_TRUE;
            }
        }

        //if we've cached the search type, use that instead of all objects
        switch (operand->SearchType())
        {
            case VMSetToNextSearchType::ObjectOnSameTile:
                // TS1(sims): FreeSO "if (Pointer == null) return GOTO_FALSE; // verified in ts1" (FreeSims threw; e.g.
                // Makin' Magic spookShowStage 'main' looks for objects on the tile of an actor id it remembered).
                if (Pointer == nullptr && context->VM()->TS1) return VMPrimitiveExitCode::GOTO_FALSE;
                if (Pointer == nullptr) throw VMSimanticsException("Set To Next: object on same tile of null object", context); // C# NullReferenceException
                entities = context->VM()->Context->SetToNextCache->GetObjectsAt(Pointer->Position()); break;
            case VMSetToNextSearchType::Person:
                entities = &context->VM()->Context->SetToNextCache->Avatars; break;
            case VMSetToNextSearchType::ObjectOfType:
                entities = context->VM()->Context->SetToNextCache->GetObjectsByGUID(operand->GUID); break;
            case VMSetToNextSearchType::ObjectWithCategoryEqualToSP0:
                entities = context->VM()->Context->SetToNextCache->GetObjectsByCategory(context->Args[0]); break;
            default: break;
        }
        if (entities == nullptr) return VMPrimitiveExitCode::GOTO_FALSE;

        bool loop = (operand->SearchType() == VMSetToNextSearchType::ObjectOnSameTile);
        VMEntity* first = nullptr;

        for (size_t i = 0; i < entities->size(); i++) //generic search through all objects
        {
            auto temp = (*entities)[i];
            bool found = false;
            if (temp->ObjectID > targetValue || loop)
            {
                switch (operand->SearchType())
                { //manual search types
                    case VMSetToNextSearchType::NonPerson:
                        found = (dynamic_cast<VMGameObject*>(temp) != nullptr); // C#: temp.GetType() == typeof(VMGameObject)
                        break;
                    case VMSetToNextSearchType::NeighborId:
                        //var next = Content.Content.Get().TS1Neighborhood.SetToNext(targetValue);
                        //if (next < 0) return VMPrimitiveExitCode.GOTO_FALSE;
                        VMMemory::SetVariable(context, operand->TargetOwner, (int16_t)operand->TargetData, 1);
                        return VMPrimitiveExitCode::GOTO_TRUE;
                    case VMSetToNextSearchType::ObjectWithCategoryEqualToSP0:
                        // TS1(sims): FreeSO takes the category list as is (the category is the object's Category
                        // attribute, Me[59], which the cache is keyed on). FreeSims also required FunctionFlags == SP0, so
                        // the phone never found its plugins (category 1): no "Go Downtown"/"Vacation"/"Studio Town"/...
                        if (context->VM()->TS1) { found = true; break; }
                        found = ((int32_t)temp->Object->OBJ->FunctionFlags == (int32_t)context->Args[0]); //I'm assuming that means "Stack parameter 0", that category means function and that it needs to be exactly the same (no subsets)
                        break;
                    case VMSetToNextSearchType::NeighborOfType:
                        //var nexta = Content.Content.Get().TS1Neighborhood.SetToNext(targetValue, operand.GUID);
                        //if (nexta < 0) return VMPrimitiveExitCode.GOTO_FALSE;
                        VMMemory::SetVariable(context, operand->TargetOwner, (int16_t)operand->TargetData, 1);
                        return VMPrimitiveExitCode::GOTO_TRUE;
                    case VMSetToNextSearchType::Career:
                        return VMPrimitiveExitCode::GOTO_TRUE;
                    case VMSetToNextSearchType::ClosestHouse:
                        return VMPrimitiveExitCode::GOTO_TRUE;
                    case VMSetToNextSearchType::FamilyMember:
                        return VMPrimitiveExitCode::GOTO_TRUE;
                    default:
                        //set to next object, or cached search.
                        found = true; break;
                }
                if (temp->ObjectID <= targetValue && found)
                {
                    //remember the first element in case we need to loop back to it (set to next tile on same location)
                    if (first == nullptr) first = temp;
                    found = false;
                }
            }
            if (found)
            {
                VMMemory::SetVariable(context, operand->TargetOwner, (int16_t)operand->TargetData, temp->ObjectID);
                return VMPrimitiveExitCode::GOTO_TRUE;
            }
        }

        if (loop)
        {
            if (first == nullptr) return VMPrimitiveExitCode::GOTO_FALSE; //no elements of this kind at all.
            else
            {
                VMMemory::SetVariable(context, operand->TargetOwner, (int16_t)operand->TargetData, first->ObjectID); //set to loop, so go back to lowest obj id.
                return VMPrimitiveExitCode::GOTO_TRUE;
            }
            //loop around
        }

    }
    return VMPrimitiveExitCode::GOTO_FALSE; //ran out of objects to test
}

int VMSetToNext::getAdjDir(VMEntity* src, VMEntity* dest)
{
    int diffX = dest->Position().TileX() - src->Position().TileX();
    int diffY = dest->Position().TileY() - src->Position().TileY();

    return getAdjDir(diffX, diffY);
}

int VMSetToNext::getAdjDir(int diffX, int diffY)
{

    //negative y is anchor
    //positive x is 90 degrees

    return (diffX == 0) ?
        ((diffY < 0) ? 0 : 2) :
        ((diffX < 0) ? 3 : 1);
}

void VMSetToNextOperand::Read(const uint8_t* bytes)
{
    st::Reader io(bytes, 8, st::Endian::Little);

    this->GUID = io.ReadUInt32();
    //132 was object of type
    this->Flags = io.ReadByte();
    this->TargetOwner = (Engine::Scopes::VMVariableScope)io.ReadByte();
    this->Local = io.ReadByte();
    this->TargetData = io.ReadByte();

    if ((Flags & 0x80) == 0)
    {
        //clobber this, we should always set flag for saving.
        Flags |= 0x80;
        TargetOwner = Engine::Scopes::VMVariableScope::StackObjectID;
        TargetData = 0;
    }
}

void VMSetToNextOperand::Write(uint8_t* bytes)
{
    st::Writer io(st::Endian::Little);
    io.WriteUInt32(GUID);
    io.WriteByte(Flags);
    io.WriteByte((uint8_t)TargetOwner);
    io.WriteByte(Local);
    io.WriteByte(TargetData);
    auto& data = io.ToArray();
    std::memcpy(bytes, data.data(), data.size() < 8 ? data.size() : 8);
}

}}}
