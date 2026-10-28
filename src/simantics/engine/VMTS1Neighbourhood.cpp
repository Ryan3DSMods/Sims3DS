/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// TS1(sims): see VMTS1Neighbourhood.h. New code (FreeSims has none for The Sims 1 neighbourhood).
#include <cstdlib>
#include "simantics/engine/VMTS1Neighbourhood.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMThread.h"
#include "simantics/engine/VMMemory.h"
#include "simantics/engine/VMSimanticsException.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/model/VMPersonDataVariable.h"
#include "simantics/model/VMStackObjectDefinitionVariable.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"
#include "content/ContentManager.h"
#include "content/TS1NeighbourProvider.h"
#include "content/TS1JobProvider.h"
#include "files/formats/iff/chunks/NBRS.h"
#include "files/formats/iff/chunks/OBJD.h"
#include <algorithm>

namespace FSO { namespace SimAntics { namespace Engine { namespace Utils {

using FSO::Content::TS1::TS1NeighborhoodProvider;
using FSO::Files::Formats::IFF::Chunks::Neighbour;
using FSO::Files::Formats::IFF::Chunks::OBJD;
using FSO::SimAntics::Model::VMPersonDataVariable;

TS1NeighborhoodProvider* VMTS1Neighbourhood::Provider()
{
    auto content = FSO::Content::Content::Get();
    if (content == nullptr || !content->Neighborhood || content->Neighborhood->Neighbors == nullptr) return nullptr;
    return content->Neighborhood.get();
}

Neighbour* VMTS1Neighbourhood::GetNeighbor(int16_t neighborID)
{
    auto prov = Provider();
    return (prov == nullptr) ? nullptr : prov->GetNeighborByID(neighborID);
}

int16_t VMTS1Neighbourhood::NeighborIDOf(VMEntity* ent)
{
    auto ava = dynamic_cast<VMAvatar*>(ent);
    if (ava == nullptr) return 0;
    return ava->GetPersonData((VMPersonDataVariable)PD_NEIGHBOR_ID);
}

VMAvatar* VMTS1Neighbourhood::FindAvatarForNeighbor(FSO::SimAntics::VM* vm, int16_t neighborID)
{
    if (neighborID <= 0) return nullptr;
    for (auto ent : vm->Entities) {
        auto ava = dynamic_cast<VMAvatar*>(ent);
        if (ava != nullptr && !ava->Dead && ava->GetPersonData((VMPersonDataVariable)PD_NEIGHBOR_ID) == neighborID) return ava;
    }
    return nullptr;
}

static int16_t PersonDataOf(Neighbour* n, int32_t index)
{
    if (n == nullptr || index < 0 || index >= (int32_t)n->PersonData.size()) return 0;
    return n->PersonData[index];
}

int16_t VMTS1Neighbourhood::GetNeighborData(VMStackFrame* context, int16_t neighborID, int16_t data)
{
    auto prov = Provider();
    auto neigh = (prov == nullptr) ? nullptr : prov->GetNeighborByID(neighborID);
    if (neigh == nullptr) return 0;
    auto vm = context->VM();
    switch (data)
    {
        case 0: { //person instance id: the neighbour's object on this lot, 0 if not here.
            auto ava = FindAvatarForNeighbor(vm, neighborID);
            return (ava == nullptr) ? (int16_t)0 : ava->ObjectID;
        }
        case 1: { //belongs in house: lives on the current lot (global 10 "Current House").
            auto house = prov->GetHouseOfNeighbor(neigh);
            return (int16_t)((house != 0 && house == vm->GetGlobalValue(GLOBAL_CURRENT_HOUSE)) ? 1 : 0);
        }
        case 2: //person age
            return GetNeighborPersonData(context, neighborID, PD_PERSON_AGE);
        case 3: //relationship raw score
        case 4: { //relationship score
            // of the caller (if it is a neighbour) towards this neighbour. Not used by the shipped BHAVs.
            auto me = NeighborIDOf(context->Caller);
            auto rel = (me == 0) ? nullptr : GetRelationship(me, neighborID, false);
            return (rel == nullptr || rel->empty()) ? (int16_t)0 : (*rel)[0];
        }
        case 5: { //friend count: relationships of this neighbour at "friend" level (>= 50).
            int16_t count = 0;
            for (auto& r : neigh->Relationships)
                if (!r.second.empty() && r.second[0] >= 50) count++;
            return count;
        }
        case 6: //house number
            return prov->GetHouseOfNeighbor(neigh);
        case 7: //has telephone: neighbours who live in a house have a phone line (townies do not).
            return (int16_t)((prov->GetHouseOfNeighbor(neigh) != 0) ? 1 : 0);
        case 8: //has baby: babies are objects on the lot in TS1, never neighbours.
            return 0;
        case 9: { //family friend count
            auto fam = prov->GetFamily(GetNeighborPersonData(context, neighborID, PD_FAMILY_NUMBER));
            return (fam == nullptr) ? (int16_t)0 : (int16_t)fam->FamilyFriends;
        }
        default:
            throw VMSimanticsException("Neighbor data out of bounds.", context);
    }
}

int16_t VMTS1Neighbourhood::GetNeighborPersonData(VMStackFrame* context, int16_t neighborID, int16_t data)
{
    // A neighbour present on the lot answers with its live person data (it is written back to the record
    // with "Save This Sim's Persistent Data"), otherwise the neighbourhood record is used.
    auto ava = FindAvatarForNeighbor(context->VM(), neighborID);
    if (ava != nullptr && data >= 0 && data <= 100) return ava->GetPersonData((VMPersonDataVariable)data);
    return PersonDataOf(GetNeighbor(neighborID), data);
}

bool VMTS1Neighbourhood::SetNeighborPersonData(VMStackFrame* context, int16_t neighborID, int16_t data, int16_t value)
{
    auto neigh = GetNeighbor(neighborID);
    if (neigh != nullptr && data >= 0 && data < (int16_t)neigh->PersonData.size()) neigh->PersonData[data] = value;
    auto ava = FindAvatarForNeighbor(context->VM(), neighborID);
    if (ava != nullptr && data >= 0 && data <= 100) ava->SetPersonData((VMPersonDataVariable)data, value);
    return true;
}

int16_t VMTS1Neighbourhood::GetJobData(VMStackFrame* context, int16_t data)
{
    auto content = FSO::Content::Content::Get();
    if (!content->Jobs) return 0;
    auto& temps = context->Thread->TempRegisters;
    return (int16_t)content->Jobs->GetJobData(temps[0], temps[1], data);
}

int16_t VMTS1Neighbourhood::GetNeighborhoodData(VMStackFrame* context, int16_t data)
{
    auto prov = Provider();
    if (data < 0 || data >= 16) throw VMSimanticsException("Neighborhood data out of bounds.", context);
    if (prov == nullptr || prov->Neighborhood == nullptr) return 0;
    return prov->Neighborhood->NeighborhoodData[data];
}

bool VMTS1Neighbourhood::SetNeighborhoodData(VMStackFrame* context, int16_t data, int16_t value)
{
    auto prov = Provider();
    if (data < 0 || data >= 16) throw VMSimanticsException("Neighborhood data out of bounds.", context);
    if (prov == nullptr || prov->Neighborhood == nullptr) return false;
    prov->Neighborhood->NeighborhoodData[data] = value;
    return true;
}

uint32_t VMTS1Neighbourhood::TypeAttrGUID(OBJD* objd)
{
    // OBJD fields 92/93 'type attr guid 1/2' (Behavior.iff STR# 204); RawData starts at field 2.
    if (objd == nullptr) return 0;
    uint32_t guid = 0;
    if (objd->RawData.size() > 91) guid = (uint32_t)objd->RawData[90] | ((uint32_t)objd->RawData[91] << 16);
    return (guid != 0) ? guid : objd->GUID; // no shared type: the object's own GUID
}

int16_t VMTS1Neighbourhood::GetTypeAttr(VMStackFrame* context, VMEntity* ent, int16_t data)
{
    if (ent == nullptr) throw VMSimanticsException("Object reference not set to an instance of an object.", context);
    auto content = FSO::Content::Content::Get();
    if (!content->Neighborhood) return 0;
    return content->Neighborhood->GetTATT(TypeAttrGUID(ent->Object->OBJ), data);
}

bool VMTS1Neighbourhood::SetTypeAttr(VMStackFrame* context, VMEntity* ent, int16_t data, int16_t value)
{
    if (ent == nullptr) throw VMSimanticsException("Object reference not set to an instance of an object.", context);
    auto content = FSO::Content::Content::Get();
    if (!content->Neighborhood) return false;
    content->Neighborhood->SetTATT(TypeAttrGUID(ent->Object->OBJ), data, value);
    return true;
}

int16_t VMTS1Neighbourhood::GetNeighborDefinition(VMStackFrame* context, int16_t neighborID, int16_t data)
{
    auto neigh = GetNeighbor(neighborID);
    if (neigh == nullptr) return 0;
    auto obj = FSO::Content::Content::Get()->WorldObjects->Get(neigh->GUID, true);
    if (obj == nullptr) return 0;
    return VMMemory::GetEntityDefinitionVar(obj->OBJ, (FSO::SimAntics::Model::VMOBJDVariable)data, context);
}

std::vector<int16_t>* VMTS1Neighbourhood::GetRelationship(int16_t from, int16_t to, bool create)
{
    auto neigh = GetNeighbor(from);
    if (neigh == nullptr) return nullptr;
    auto it = neigh->Relationships.find(to);
    if (it == neigh->Relationships.end()) {
        if (!create) return nullptr;
        it = neigh->Relationships.emplace(to, std::vector<int16_t>()).first;
    }
    return &it->second;
}

int16_t VMTS1Neighbourhood::InheritNeighbor(VMAvatar* avatar, FSO::SimAntics::VM* vm)
{
    auto prov = Provider();
    if (prov == nullptr || avatar == nullptr || avatar->Object == nullptr) return 0;
    int16_t id = 0;
    if (!prov->GetNeighborIDForGUID(avatar->Object->OBJ->GUID, id)) return 0;
    auto neigh = prov->GetNeighborByID(id);
    if (neigh == nullptr) return 0;
    // FIX(sims): FreeSO VMAvatar.InheritNeighbor: a neighbour who is not of the lot's family is a visitor (person
    // type 1) and starts ungreeted; the visitor schedule survives. Before, the NBRS person type (0) was copied, so every
    // walk-by / welcome-wagon neighbour acted as a resident: used the beds/fridge/shower, never went home, got the
    // family's car pool / school bus / military school letters (Mortimer & Cassandra Goth stayed on a new family's lot).
    auto lastPersonType = avatar->GetPersonData(VMPersonDataVariable::PersonType);
    auto lastGender = avatar->GetPersonData(VMPersonDataVariable::Gender);
    auto sched = avatar->GetPersonData(VMPersonDataVariable::VisitorSchedule);
    // FIX(sims) QA2: records of the newer layout (version 0xA, 256 shorts on disk) carry person data 80..87 too:
    // Superstar fame score / stars (80, 81, 82) and other expansion fields. They were cut at 80, so a Sim's fame was
    // lost on every lot change (agent signed at home: fame 1 -> 0 at Studio Town). Version 4 records stop at 80.
    int32_t count = std::min((int32_t)neigh->PersonData.size(), neigh->Version == 0x4 ? (int32_t)TS1NeighborhoodProvider::TS1_PERSON_DATA_COUNT : 88);
    for (int32_t i = 0; i < count; i++) {
        // posture / tree use fields are runtime state; everything else is the persistent person.
        if (i == (int32_t)VMPersonDataVariable::Posture) continue;
        // FIX(sims): 'priority' (33) is the running interaction's priority, also runtime state (the lot loader skips it
        // in OBJM too). A Sim saved to the neighbourhood mid-interaction (the traveller leaving a community lot in the
        // cab at priority 100, a townie) came back with it; with nothing running, 'idle for input' saw a queued item it
        // could never push (100 <= 100) and 'check queue' spun into the 500k-primitive guard (Bob after a date,
        // Bella as a townie).
        if (i == (int32_t)VMPersonDataVariable::Priority) { avatar->SetPersonData(VMPersonDataVariable::Priority, 0); continue; }
        avatar->SetPersonData((VMPersonDataVariable)i, neigh->PersonData[i]);
    }
    avatar->SetPersonData((VMPersonDataVariable)PD_NEIGHBOR_ID, id);
    // FreeSO keeps the body-string gender of a pet in its FSO encoding (8/9/16/17); TS1 values (2..5) come from NBRS
    if (lastGender & (8 | 16)) avatar->SetPersonData(VMPersonDataVariable::Gender, lastGender);
    if (vm != nullptr) { // FIX(sims) (see above)
        auto current = CurrentFamily(vm);
        if (lastPersonType == 0)
            avatar->SetPersonData(VMPersonDataVariable::PersonType,
                (int16_t)((current != nullptr && avatar->GetPersonData((VMPersonDataVariable)PD_FAMILY_NUMBER) == (int16_t)current->ChunkID) ? 0 : 1));
        else avatar->SetPersonData(VMPersonDataVariable::PersonType, lastPersonType);
        avatar->SetPersonData(VMPersonDataVariable::VisitorSchedule, sched);
        avatar->SetPersonData(VMPersonDataVariable::GreetStatus, 0);
    }
    return id;
}

bool VMTS1Neighbourhood::SavePersistentData(VMAvatar* avatar)
{
    auto id = NeighborIDOf(avatar);
    auto neigh = GetNeighbor(id);
    if (neigh == nullptr) return false;
    if (neigh->PersonData.size() < (size_t)TS1NeighborhoodProvider::TS1_PERSON_DATA_COUNT)
        neigh->PersonData.resize(88, 0);
    if (neigh->PersonMode == 0) neigh->PersonMode = 5;
    for (int32_t i = 0; i < TS1NeighborhoodProvider::TS1_PERSON_DATA_COUNT; i++)
        neigh->PersonData[i] = avatar->GetPersonData((VMPersonDataVariable)i);
    // FIX(sims) QA2: person data 80..87 (Superstar fame, ...) only fit the newer record layout; a version 4 record is
    // upgraded to it (version 0xA + the int 9 that follows, as the expansion packs write it) once one of them is set.
    if (neigh->PersonData.size() < 88) neigh->PersonData.resize(88, 0);
    bool extra = false;
    for (int32_t i = TS1NeighborhoodProvider::TS1_PERSON_DATA_COUNT; i < 88; i++) {
        neigh->PersonData[i] = avatar->GetPersonData((VMPersonDataVariable)i);
        if (neigh->PersonData[i] != 0) extra = true;
    }
    if (extra && neigh->Version == 0x4) { neigh->Version = 0xA; neigh->Unknown3 = 9; }
    auto prov = Provider();
    if (prov != nullptr) prov->DirtyAvatars.insert(neigh->GUID);
    return true;
}

}}}}

