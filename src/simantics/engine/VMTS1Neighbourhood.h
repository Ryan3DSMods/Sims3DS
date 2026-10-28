/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// TS1(sims): The Sims 1 neighbourhood access for the VM.
//
// FreeSims has no code for these (the TS1 scopes return 1 / throw, the Set To Next neighbour cases are
// commented out). Semantics come from the game's own Edith labels in GameData/Behavior.iff
// (STR# 132 'data owners', 221 'neighbor data labels', 243 'job data', 249 'neighborhood data labels',
// 200 'Person Data Labels') and from how the shipped BHAVs use them (see STUBS_neighbourhood.md).
// Everything here is new code; nothing in it is a translation.
#pragma once
#include <cstdint>
#include <vector>
#include "simantics/_files_contract.h"

namespace FSO { namespace Content { namespace TS1 { class TS1NeighborhoodProvider; } } }
namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks { class Neighbour; class OBJD; class FAMI; } } } } }

namespace FSO { namespace SimAntics {
class VM;
class VMEntity;
class VMAvatar;
namespace Engine {
class VMStackFrame;
namespace Utils {

class VMTS1Neighbourhood {
public:
    // Person data indices used by the neighbourhood (Behavior.iff STR# 200).
    static constexpr int32_t PD_NEIGHBOR_ID = 31;
    static constexpr int32_t PD_JOB_TYPE = 56;
    static constexpr int32_t PD_JOB_LEVEL = 57;
    static constexpr int32_t PD_PERSON_AGE = 58;
    static constexpr int32_t PD_FAMILY_NUMBER = 61;
    static constexpr int32_t PD_GENDER = 65;
    // VM globals (Behavior.iff STR# 129 'global labels').
    static constexpr uint16_t GLOBAL_SELECTED_PERSON = 3;
    static constexpr uint16_t GLOBAL_CURRENT_FAMILY = 9;
    static constexpr uint16_t GLOBAL_CURRENT_HOUSE = 10;
    static constexpr uint16_t GLOBAL_BUDGET_MOD = 13;
    static constexpr uint16_t GLOBAL_BUDGET_DIV = 14;

    /// null when the install has no UserData/Neighborhood.iff
    static FSO::Content::TS1::TS1NeighborhoodProvider* Provider();
    static FSO::Files::Formats::IFF::Chunks::Neighbour* GetNeighbor(int16_t neighborID);

    /// The neighbour id of an entity (person data 31 of an avatar), 0 for objects / unknown.
    static int16_t NeighborIDOf(VMEntity* ent);
    /// The avatar on this lot that is the given neighbour, or null.
    static VMAvatar* FindAvatarForNeighbor(FSO::SimAntics::VM* vm, int16_t neighborID);

    /// scope 24 "neighbor in stack object" (neighbour id = stack object id).
    static int16_t GetNeighborData(VMStackFrame* context, int16_t neighborID, int16_t data);
    /// scope 32 "neighbor's person data".
    static int16_t GetNeighborPersonData(VMStackFrame* context, int16_t neighborID, int16_t data);
    static bool SetNeighborPersonData(VMStackFrame* context, int16_t neighborID, int16_t data, int16_t value);
    /// scope 33 "job data [temp 0,1]".
    static int16_t GetJobData(VMStackFrame* context, int16_t data);
    /// scope 34 "neighborhood data".
    static int16_t GetNeighborhoodData(VMStackFrame* context, int16_t data);
    static bool SetNeighborhoodData(VMStackFrame* context, int16_t data, int16_t value);
    /// scopes 36/37 "my type attr" / "stack obj's type attr" (TATT chunk of Neighborhood.iff).
    static uint32_t TypeAttrGUID(FSO::Files::Formats::IFF::Chunks::OBJD* objd);
    static int16_t GetTypeAttr(VMStackFrame* context, VMEntity* ent, int16_t data);
    static bool SetTypeAttr(VMStackFrame* context, VMEntity* ent, int16_t data, int16_t value);
    /// scope 38 "Neighbor's Object Definition" (OBJD of the neighbour in the stack object).
    static int16_t GetNeighborDefinition(VMStackFrame* context, int16_t neighborID, int16_t data);

    /// Relationship values from neighbour `from` to neighbour `to` (NBRS relationship table), null if absent
    /// and !create.
    static std::vector<int16_t>* GetRelationship(int16_t from, int16_t to, bool create);

    /// Copies the neighbour record (person data) onto an avatar of that neighbour's GUID before its init tree
    /// runs, the way The Sims spawns a neighbour. Returns the neighbour id (0 if the GUID is not a neighbour).
    static int16_t InheritNeighbor(VMAvatar* avatar, FSO::SimAntics::VM* vm = nullptr);
    /// "Save This Sim's Persistent Data": writes the avatar's person data back into its neighbour record.
    static bool SavePersistentData(VMAvatar* avatar);

    // ---- families (Generic Sims Call 4/5/6/7/33) ----
    /// The family living on this lot: the provider's GameState.ActiveFamily, else FAMI #global 9 "Current Family".
    static FSO::Files::Formats::IFF::Chunks::FAMI* CurrentFamily(FSO::SimAntics::VM* vm);
    /// The person a family call acts on: the stack object if it is an avatar, otherwise the stack object id taken
    /// as a neighbour id. False if neither names a known person.
    static bool ResolvePerson(VMStackFrame* context, int16_t& neighborID, uint32_t& guid);
    /// Moves a person into family `familyNumber` (0 = no family): FAMI member lists, the neighbour record's and the
    /// on-lot avatar's person data 61.
    static void MoveToFamily(FSO::SimAntics::VM* vm, int16_t neighborID, uint32_t guid, int16_t familyNumber);
    /// Creates a neighbour record for an avatar that has none (deprecated "make new neighbor"). Returns its id.
    static int16_t MakeNewNeighbor(VMAvatar* avatar);
};

}}}}
