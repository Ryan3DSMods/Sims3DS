/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// TS1(sims): puts a neighbourhood family on its lot.
//
// FreeSims has no TS1 family code. Its closest equivalent is VMWorldActivator.CreateAvatar(guid, xml, visitor,
// id) (+ VMBlueprintRestoreCmd), which creates each character by GUID and stands it next to the mailbox (GUIDs
// 0xEF121974 / 0x1D95C9B0 / 0x74E8A2A9 / 0x865A6812). This loader follows the same idea for The Sims 1 data:
//   * the family is FAMI.HouseNumber == house (TS1NeighborhoodProvider::FamilyForHouse) or a given FAMI;
//   * every member GUID (FAMI.FamilyGUIDs) is an OBJD in UserData/Characters/User*.iff (registered with
//     WorldObjects by TS1NeighborhoodProvider::LoadCharacters). A TS1 house file that the family lives in already
//     lists them in OBJT/OBJM, so VMWorldActivator::LoadFromIff has created those avatars (out of world: the
//     OBJM decode gives no person positions); they are reused, missing members are created;
//   * VMAvatar::Init (TS1 path) inherits each person's neighbour record (person data, family number, neighbour
//     id) before the person's init tree runs;
//   * avatars without a position are placed with the translated VMFindLocationFor next to the mailbox (as
//     FreeSims' CreateAvatar), else the pedestrian/car portal, else the lot centre;
//   * the TS1 globals the person trees read are restored: 9 "Current Family", 10 "Current House" (FreeSims'
//     activator overwrites it with 1), 13/14 budget, 3 "Selected Person ID".
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "simantics/_files_contract.h"

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks { class FAMI; } } } } }
namespace FSO { namespace SimAntics { class VM; class VMEntity; class VMAvatar; } }

class VMTS1FamilyLoader {
public:
    static const uint32_t MAILBOX_GUIDS[4];
    static const uint32_t PORTAL_GUIDS[2];

    explicit VMTS1FamilyLoader(FSO::SimAntics::VM* vm) : VM(vm) {}

    /// Call after VMWorldActivator::LoadFromIff. `family` == null: the family living in `houseNumber`.
    /// Returns the family's avatars (empty if the house has no family).
    std::vector<FSO::SimAntics::VMAvatar*> LoadFamily(int16_t houseNumber, FSO::Files::Formats::IFF::Chunks::FAMI* family = nullptr);

    /// Mailbox (FreeSims' CreateAvatar GUID list), else a portal, else null.
    FSO::SimAntics::VMEntity* FindArrivalPoint();
    /// Places an out-of-world avatar near `ref` (or the lot centre). True if it is now on the lot.
    bool PlaceNear(FSO::SimAntics::VMAvatar* avatar, FSO::SimAntics::VMEntity* ref);

    FSO::Files::Formats::IFF::Chunks::FAMI* Family = nullptr;
    std::vector<std::string> Log; // human readable notes about what was done

private:
    FSO::SimAntics::VM* VM;
};