namespace FSO { namespace SimAntics { namespace Engine { namespace Utils {

using FSO::Files::Formats::IFF::Chunks::FAMI;

FAMI* VMTS1Neighbourhood::CurrentFamily(FSO::SimAntics::VM* vm)
{
    auto prov = Provider();
    if (prov == nullptr) return nullptr;
    if (prov->GameState.ActiveFamily != nullptr) return prov->GameState.ActiveFamily;
    auto num = vm->GetGlobalValue(GLOBAL_CURRENT_FAMILY);
    return (num > 0) ? prov->GetFamily(num) : nullptr;
}

bool VMTS1Neighbourhood::ResolvePerson(VMStackFrame* context, int16_t& neighborID, uint32_t& guid)
{
    neighborID = 0; guid = 0;
    auto ava = dynamic_cast<VMAvatar*>(context->StackObject);
    if (ava != nullptr) {
        neighborID = NeighborIDOf(ava);
        guid = ava->Object->OBJ->GUID;
        return true;
    }
    auto neigh = GetNeighbor(context->StackObjectID());
    if (neigh == nullptr) return false;
    neighborID = neigh->NeighbourID;
    guid = neigh->GUID;
    return true;
}

void VMTS1Neighbourhood::MoveToFamily(FSO::SimAntics::VM* vm, int16_t neighborID, uint32_t guid, int16_t familyNumber)
{
    auto prov = Provider();
    if (prov == nullptr) return;
    for (auto fam : prov->ListFamilies()) {
        auto& g = fam->FamilyGUIDs;
        if ((int16_t)fam->ChunkID == familyNumber) {
            if (std::find(g.begin(), g.end(), guid) == g.end()) g.push_back(guid);
        } else {
            g.erase(std::remove(g.begin(), g.end(), guid), g.end());
        }
        auto& r = fam->RuntimeSubset;
        if ((int16_t)fam->ChunkID != familyNumber) r.erase(std::remove(r.begin(), r.end(), guid), r.end());
    }
    auto neigh = prov->GetNeighborByID(neighborID);
    if (neigh != nullptr && (int32_t)neigh->PersonData.size() > PD_FAMILY_NUMBER) neigh->PersonData[PD_FAMILY_NUMBER] = familyNumber;
    for (auto ent : vm->Entities) {
        auto ava = dynamic_cast<VMAvatar*>(ent);
        if (ava != nullptr && ava->Object->OBJ->GUID == guid)
            ava->SetPersonData((VMPersonDataVariable)PD_FAMILY_NUMBER, familyNumber);
    }
    if (neigh != nullptr) prov->DirtyAvatars.insert(guid);
}

int16_t VMTS1Neighbourhood::MakeNewNeighbor(VMAvatar* avatar)
{
    auto prov = Provider();
    if (prov == nullptr || avatar == nullptr) return 0;
    auto existing = NeighborIDOf(avatar);
    if (existing > 0 && prov->GetNeighborByID(existing) != nullptr) return existing;
    int16_t maxID = 0;
    for (auto& e : prov->Neighbors->Entries) maxID = std::max(maxID, e->NeighbourID);
    auto neigh = new FSO::Files::Formats::IFF::Chunks::Neighbour();
    neigh->NeighbourID = (int16_t)(maxID + 1);
    neigh->GUID = avatar->Object->OBJ->GUID;
    neigh->Name = avatar->Name();
    neigh->Version = 0x4;
    neigh->PersonMode = 5;
    neigh->PersonData.assign(88, 0);
    prov->Neighbors->Entries.emplace_back(neigh);
    prov->Neighbors->NeighbourByID[neigh->NeighbourID] = neigh;
    if (prov->Neighbors->DefaultNeighbourByGUID.find(neigh->GUID) == prov->Neighbors->DefaultNeighbourByGUID.end())
        prov->Neighbors->DefaultNeighbourByGUID[neigh->GUID] = neigh->NeighbourID;
    avatar->SetPersonData((VMPersonDataVariable)PD_NEIGHBOR_ID, neigh->NeighbourID);
    SavePersistentData(avatar);
    avatar->SetPersonData((VMPersonDataVariable)PD_NEIGHBOR_ID, neigh->NeighbourID);
    neigh->PersonData[PD_NEIGHBOR_ID] = neigh->NeighbourID;
    return neigh->NeighbourID;
}

}}}}
