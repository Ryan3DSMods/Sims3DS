/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Primitives.VMGenericTS1Call — see VMGenericTS1Call.h
// TS1(sims): translated from FreeSO (tso.simantics/Primitives/VMGenericTS1Call.cs, MPL-2.0) where FreeSO has
// code; the rest is new and marked TS1(sims) with the BHAVs it was derived from.
#include "simantics/primitives/VMGenericTS1Call.h"
#include <algorithm>
#include <cstdlib>
#include <memory>
#include <string>
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMThread.h"
#include "simantics/engine/VMSimanticsException.h"
#include "simantics/engine/VMTS1Neighbourhood.h"
#include "simantics/engine/VMTS1Travel.h"
#include "simantics/engine/VMTS1Outfits.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/entities/VMGameObject.h"
#include "simantics/model/VMPersonDataVariable.h"
#include "simantics/model/VMMotive.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"
#include "content/ContentManager.h"
#include "content/TS1NeighbourProvider.h"
#include "files/formats/iff/IffFile.h"
#include "files/formats/iff/chunks/FAMI.h"
#include "files/formats/iff/chunks/NBRS.h"
#include "files/formats/iff/chunks/NGBH.h"
#include "files/formats/iff/chunks/OBJD.h"
#include "files/formats/iff/chunks/STR.h"

