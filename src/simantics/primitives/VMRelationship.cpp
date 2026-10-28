/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Primitives.VMRelationship (primitives/VMRelationship.cs)
#include "simantics/primitives/VMRelationship.h"
#include <algorithm>
#include <vector>
#include "core/io.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMMemory.h"
#include "simantics/engine/VMSimanticsException.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/VM.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/engine/VMTS1Neighbourhood.h"

namespace FSO { namespace SimAntics { namespace Primitives {
using namespace FSO::SimAntics::Engine;
using FSO::SimAntics::Engine::Utils::VMMemory;

VMPrimitiveExitCode VMRelationship::Execute(VMStackFrame* context, VMPrimitiveOperand* args)
{
    auto operand = static_cast<VMRelationshipOperand*>(args);
    const bool isOld = dynamic_cast<VMOldRelationshipOperand*>(operand) != nullptr; // C#: operand is VMOldRelationshipOperand

    // TS1(sims): The Sims 1 keeps person relationships in the neighbourhood (NBRS, keyed by neighbour id),
    // and flag bit 1 (0x02) means "the ids are neighbour ids" (FreeSims tested bit 3 (0x08) for the new
    // operand, which no shipped BHAV sets; the old operand used bit 1). The shipped trees use 0x02 for
    // neighbour loops: 'get nth friend', 'init relationships', 'Make me know everyone', phone 'adjust rel for talk'.
    if (context->VM()->TS1) {
        const bool useNeighbor = isOld ? operand->UseNeighbor() : ((operand->Flags & 2) == 2);
        std::vector<int16_t>* rel = nullptr;
        bool neighbourRel = false;
        int16_t n1 = 0, n2 = 0;
        if (useNeighbor) {
            switch (operand->Mode)
            {
                case 0: n1 = FSO::SimAntics::Engine::Utils::VMTS1Neighbourhood::NeighborIDOf(context->Caller); n2 = context->StackObjectID(); break;
                case 1: n1 = context->StackObjectID(); n2 = FSO::SimAntics::Engine::Utils::VMTS1Neighbourhood::NeighborIDOf(context->Caller); break;
                case 2: n1 = context->StackObjectID(); n2 = isOld ? context->Args.at(0) : context->Locals.at(operand->Local); break;
                case 3: n1 = isOld ? context->Args.at(0) : context->Locals.at(operand->Local); n2 = context->StackObjectID(); break;
                default: throw VMSimanticsException("Invalid relationship type!", context);
            }
            neighbourRel = true;
        } else {
            VMEntity* o1 = nullptr; VMEntity* o2 = nullptr;
            switch (operand->Mode)
            {
                case 0: o1 = context->Caller; o2 = context->StackObject; break;
                case 1: o1 = context->StackObject; o2 = context->Caller; break;
                case 2: o1 = context->StackObject; o2 = context->VM()->GetObjectById(isOld ? context->Args.at(0) : context->Locals.at(operand->Local)); break;
                case 3: o1 = context->VM()->GetObjectById(isOld ? context->Args.at(0) : context->Locals.at(operand->Local)); o2 = context->StackObject; break;
                default: throw VMSimanticsException("Invalid relationship type!", context);
            }
            n1 = FSO::SimAntics::Engine::Utils::VMTS1Neighbourhood::NeighborIDOf(o1); n2 = FSO::SimAntics::Engine::Utils::VMTS1Neighbourhood::NeighborIDOf(o2);
            // two neighbours on the lot: use their persistent neighbourhood relationship.
            neighbourRel = (n1 > 0 && n2 > 0 && FSO::SimAntics::Engine::Utils::VMTS1Neighbourhood::GetNeighbor(n1) != nullptr && FSO::SimAntics::Engine::Utils::VMTS1Neighbourhood::GetNeighbor(n2) != nullptr);
        }
        if (neighbourRel) {
            rel = FSO::SimAntics::Engine::Utils::VMTS1Neighbourhood::GetRelationship(n1, n2, false);
            if (rel == nullptr)
            {
                if (operand->FailIfTooSmall()) return VMPrimitiveExitCode::GOTO_FALSE;
                rel = FSO::SimAntics::Engine::Utils::VMTS1Neighbourhood::GetRelationship(n1, n2, true);
                if (rel == nullptr) return VMPrimitiveExitCode::GOTO_FALSE; // n1 is not a neighbour
            }
            if (rel->size() <= operand->RelVar)
            {
                if (operand->FailIfTooSmall()) return VMPrimitiveExitCode::GOTO_FALSE;
                else while (rel->size() <= operand->RelVar) rel->push_back(0);
            }
            if (operand->SetMode() == 0)
                VMMemory::SetVariable(context, operand->VarScope, operand->VarData, (*rel)[operand->RelVar]);
            // TS1(sims): FreeSO clamps set/add to -100..100 (the trees add deltas without checking the bounds).
            else if (operand->SetMode() == 1)
                (*rel)[operand->RelVar] = (int16_t)std::max(-100, std::min(100, (int)VMMemory::GetVariable(context, operand->VarScope, operand->VarData)));
            else if (operand->SetMode() == 2)
                (*rel)[operand->RelVar] = (int16_t)std::max(-100, std::min(100, (*rel)[operand->RelVar] + (int)VMMemory::GetVariable(context, operand->VarScope, operand->VarData)));
            return VMPrimitiveExitCode::GOTO_TRUE;
        }
        // otherwise: object relationships (MeToObject), exactly as below.
    }

    VMEntity* obj1;
    VMEntity* obj2;

    switch (operand->Mode)
    {
        case 0: //from me to stack object
            obj1 = context->Caller;
            obj2 = context->StackObject;
            break;
        case 1: //from stack object to me
            obj1 = context->StackObject;
            obj2 = context->Caller;
            break;
        case 2: //from stack object to object in local/stack param
            obj1 = context->StackObject;
            obj2 = context->VM()->GetObjectById(isOld ? context->Args.at(0) : context->Locals.at(operand->Local));
            break;
        case 3: //from object in local/stack param to stack object
            obj1 = context->VM()->GetObjectById(isOld ? context->Args.at(0) : context->Locals.at(operand->Local));
            obj2 = context->StackObject;
            break;
        default:
            throw VMSimanticsException("Invalid relationship type!", context);
    }

    // FIX(sims) QA4: FreeSO (the TS1 reference) answers true when either object is gone:
    //   if ((obj1 == null || obj2 == null) && !(ts1 && operand.UseNeighbor)) return VMPrimitiveExitCode.GOTO_TRUE;
    // FreeSims threw (a reset). The Hot Date socials read the partner from the social object's attribute 0
    // ('so id 0 := so attr 0'), which the conversation clears when the partner leaves ('set people to init with'),
    // so 'Gossip A' / 'Brag - About Self - Decision' reset the Sim when the other one had walked off (QA3 sweep:
    // Ginia on Downtown 23; QA4: Gunther on Magic Town 96).
    if (obj1 == nullptr || obj2 == nullptr) {
        if (context->VM()->TS1) return VMPrimitiveExitCode::GOTO_TRUE;
        throw VMSimanticsException("Object reference not set to an instance of an object.", context);
    }

    auto& rels = obj1->MeToObject;
    auto targId = (uint16_t)obj2->ObjectID;

    //check if exists
    if (rels.find(targId) == rels.end())
    {
        if (operand->FailIfTooSmall()) return VMPrimitiveExitCode::GOTO_FALSE;
        else rels.emplace(targId, std::vector<int16_t>());
    }
    if (rels[targId].size() <= operand->RelVar)
    {
        if (operand->FailIfTooSmall()) return VMPrimitiveExitCode::GOTO_FALSE;
        else {
            while (rels[targId].size() <= operand->RelVar) rels[targId].push_back(0);
        }
    }


    if (operand->SetMode() == 0)
    {
        VMMemory::SetVariable(context, operand->VarScope, operand->VarData, rels[targId][operand->RelVar]);
    }
    else if (operand->SetMode() == 1)
    { //todo, special system for server persistent avatars and pets
        auto value = VMMemory::GetVariable(context, operand->VarScope, operand->VarData);
        rels[targId][operand->RelVar] = value;
    }
    else if (operand->SetMode() == 2)
    {
        auto value = VMMemory::GetVariable(context, operand->VarScope, operand->VarData);
        rels[targId][operand->RelVar] = (int16_t)(rels[targId][operand->RelVar] + value);
    }

    return VMPrimitiveExitCode::GOTO_TRUE;
}

// ---- VMOldRelationshipOperand ----

void VMOldRelationshipOperand::Read(const uint8_t* bytes)
{
    st::Reader io(bytes, 8, st::Endian::Little);
    GetSet = io.u8();
    RelVar = io.u8();
    VarScope = Engine::Scopes::VMVariableScope::Parameters;
    VarData = io.u8(); //parameter number
    Mode = io.u8();
    Flags = io.u8();
}

void VMOldRelationshipOperand::Write(uint8_t* bytes)
{
    st::Writer io(st::Endian::Little);
    io.WriteByte(GetSet);
    io.WriteByte(RelVar);
    io.WriteByte((uint8_t)VarData);
    io.WriteByte(Mode);
    io.WriteByte(Flags);
    auto& buf = io.ToArray();
    std::copy(buf.begin(), buf.begin() + std::min<size_t>(buf.size(), OPERAND_SIZE), bytes);
}

// ---- VMRelationshipOperand ----

void VMRelationshipOperand::Read(const uint8_t* bytes)
{
    st::Reader io(bytes, 8, st::Endian::Little);
    RelVar = io.u8();
    Mode = io.u8();
    Flags = io.u8();
    Local = io.u8();
    VarScope = (Engine::Scopes::VMVariableScope)io.u16();
    VarData = io.s16();
}

void VMRelationshipOperand::Write(uint8_t* bytes)
{
    st::Writer io(st::Endian::Little);
    io.WriteByte(RelVar);
    io.WriteByte(Mode);
    io.WriteByte(Flags);
    io.WriteByte(Local);
    io.WriteUInt16((uint16_t)VarScope);
    io.WriteInt16(VarData);
    auto& buf = io.ToArray();
    std::copy(buf.begin(), buf.begin() + std::min<size_t>(buf.size(), OPERAND_SIZE), bytes);
}

}}}
