/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// TS1(sims): see VMTS1FamilyLoader.h
#include "simantics/utils/VMTS1FamilyLoader.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"
#include "simantics/VMArchitecture.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/entities/VMMultitileGroup.h"
#include "simantics/engine/VMTS1Neighbourhood.h"
#include "simantics/engine/VMTS1Travel.h"
#include "simantics/primitives/VMFindLocationFor.h"
#include "content/ContentManager.h"
#include "content/TS1NeighbourProvider.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>

using namespace FSO::SimAntics;
using FSO::SimAntics::Engine::Utils::VMTS1Neighbourhood;
using FSO::SimAntics::Engine::Utils::VMTS1Travel;
using FSO::Files::Formats::IFF::Chunks::FAMI;
using FSO::LotView::Model::LotTilePos;
using FSO::LotView::Model::Direction;

// FreeSims VMWorldActivator.CreateAvatar: 4010940788u, 496355760, 1961361321, 2254071826u
const uint32_t VMTS1FamilyLoader::MAILBOX_GUIDS[4] = { 0xEF121974u, 0x1D95C9B0u, 0x74E8A2A9u, 0x865A6812u };
// pedestrian / car portal ("*Portal - Pedestrian", "*Portal - Car")
const uint32_t VMTS1FamilyLoader::PORTAL_GUIDS[2] = { 0x81E6BEF9u, 0x865A6812u };

VMEntity* VMTS1FamilyLoader::FindArrivalPoint()
{
    for (auto guid : MAILBOX_GUIDS) {
        for (auto ent : VM->Entities) {
            if (ent->Object->OBJ->GUID == guid && ent->Position() != LotTilePos::OUT_OF_WORLD) return ent;
        }
    }
    for (auto guid : PORTAL_GUIDS) {
        for (auto ent : VM->Entities) {
            if (ent->Object->OBJ->GUID == guid && ent->Position() != LotTilePos::OUT_OF_WORLD) return ent;
        }
    }
    return nullptr;
}

bool VMTS1FamilyLoader::PlaceNear(VMAvatar* avatar, VMEntity* ref)
{
    auto ctx = VM->Context;
    if (ref != nullptr && Primitives::VMFindLocationFor::FindLocationFor(avatar, ref, ctx)
        && avatar->Position() != LotTilePos::OUT_OF_WORLD) return true;
    // fallback: spiral out from the lot centre on the ground floor
    auto arch = ctx->Architecture();
    int cx = arch->Width / 2, cy = arch->Height / 2;
    for (int r = 0; r < std::max(arch->Width, arch->Height) / 2; r++) {
        for (int dy = -r; dy <= r; dy++) {
            for (int dx = -r; dx <= r; dx++) {
                if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
                auto pos = LotTilePos::FromBigTile((int16_t)(cx + dx), (int16_t)(cy + dy), 1);
                if (ctx->IsUserOutOfBounds(pos)) continue;
                avatar->SetPosition(pos, Direction::WEST, ctx);
                if (avatar->Position() != LotTilePos::OUT_OF_WORLD) return true;
            }
        }
    }
    return false;
}