namespace FSO { namespace SimAntics { namespace Primitives {
using namespace FSO::SimAntics::Engine;
using namespace FSO::SimAntics::Model;
using FSO::SimAntics::Engine::Utils::VMTS1Neighbourhood;
using FSO::SimAntics::Engine::Utils::VMTS1Travel;
using FSO::SimAntics::Engine::Utils::VMTS1Outfits;
using FSO::Files::Formats::IFF::Chunks::InventoryItem;
using FSO::Files::Formats::IFF::Chunks::STR;

namespace {
template<class T> T* NotNull(T* e, VMStackFrame* context) {
    if (e == nullptr) throw VMSimanticsException("NullReferenceException", context);
    return e;
}
// C# hard cast `(VMAvatar)x`
VMAvatar* AvatarCast(VMEntity* e, VMStackFrame* context) {
    if (e == nullptr) return nullptr;
    auto r = dynamic_cast<VMAvatar*>(e);
    if (r == nullptr) throw VMSimanticsException("InvalidCastException", context);
    return r;
}
const VMMotive SimDataMotives[7] = { VMMotive::Bladder, VMMotive::Comfort, VMMotive::Energy, VMMotive::Fun,
                                     VMMotive::Hunger, VMMotive::Hygiene, VMMotive::Social };

// ---- TS1(sims): Superstar fame tables (GameData/fame.iff) ----
struct FameTables {
    bool Loaded = false;
    std::vector<int32_t> ScoreNeeded;        // STR#5 'Fame Score Needed'
    std::vector<int32_t> FriendPowerNeeded;  // STR#4 'Friend Star Power Needed'
    std::vector<std::vector<int32_t>> SkillsNeeded; // STR#6 (Cooking, Repair, Charisma, Body, Logic, Creativity)
};
FameTables& Fame() {
    static FameTables t;
    if (t.Loaded) return t;
    t.Loaded = true;
    auto c = FSO::Content::Content::Get();
    std::unique_ptr<FSO::Files::Formats::IFF::IffFile> iff;
    for (auto name : { "/GameData/fame.iff", "/GameData/Fame.iff", "/GameData/FAME.IFF" }) {
        auto path = c->SimsCompleteDir + name;
        if (!FSO::Content::FileExists(path)) continue;
        try { iff.reset(new FSO::Files::Formats::IFF::IffFile(path)); } catch (...) { iff.reset(); }
        if (iff) break;
    }
    if (!iff) return t;
    auto ints = [&](uint16_t id) {
        std::vector<int32_t> r;
        auto s = iff->Get<STR>(id);
        if (s != nullptr) for (int32_t i = 0; i < s->Length(); i++) r.push_back(atoi(s->GetString(i).c_str()));
        return r;
    };
    t.ScoreNeeded = ints(5);
    t.FriendPowerNeeded = ints(4);
    if (auto s = iff->Get<STR>(6)) {
        for (int32_t i = 0; i < s->Length(); i++) {
            std::vector<int32_t> row;
            auto str = s->GetString(i);
            size_t p = 0;
            while (p <= str.size()) {
                auto c2 = str.find(',', p);
                row.push_back(atoi(str.substr(p, (c2 == std::string::npos) ? std::string::npos : c2 - p).c_str()));
                if (c2 == std::string::npos) break;
                p = c2 + 1;
            }
            t.SkillsNeeded.push_back(row);
        }
    }
    return t;
}
// skills in fame.iff order (Cooking, Repair, Charisma, Body, Logic, Creativity) -> person data (Behavior.iff STR#200)
const int FameSkillPD[6] = { 10, 12, 11, 17, 18, 15 };
const int PD_FAME_SCORE = 80, PD_FAME_LEVEL = 81, PD_FAME_HIGHWATER = 82;
const int16_t MAX_FAME_LEVEL = 10; // fame.iff STR#1: Nobody .. Superstar (0 - 5 stars in half stars)

/// Sum of the star power (stars) of the Sim's friends: neighbours it has a daily relationship >= 50 with.
int32_t FriendStarPower(VMAvatar* avatar) {
    auto prov = VMTS1Neighbourhood::Provider();
    if (prov == nullptr || prov->Neighbors == nullptr) return 0;
    auto me = VMTS1Neighbourhood::NeighborIDOf(avatar);
    if (me <= 0) return 0;
    int32_t halfStars = 0;
    for (auto& n : prov->Neighbors->Entries) {
        if (n->NeighbourID == me) continue;
        auto rel = VMTS1Neighbourhood::GetRelationship(me, n->NeighbourID, false);
        if (rel == nullptr || rel->empty() || (*rel)[0] < 50) continue;
        auto live = VMTS1Neighbourhood::FindAvatarForNeighbor(avatar->Thread ? avatar->Thread->Context->VM : nullptr, n->NeighbourID);
        int16_t level = (live != nullptr) ? live->GetPersonData((VMPersonDataVariable)PD_FAME_LEVEL)
            : (((int32_t)n->PersonData.size() > PD_FAME_LEVEL) ? n->PersonData[PD_FAME_LEVEL] : (int16_t)0);
        halfStars += std::max<int16_t>(0, level);
    }
    return halfStars / 2;
}
}

int16_t VMGenericTS1Call::FameLevelFor(VMAvatar* avatar, int16_t level, bool promote)
{
    auto& t = Fame();
    auto score = avatar->GetPersonData((VMPersonDataVariable)PD_FAME_SCORE);
    if (promote) {
        int32_t friends = -1;
        while (level < MAX_FAME_LEVEL && level < (int16_t)t.ScoreNeeded.size()) {
            // the requirement row `level` gets the Sim from `level` to `level + 1`
            if (score < t.ScoreNeeded[level]) break;
            if (level < (int16_t)t.FriendPowerNeeded.size() && t.FriendPowerNeeded[level] > 0) {
                if (friends < 0) friends = FriendStarPower(avatar);
                if (friends < t.FriendPowerNeeded[level]) break;
            }
            bool skills = true;
            if (level < (int16_t)t.SkillsNeeded.size()) {
                auto& row = t.SkillsNeeded[level];
                for (size_t i = 0; i < row.size() && i < 6; i++)
                    if (avatar->GetPersonData((VMPersonDataVariable)FameSkillPD[i]) / 100 < row[i]) { skills = false; break; }
            }
            if (!skills) break;
            level++;
        }
    } else {
        while (level > 0 && level - 1 < (int16_t)t.ScoreNeeded.size() && score < t.ScoreNeeded[level - 1]) level--;
    }
    return level;
}

VMPrimitiveExitCode VMGenericTS1Call::Execute(VMStackFrame* context, VMGenericTS1CallMode call)
{
    auto vm = context->VM();
    auto prov = VMTS1Neighbourhood::Provider();
    uint32_t inventoryInd = 10;
    switch (call) {
        case VMGenericTS1CallMode::HouseTutorialComplete: //0
            // TS1(sims): FreeSO leaves this empty. The Newbie house (UserData House07, SIMI global 26 'Is Tutorial
            // House' = 1) runs the tutorial; Tutorial.iff 'cancel tutorial' / the last lesson call this, after which the
            // house is an ordinary lot (the phone plugins check global 26 before offering Go Downtown / Vacation /...).
            vm->SetGlobalValue(26, 0);
            VMTS1Travel::Signal(vm, VMTS1Travel::EventTutorialComplete, 0);
            break;
        case VMGenericTS1CallMode::PullDownTaxiDialog: //3
            // TS1(sims): the ped markers call this once the travellers are on the lot (PedMarkers 'main - middle
            // ped marker', VacationPedMarker / NeighborhoodPedMarker 'Main - Family Spawn'): the transit screen goes.
            return VMPrimitiveExitCode::GOTO_TRUE;
        /*
       MakeNewNeighbor = 7, //this one is "depracated". there's a dedicated primitive for this.
       FamilyTutorialComplete = 8,
       ArchitectureTutorialComplete = 9, */
        case VMGenericTS1CallMode::DisableBuildBuy: //10
            vm->TS1BuildBuyEnabled = false;
            VMTS1Travel::Signal(vm, VMTS1Travel::EventBuildBuyChange, 0);
            break;
        case VMGenericTS1CallMode::EnableBuildBuy: //11
            vm->TS1BuildBuyEnabled = true;
            VMTS1Travel::Signal(vm, VMTS1Travel::EventBuildBuyChange, 1);
            break;
        case VMGenericTS1CallMode::MyRoutingFootprintEqualsTemp0: //15
            //todo: change the avatar's routing footprint (need to find out how exactly this is changed in the normal game)
            break;
        // 16. Change Normal Outfit
        case VMGenericTS1CallMode::ChangeToLotInTemp0: { //17
            //-1 is this family's home lot
            VMTS1Travel::RequestLotSwitch(vm, dynamic_cast<VMAvatar*>(context->Caller), context->Thread->TempRegisters[0]);
            return VMPrimitiveExitCode::GOTO_TRUE_NEXT_TICK;
        }
        case VMGenericTS1CallMode::BuildTheDowntownSimAndPlaceObjIDInTemp0: { //18
            //spawn downtown sim out of world
            context->Thread->TempRegisters[0] = 0;
            if (prov == nullptr) break;
            auto& crossDataDT = prov->GameState;
            // TS1(sims): no Sim is travelling (the lot was opened directly): nobody to build. FreeSO would
            // dereference a null avatar here (reset); the ped marker loops on temp 0 == 0 instead.
            if (crossDataDT.DowntownSimGUID == 0 || crossDataDT.ActiveFamily == nullptr) break;
            VMAvatar* control = nullptr;
            for (auto e : vm->Entities) { // TS1(sims): build it once
                auto a = dynamic_cast<VMAvatar*>(e);
                if (a != nullptr && !a->Dead && a->Object->OBJ->GUID == crossDataDT.DowntownSimGUID) { control = a; break; }
            }
            if (control == nullptr) control = VMTS1Travel::CreatePerson(vm, crossDataDT.DowntownSimGUID);
            if (control == nullptr) break;
            crossDataDT.ActiveFamily->SelectOneMember(crossDataDT.DowntownSimGUID);
            VMTS1Travel::ActivateFamily(vm, crossDataDT.ActiveFamily);
            control->SetPersonData((VMPersonDataVariable)VMTS1Neighbourhood::PD_FAMILY_NUMBER, (int16_t)crossDataDT.ActiveFamily->ChunkID);
            VMTS1Travel::SelectSim(vm, control);
            context->Thread->TempRegisters[0] = vm->GetGlobalValue(3);
            break;
        }
        case VMGenericTS1CallMode::SpawnTakeBackHomeDataOfPersonInTemp0: //20
            inventoryInd = 11;
            // fall through
        case VMGenericTS1CallMode::SpawnDowntownDateOfPersonInTemp0: { //19
            //spawn our autofollow sim
            auto ntarget = AvatarCast(vm->GetObjectById(context->Thread->TempRegisters[0]), context);
            context->Thread->TempRegisters[0] = -1;
            if (ntarget == nullptr) return VMPrimitiveExitCode::GOTO_FALSE; //vacation?
            auto neighbour = ntarget->GetPersonData(VMPersonDataVariable::NeighborId);
            auto inventory = (prov == nullptr) ? nullptr : prov->GetInventoryByNID(neighbour);
            if (inventory != nullptr) {
                bool has = false; int16_t toSpawn = 0;
                for (auto& x : *inventory) if (x.Type == 2 && x.GUID == inventoryInd) { has = true; toSpawn = (int16_t)x.Count; break; }
                if (has) {
                    auto spawntarg = VMTS1Neighbourhood::GetNeighbor(toSpawn);
                    auto autofollow = (spawntarg == nullptr) ? nullptr : VMTS1Travel::CreatePerson(vm, spawntarg->GUID);
                    if (autofollow != nullptr) context->Thread->TempRegisters[0] = autofollow->ObjectID;
                    inventory->erase(std::remove_if(inventory->begin(), inventory->end(),
                        [&](const InventoryItem& x) { return x.Type == 2 && x.GUID == inventoryInd; }), inventory->end());
                }
            }
            break;
        }
        case VMGenericTS1CallMode::SpawnInventorySimDataEffects: { //21
            //for the caller? stack object?
            //do caller for now
            auto eperson = AvatarCast(vm->GetObjectById(context->Thread->TempRegisters[0]), context);
            if (eperson == nullptr) return VMPrimitiveExitCode::GOTO_TRUE; // TS1(sims): C# would throw (null)
            auto nid = eperson->GetPersonData(VMPersonDataVariable::NeighborId);
            auto eInv = (prov == nullptr) ? nullptr : prov->GetInventoryByNID(nid);
            if (eInv == nullptr || std::none_of(eInv->begin(), eInv->end(), [](const InventoryItem& x) { return x.Type == 2; }))
                return VMPrimitiveExitCode::GOTO_TRUE;
            for (int m = 0; m < 7; m++) eperson->SetMotiveData(SimDataMotives[m], VMTS1Travel::GetSimData(nid, (uint32_t)m));
            //remove the effects since we've used em
            //7 and 8, time, were used to start the lot. they arent really used on return
            //9 is not used by fso
            eInv->erase(std::remove_if(eInv->begin(), eInv->end(),
                [](const InventoryItem& x) { return x.Type == 2 && x.GUID < 7; }), eInv->end());
            return VMPrimitiveExitCode::GOTO_TRUE;
        }
        case VMGenericTS1CallMode::SelectDowntownLot: //22
            //TODO: this is a pre-unleashed system I believe. likely need to add this if we want to support older TS1 versions (need testers)
            return VMPrimitiveExitCode::GOTO_TRUE;
        case VMGenericTS1CallMode::GetDowntownTimeFromSOInventory: { //23
            // TS1(sims): FreeSO left this empty. GoDowntownPhonePlugin / VacationPhonePlugin 'Interaction - Go
            // Downtown' call it after the trip and advance the clock until it matches (hours in T0, minutes in T1),
            // from the sim data the trip saved (Generic Sims Call 17).
            auto eperson = dynamic_cast<VMAvatar*>(context->StackObject);
            if (eperson == nullptr) eperson = dynamic_cast<VMAvatar*>(context->Caller);
            if (eperson == nullptr) return VMPrimitiveExitCode::GOTO_TRUE;
            auto nid = eperson->GetPersonData(VMPersonDataVariable::NeighborId);
            bool fh = false, fm = false;
            auto h = VMTS1Travel::GetSimData(nid, VMTS1Travel::SLOT_HOURS, &fh), m = VMTS1Travel::GetSimData(nid, VMTS1Travel::SLOT_MINUTES, &fm);
            context->Thread->TempRegisters[0] = fh ? h : (int16_t)vm->Context->Clock->Hours;
            context->Thread->TempRegisters[1] = fm ? m : (int16_t)vm->Context->Clock->Minutes;
            return VMPrimitiveExitCode::GOTO_TRUE; //UNUSED?
        }
        case VMGenericTS1CallMode::HotDateChangeSuitsPermanentlyCall: { //24
            //temp 0: outfit type
            //temp 1: outfit index
            // TS1(sims): FreeSO VMTS1PurchasableOutfitHelper.SetSuit (simantics/engine/VMTS1Outfits). Returns the
            // previous index in temp 1. An index the Sim cannot wear changes nothing.
            auto r = VMTS1Outfits::SetSuit(AvatarCast(context->Caller, context), context->Thread->TempRegisters[0], context->Thread->TempRegisters[1]);
            context->Thread->TempRegisters[1] = (r == -2) ? (int16_t)-1 : r;
            return VMPrimitiveExitCode::GOTO_TRUE;
        }
        // 25. SaveSimPersistentData (motives, relationships?) — VMGenericTSOCall
        case VMGenericTS1CallMode::BuildVacationFamilyPutFamilyNumInTemp0: { //26
            //in our implementation, vacation lots build the family in the same way as normal lots.
            if (prov == nullptr) break;
            auto& crossData2 = prov->GameState;
            // TS1(sims): nobody travelling (the lot was opened directly): no family to build.
            // The ped marker has no "nobody came" branch (the original never opens a vacation lot without visitors):
            // wait here until a family arrives (VMTS1Travel::Arrive) instead of resetting it.
            if (crossData2.ActiveFamily == nullptr || crossData2.DowntownSimGUID == 0)
                return VMPrimitiveExitCode::CONTINUE_NEXT_TICK;
            if (crossData2.LotTransitInfo >= 1) {
                crossData2.ActiveFamily->SelectWholeFamily();
                VMTS1Travel::ActivateFamily(vm, crossData2.ActiveFamily);
                context->Thread->TempRegisters[0] = vm->GetGlobalValue(9);

                //set to 1 if we spawned a whole family.
                //seems to be from globals 34 on the lot we exited. Magic town uses 0 for a single sim, and 1 for whole family
                //(blimp, though 1 is still set for whole family when theres only one person in it!)

                VMTS1Travel::VerifyFamily(vm);
                for (auto e : vm->Entities) {
                    auto a = dynamic_cast<VMAvatar*>(e);
                    if (a != nullptr && a->Object->OBJ->GUID == crossData2.DowntownSimGUID) { VMTS1Travel::SelectSim(vm, a); break; }
                }
            } else {
                auto control2 = VMTS1Travel::CreatePerson(vm, crossData2.DowntownSimGUID);
                if (control2 != nullptr) {
                    crossData2.ActiveFamily->SelectOneMember(crossData2.DowntownSimGUID);
                    VMTS1Travel::ActivateFamily(vm, crossData2.ActiveFamily);
                    VMTS1Travel::SelectSim(vm, control2);
                }
                context->Thread->TempRegisters[0] = vm->GetGlobalValue(3);
            }

            context->Thread->TempRegisters[1] = (int16_t)((crossData2.LotTransitInfo >= 1) ? 1 : 0);
            break;
        }
        case VMGenericTS1CallMode::ReturnNumberOfAvaiableVacationLotsInTemp0: //27
            //TODO: vacation lots are disabled when other people have saved the game there! that's what this primitive checks.
            context->Thread->TempRegisters[0] = 9;
            break;
        // 28. ReturnZoningTypeOfLotInTemp0 — VMGenericTSOCall
        case VMGenericTS1CallMode::SetStackObjectsSuit: { //29
            // suit type in temp0, suit index in temp1. Returns old index in temp1. (FreeSO)
            auto r = VMTS1Outfits::SetSuit(AvatarCast(context->StackObject, context), context->Thread->TempRegisters[0], context->Thread->TempRegisters[1]);
            context->Thread->TempRegisters[1] = (r == -2) ? (int16_t)-1 : r;
            return VMPrimitiveExitCode::GOTO_TRUE;
        }
        case VMGenericTS1CallMode::GetStackObjectsSuit: //30
            context->Thread->TempRegisters[1] = VMTS1Outfits::GetSuitIndex(AvatarCast(context->StackObject, context), context->Thread->TempRegisters[0]);
            return VMPrimitiveExitCode::GOTO_TRUE;
        case VMGenericTS1CallMode::CountStackObjectSuits: //31
            // FreeSO writes the count to temp 0 (the label says temp 1; no shipped BHAV calls it).
            context->Thread->TempRegisters[0] = VMTS1Outfits::CountSuits(AvatarCast(context->StackObject, context), context->Thread->TempRegisters[0]);
            return VMPrimitiveExitCode::GOTO_TRUE;
        case VMGenericTS1CallMode::CreatePurchasedPetsNearOwner: { //32
            auto fam = VMTS1Neighbourhood::CurrentFamily(vm);
            if (fam == nullptr) return VMPrimitiveExitCode::GOTO_TRUE;
            fam->SelectWholeFamily();
            VMTS1Travel::VerifyFamily(vm);
            return VMPrimitiveExitCode::GOTO_TRUE;
        }
        // 33. AddToFamilyInTemp0 — VMGenericTSOCall
        case VMGenericTS1CallMode::PromoteFameIfNeeded: //34
        case VMGenericTS1CallMode::DemoteFameIfNeeded: { //36
            // TS1(sims): Superstar. Global 'Adjust Stack Object's Fame 1.8', ControllerStudioLot / PhonePluginStudioLots
            // 'Promotion Check', Charms 'Local adjust fame': the stack object's (else the caller's) star level (person data
            // 81) follows its fame score (80) and, for promotions, its friends' star power and skills, by the tables of
            // GameData/fame.iff; true = the level changed (the callers then show the promotion/demotion message).
            auto ava = dynamic_cast<VMAvatar*>(context->StackObject);
            if (ava == nullptr) ava = dynamic_cast<VMAvatar*>(context->Caller);
            if (ava == nullptr) return VMPrimitiveExitCode::GOTO_FALSE;
            auto old = ava->GetPersonData((VMPersonDataVariable)PD_FAME_LEVEL);
            auto level = FameLevelFor(ava, old, call == VMGenericTS1CallMode::PromoteFameIfNeeded);
            if (level == old) return VMPrimitiveExitCode::GOTO_FALSE;
            ava->SetPersonData((VMPersonDataVariable)PD_FAME_LEVEL, level);
            if (level > ava->GetPersonData((VMPersonDataVariable)PD_FAME_HIGHWATER)) ava->SetPersonData((VMPersonDataVariable)PD_FAME_HIGHWATER, level);
            VMTS1Travel::Signal(vm, VMTS1Travel::EventFameChanged, ava->ObjectID);
            return VMPrimitiveExitCode::GOTO_TRUE;
        }
        case VMGenericTS1CallMode::TakeTaxiHook: //35
            //not sure where this one is called, seems to have been added for studiotown
            break;
        case VMGenericTS1CallMode::CancelPieMenu: //37
            VMTS1Travel::Signal(vm, VMTS1Travel::EventCancelPieMenu, context->Caller ? context->Caller->ObjectID : 0); // TS1(sims)
            break;
        case VMGenericTS1CallMode::GetTokensFromString: { //38
            // TS1(sims): Makin' Magic. MagicMasterSpells 'Get Ingredients for Spell' / 'Get Spell From Ingredients' /
            // 'Get Ingredients for PressOven' / 'Get Recipe From Ingredients': temp 0 = STR# of the running object
            // (3 adult spell recipes, 4 kid charm recipes, 5/6 nectar & oven recipes), temp 1 = entry. The entry's comma
            // separated numbers go to temp 1, 2, 3...; temp 0 = how many (-1 when the table/entry does not exist).
            auto& temps = context->Thread->TempRegisters;
            auto table = context->ScopeResource()->Get<STR>((uint16_t)temps[0]);
            if (table == nullptr || temps[1] < 0 || temps[1] >= table->Length()) { temps[0] = -1; break; }
            auto str = table->GetString(temps[1]);
            int16_t n = 0;
            size_t p = 0;
            while (p < str.size() && n < (int16_t)temps.size() - 1) {
                auto c = str.find(',', p);
                auto tok = str.substr(p, (c == std::string::npos) ? std::string::npos : c - p);
                char* end = nullptr;
                auto v = strtol(tok.c_str(), &end, 10);
                if (end != tok.c_str()) temps[1 + n++] = (int16_t)v;
                if (c == std::string::npos) break;
                p = c + 1;
            }
            temps[0] = n;
            break;
        }
        case VMGenericTS1CallMode::ChildToAdult: { //39
            // TS1(sims): FreeSO leaves this empty. Temp 0 = the child (CharmsKid 'Grow Me', SocialsMagic 'Magic -
            // Kid to Adult'); Temp 1 = the grown Sim's object id (the charm then finds it a location). Same object.
            auto kid = dynamic_cast<VMAvatar*>(vm->GetObjectById(context->Thread->TempRegisters[0]));
            if (!VMTS1Outfits::ChildToAdult(kid, vm->Context)) return VMPrimitiveExitCode::GOTO_FALSE;
            context->Thread->TempRegisters[1] = kid->ObjectID;
            return VMPrimitiveExitCode::GOTO_TRUE;
        }
        case VMGenericTS1CallMode::PetToAdult: { //40
            auto pet = dynamic_cast<VMAvatar*>(vm->GetObjectById(context->Thread->TempRegisters[0]));
            if (!VMTS1Outfits::PetToAdult(pet)) return VMPrimitiveExitCode::GOTO_FALSE;
            context->Thread->TempRegisters[1] = pet->ObjectID;
            return VMPrimitiveExitCode::GOTO_TRUE;
        }
        case VMGenericTS1CallMode::HeadFlush: //41
            VMTS1Travel::Signal(vm, VMTS1Travel::EventHeadFlush, context->Caller ? context->Caller->ObjectID : 0); // TS1(sims)
            break;
        case VMGenericTS1CallMode::MakeTemp0SelectedSim: { //42
            //right now assume there's only one ts1 client, and that's us.
            auto target = vm->GetObjectById(context->Thread->TempRegisters[0]);
            if (target == nullptr || dynamic_cast<VMGameObject*>(target) != nullptr) return VMPrimitiveExitCode::GOTO_FALSE;
            VMTS1Travel::SelectSim(vm, static_cast<VMAvatar*>(target));
            return VMPrimitiveExitCode::GOTO_TRUE;
        }
        case VMGenericTS1CallMode::FamilySpellsIntoController: //43
            // TS1(sims): FreeSO leaves this empty. Called by the Magic Controller ('init tree' / 'load', on lots
            // with a family) to restore the family's made spells into its attributes 0..5.
            VMTS1Travel::LoadFamilySpells(vm, context->Caller);
            break;
        default:
            break;
    }
    return VMPrimitiveExitCode::GOTO_TRUE;
}

}}}