std::vector<VMAvatar*> VMTS1FamilyLoader::LoadFamily(int16_t houseNumber, FAMI* family)
{
    std::vector<VMAvatar*> result;
    auto prov = VMTS1Neighbourhood::Provider();
    if (prov == nullptr) { Log.push_back("no neighbourhood (UserData/Neighborhood.iff) loaded"); return result; }
    if (family == nullptr) family = prov->GetFamilyForHouse(houseNumber);
    Family = family;
    char buf[256];

    // the lot's identity (FreeSims' activator sets global 10 to 1; TS1 trees compare it with house numbers)
    VM->SetGlobalValue(VMTS1Neighbourhood::GLOBAL_CURRENT_HOUSE, houseNumber);
    if (family == nullptr) {
        prov->GameState.ActiveFamily = nullptr;
        VM->SetGlobalValue(VMTS1Neighbourhood::GLOBAL_CURRENT_FAMILY, 0);
        snprintf(buf, sizeof(buf), "house %d: no family lives here", houseNumber);
        Log.push_back(buf);
        return result;
    }

    if (family->HouseNumber != houseNumber) {
        // moving a family in (TS1 "move in" from the family bin)
        if (family->HouseNumber == 0) {
            family->HouseNumber = houseNumber;
            prov->FamilyForHouse[houseNumber] = family;
        }
    }
    prov->GameState.ActiveFamily = family;
    // FIX(sims): neighbourhood data 2 is the house of the family being played (the shipped Neighborhood.iff has 7,
    // the last house played). The 'Can this person visit?' trees of every visitor generator (Downtown/Vacation/Old
    // Town/Studio/Magic ped markers, trolley, limos, VisitGenerator) refuse Sims whose house == nhood 2, so the active
    // family never shows up as townies on a community lot (Bella Goth Downtown while she was at home). Nothing set
    // it: it stayed 7 from the shipped save.
    if (prov->Neighborhood != nullptr) prov->Neighborhood->NeighborhoodData[2] = houseNumber;
    family->SelectWholeFamily();
    VM->SetGlobalValue(VMTS1Neighbourhood::GLOBAL_CURRENT_FAMILY, (int16_t)family->ChunkID);
    VM->SetGlobalValue(VMTS1Neighbourhood::GLOBAL_BUDGET_MOD, (int16_t)(family->Budget % 10000));
    VM->SetGlobalValue(VMTS1Neighbourhood::GLOBAL_BUDGET_DIV, (int16_t)(family->Budget / 10000));
    VMTS1Travel::ApplyFamilySpells(VM); // FIX(sims): Makin' Magic family spells into the Magic Controller

    auto arrival = FindArrivalPoint();
    snprintf(buf, sizeof(buf), "house %d: family #%d '%s', %zu members, budget %d, arrival point %s",
        houseNumber, family->ChunkID, prov->GetFamilyName((int16_t)family->ChunkID).c_str(), family->RuntimeSubset.size(),
        family->Budget, (arrival == nullptr) ? "(none)" : arrival->ToString().c_str());
    Log.push_back(buf);

    for (auto guid : family->RuntimeSubset) {
        VMAvatar* avatar = nullptr;
        // reuse the person the house file already contained (created by VMWorldActivator from OBJM)
        for (auto ent : VM->Entities) {
            auto ava = dynamic_cast<VMAvatar*>(ent);
            if (ava != nullptr && !ava->Dead && ava->Object->OBJ->GUID == guid
                && std::find(result.begin(), result.end(), ava) == result.end()) { avatar = ava; break; }
        }
        bool created = false;
        if (avatar == nullptr) {
            auto group = VM->Context->CreateObjectInstance(guid, LotTilePos::OUT_OF_WORLD, Direction::WEST, true);
            if (group == nullptr || group->Objects.empty()) {
                snprintf(buf, sizeof(buf), "  member %08X: no character object (missing User*.iff?)", guid);
                Log.push_back(buf);
                continue;
            }
            avatar = dynamic_cast<VMAvatar*>(group->Objects[0]);
            if (avatar == nullptr) continue;
            created = true;
        }
        // family number (person data 61) — normally already inherited from the neighbour record
        avatar->SetPersonData((Model::VMPersonDataVariable)VMTS1Neighbourhood::PD_FAMILY_NUMBER, (int16_t)family->ChunkID);
        // FIX(sims): residents are person type 0 (InheritNeighbor makes everyone else a visitor)
        avatar->SetPersonData(Model::VMPersonDataVariable::PersonType, 0);
        bool placed = avatar->Position() != LotTilePos::OUT_OF_WORLD;
        if (!placed) placed = PlaceNear(avatar, arrival);
        auto p = avatar->Position();
        snprintf(buf, sizeof(buf), "  member %08X '%s' (neighbour %d, %s, age %d): %s, %s at tile (%d,%d,L%d)",
            guid, avatar->Name().c_str(), VMTS1Neighbourhood::NeighborIDOf(avatar), avatar->TS1Appearance.Gender.c_str(),
            avatar->GetPersonData((Model::VMPersonDataVariable)VMTS1Neighbourhood::PD_PERSON_AGE),
            created ? "created" : "from house file", placed ? "placed" : "NOT PLACED", p.TileX(), p.TileY(), (int)p.Level);
        Log.push_back(buf);
        result.push_back(avatar);
    }

    // persons the house file contained that are not in the family and have no position (their saved state is in
    // OBJM, which this port does not decode): send them away instead of running them out of world.
    std::vector<VMEntity*> stale;
    for (auto ent : VM->Entities) {
        auto ava = dynamic_cast<VMAvatar*>(ent);
        if (ava == nullptr || ava->Dead || std::find(result.begin(), result.end(), ava) != result.end()) continue;
        if (ava->Position() == LotTilePos::OUT_OF_WORLD && VMTS1Neighbourhood::NeighborIDOf(ava) > 0) stale.push_back(ava);
    }
    for (auto ent : stale) {
        snprintf(buf, sizeof(buf), "  removed out-of-world non-family person %s", ent->ToString().c_str());
        Log.push_back(buf);
        ent->Delete(true, VM->Context);
    }

    if (!result.empty()) VM->SetGlobalValue(VMTS1Neighbourhood::GLOBAL_SELECTED_PERSON, result[0]->ObjectID);
    return result;
}
