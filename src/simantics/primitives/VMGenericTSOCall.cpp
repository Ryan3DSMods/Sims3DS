// FSO.SimAntics.Primitives.VMGenericTSOCall (primitives/VMGenericTSOCall.cs)
/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
#include "simantics/primitives/VMGenericTSOCall.h"
#include "simantics/primitives/VMGenericTS1Call.h" // TS1(sims)
#include <cstring>
#include <algorithm>
#include "core/io.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMThread.h"
#include "simantics/engine/VMQueuedAction.h"
#include "simantics/engine/VMSimanticsException.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/entities/VMGameObject.h"
#include "simantics/entities/VMMultitileGroup.h"
#include "simantics/model/TSOPlatform/VMTSOAvatarState.h"
#include "simantics/model/TSOPlatform/VMTSOObjectState.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"
#include "simantics/engine/VMTS1Neighbourhood.h"
#include "content/ContentManager.h"
#include "content/TS1NeighbourProvider.h"

namespace FSO { namespace SimAntics { namespace Primitives {
using namespace FSO::SimAntics::Engine;
using namespace FSO::SimAntics::Model;
using namespace FSO::SimAntics::Model::TSOPlatform;

namespace {
// C# reference semantics: a member access on null throws NullReferenceException, which the
// thread's catch turns into a reset. Mirror that instead of dereferencing a null pointer.
template<class T> T* NotNull(T* e, VMStackFrame* context) {
    if (e == nullptr) throw VMSimanticsException("NullReferenceException", context);
    return e;
}
// C# hard cast `(VMAvatar)x`: null stays null, a wrong type throws InvalidCastException.
template<class T> T* HardCast(VMEntity* e, VMStackFrame* context) {
    if (e == nullptr) return nullptr;
    auto r = dynamic_cast<T*>(e);
    if (r == nullptr) throw VMSimanticsException("InvalidCastException", context);
    return r;
}
// C# `(VMTSOAvatarState)entity.TSOState` / `(VMTSOObjectState)entity.TSOState`
template<class T> T* StateCast(VMEntity* e, VMStackFrame* context) {
    auto state = NotNull(e, context)->TSOState();
    if (state == nullptr) return nullptr;
    auto r = dynamic_cast<T*>(state);
    if (r == nullptr) throw VMSimanticsException("InvalidCastException", context);
    return r;
}
}

VMPrimitiveExitCode VMGenericTSOCall::Execute(VMStackFrame* context, VMPrimitiveOperand* args) {
    auto operand = static_cast<VMGenericTSOCallOperand*>(args);

    // ADDED(sims): The Sims 1 "Generic Sims Call" (opcode 1). FreeSims only implements the
    // TSO meanings of these modes; TS1 shares modes 0-16 but reuses 17+ for its own (expansion)
    // calls, and assigns different meanings to 4-7 (family management). Without this, TS1 global
    // code such as "Is lot a Residential Lot?" (global tree 321, mode 28) hit TSO handlers and reset.
    if (context->VM()->TS1) {
        uint8_t call = (uint8_t)operand->Call;
        switch (call) {
            case 1: case 2: case 13: case 14:            // same meaning in TS1 and TSO: use the TSO code below
                break;
            case 4: { // Add to family: the person in the stack object (avatar, or neighbour id) joins the family
                      // living on this lot. TS1(sims): SocialInteractions 'do move in', Baby 'create child',
                      // Tutorial 'add Betty' (FreeSims: TSO "is stack object a roommate").
                int16_t nid; uint32_t guid;
                auto fam = FSO::SimAntics::Engine::Utils::VMTS1Neighbourhood::CurrentFamily(context->VM());
                if (fam == nullptr || !FSO::SimAntics::Engine::Utils::VMTS1Neighbourhood::ResolvePerson(context, nid, guid)) return VMPrimitiveExitCode::GOTO_TRUE;
                FSO::SimAntics::Engine::Utils::VMTS1Neighbourhood::MoveToFamily(context->VM(), nid, guid, (int16_t)fam->ChunkID);
                auto& r = fam->RuntimeSubset;
                if (std::find(r.begin(), r.end(), guid) == r.end()) r.push_back(guid);
                return VMPrimitiveExitCode::GOTO_TRUE;
            }
            case 5: { // Combine assets of family in temp 0: that family's money joins the lot's family and the
                      // old family record is emptied ('do move in', after its members were added with call 4).
                auto prov = FSO::SimAntics::Engine::Utils::VMTS1Neighbourhood::Provider();
                auto fam = FSO::SimAntics::Engine::Utils::VMTS1Neighbourhood::CurrentFamily(context->VM());
                auto other = (prov == nullptr) ? nullptr : prov->GetFamily(context->Thread->TempRegisters[0]);
                if (fam == nullptr || other == nullptr || other == fam) return VMPrimitiveExitCode::GOTO_TRUE;
                fam->Budget += other->Budget;
                fam->NetWorth += other->NetWorth;
                fam->FamilyFriends = std::max(fam->FamilyFriends, other->FamilyFriends);
                other->Budget = 0;
                other->NetWorth = 0;
                other->FamilyGUIDs.clear();
                other->RuntimeSubset.clear();
                // the lot's budget lives in globals 13/14 ("Budget Mod/Div 10000") while the family is loaded
                context->VM()->SetGlobalValue(FSO::SimAntics::Engine::Utils::VMTS1Neighbourhood::GLOBAL_BUDGET_MOD, (int16_t)(fam->Budget % 10000));
                context->VM()->SetGlobalValue(FSO::SimAntics::Engine::Utils::VMTS1Neighbourhood::GLOBAL_BUDGET_DIV, (int16_t)(fam->Budget / 10000));
                return VMPrimitiveExitCode::GOTO_TRUE;
            }
            case 6: { // Remove from family: 'kill person for good', 'do kick out', 'check for military school'
                      // (the stack object id holds the leaver's neighbour id).
                int16_t nid; uint32_t guid;
                if (!FSO::SimAntics::Engine::Utils::VMTS1Neighbourhood::ResolvePerson(context, nid, guid)) return VMPrimitiveExitCode::GOTO_TRUE;
                FSO::SimAntics::Engine::Utils::VMTS1Neighbourhood::MoveToFamily(context->VM(), nid, guid, 0);
                return VMPrimitiveExitCode::GOTO_TRUE;
            }
            case 7: { // Make new neighbor ("depracated" in Behavior.iff; no shipped BHAV calls it): gives the stack
                      // object avatar a neighbour record if it has none.
                auto ava = dynamic_cast<VMAvatar*>(context->StackObject);
                if (ava != nullptr) FSO::SimAntics::Engine::Utils::VMTS1Neighbourhood::MakeNewNeighbor(ava);
                return VMPrimitiveExitCode::GOTO_TRUE;
            }
            case 25: { // Save This Sim's Persistent Data: person data back into the neighbour record.
                auto ava = dynamic_cast<VMAvatar*>(context->Caller);
                if (ava != nullptr) FSO::SimAntics::Engine::Utils::VMTS1Neighbourhood::SavePersistentData(ava);
                return VMPrimitiveExitCode::GOTO_TRUE;
            }
            case 33: { // Add to Family in Temp 0: the stack object person moves to family temp 0 (pets: 'Run Away').
                int16_t nid; uint32_t guid;
                if (!FSO::SimAntics::Engine::Utils::VMTS1Neighbourhood::ResolvePerson(context, nid, guid)) return VMPrimitiveExitCode::GOTO_TRUE;
                FSO::SimAntics::Engine::Utils::VMTS1Neighbourhood::MoveToFamily(context->VM(), nid, guid, context->Thread->TempRegisters[0]);
                return VMPrimitiveExitCode::GOTO_TRUE;
            }
            case 12: // Get distance to camera in temp0 (used for sound/LOD falloff)
                context->Thread->TempRegisters[0] = 0;
                return VMPrimitiveExitCode::GOTO_TRUE;
            case 28: { // Return zoning type of lot (lot id in temp0) in temp0; 0 = residential, 1 = community.
                // TS1(sims): LotZoning.iff (user) / MasterLotZoning.iff; unknown lots are residential.
                auto prov = FSO::SimAntics::Engine::Utils::VMTS1Neighbourhood::Provider();
                context->Thread->TempRegisters[0] = (prov == nullptr) ? (int16_t)0 : prov->GetZoning(context->Thread->TempRegisters[0]);
                return VMPrimitiveExitCode::GOTO_TRUE;
            }
            default:
                // TS1(sims): every other TS1 mode (0, 3, 8-11, 15-24, 26-27, 29-32, 34-43): primitives/VMGenericTS1Call.
                return VMGenericTS1Call::Execute(context, (FSO::SimAntics::Model::VMGenericTS1CallMode)call);
        }
    }

    switch (operand->Call) {
        // 0. HOUSE TUTORIAL COMPLETE
        case VMGenericTSOCallMode::SwapMyAndStackObjectsSlots: { //1
            auto cont1 = context->Caller->Container;
            auto cont2 = NotNull(context->StackObject, context)->Container;
            auto contS1 = context->Caller->ContainerSlot;
            auto contS2 = context->StackObject->ContainerSlot;
            if (cont1 != nullptr && cont2 != nullptr) {
                context->Caller->PrePositionChange(context->VM()->Context);
                context->StackObject->PrePositionChange(context->VM()->Context);
                cont1->ClearSlot(contS1);
                cont2->ClearSlot(contS2);
                cont1->PlaceInSlot(context->StackObject, contS1, true, context->VM()->Context);
                cont2->PlaceInSlot(context->Caller, contS2, true, context->VM()->Context);
            }
            return VMPrimitiveExitCode::GOTO_TRUE;
        }
        case VMGenericTSOCallMode::SetActionIconToStackObject: //2
            if (context->Thread->ActiveAction() == nullptr) throw VMSimanticsException("ArgumentOutOfRangeException: Queue[0]", context);
            context->Thread->ActiveAction()->IconOwner(context->StackObject); // TS1(sims): FreeSO ActiveAction
            return VMPrimitiveExitCode::GOTO_TRUE;
        // 3. DO NOT USE
        case VMGenericTSOCallMode::IsStackObjectARoommate: { //4
            if (dynamic_cast<VMGameObject*>(NotNull(context->StackObject, context)) != nullptr)
                return VMPrimitiveExitCode::GOTO_FALSE;
            return (NotNull(StateCast<VMTSOAvatarState>(context->StackObject, context), context)->Permissions < VMTSOAvatarPermissions::Roommate)
                ? VMPrimitiveExitCode::GOTO_FALSE : VMPrimitiveExitCode::GOTO_TRUE;
        }
        // 5. Combine Assets of family in Temp0
        // 6. Remove From Family
        // 7. Make New Neighbour
        // 8. Family Sims1 Tutorial Complete
        // 9. Architecture Sims1 Tutorial Complete
        // 10. Disable Build/Buy
        // 11. Enable Build/Buy
        // 12. Distance to camera in Temp0
        // 13. Abort Interactions
        case VMGenericTSOCallMode::AbortInteractions:
            return VMPrimitiveExitCode::GOTO_TRUE;
        // 14. House Radio Station equals Temp0 (TODO)
        case VMGenericTSOCallMode::HouseRadioStationEqualsTemp0:
            context->VM()->SetGlobalValue(31, context->Thread->TempRegisters[0]);
            return VMPrimitiveExitCode::GOTO_TRUE;
        // 15. My Routing Footprint equals Temp0
        // 16. Change Normal Output
        case VMGenericTSOCallMode::GetInteractionResult: //17
            context->Thread->TempRegisters[0] = 2; //0=none, 1=reject, 2=accept, 3=pet
            return VMPrimitiveExitCode::GOTO_TRUE;
        case VMGenericTSOCallMode::SetInteractionResult: //18
            //todo: set interaction result to value of temp 0. UNUSED.
            return VMPrimitiveExitCode::GOTO_TRUE;
        case VMGenericTSOCallMode::DoIOwnThisObject: { //19
            bool notOwned = dynamic_cast<VMAvatar*>(NotNull(context->StackObject, context)) != nullptr
                || NotNull(StateCast<VMTSOObjectState>(context->StackObject, context), context)->OwnerID != context->Caller->PersistID;
            context->Thread->TempRegisters[0] = notOwned ? (int16_t)0 : (int16_t)1;
            return VMPrimitiveExitCode::GOTO_TRUE;
        }
        case VMGenericTSOCallMode::DoesTheLocalMachineSimOwnMe: //20
            context->Thread->TempRegisters[1] = 1; //TODO
            return VMPrimitiveExitCode::GOTO_TRUE;
        case VMGenericTSOCallMode::MakeMeStackObjectsOwner: //21
            if (dynamic_cast<VMAvatar*>(NotNull(context->StackObject, context)) != nullptr) return VMPrimitiveExitCode::GOTO_TRUE;
            NotNull(StateCast<VMTSOObjectState>(context->StackObject, context), context)->OwnerID = context->Caller->PersistID;
            return VMPrimitiveExitCode::GOTO_TRUE;
        //TODO: may need to update in global server
        // 22. Get Permissions (TODO)
        // 23. Set Permissions (TODO)
        case VMGenericTSOCallMode::AskStackObjectToBeRoommate: //24
            //0 = initiate. 1 = accept. 2 = reject.
            // STUB(3ds): GlobalLink (TSO server) is always null on 3DS, so this never fires.
            if (context->Thread->TempRegisters[0] == 1 && context->VM()->GlobalLink() != nullptr)
                context->VM()->GlobalLink()->RequestRoommate(context->VM(), HardCast<VMAvatar>(context->StackObject, context));
            return VMPrimitiveExitCode::GOTO_TRUE;

        case VMGenericTSOCallMode::LeaveLot: { //25
            auto caller = NotNull(HardCast<VMAvatar>(context->Caller, context), context);
            caller->UserLeaveLot();
            if (context->VM()->GlobalLink() != nullptr) {
                context->VM()->GlobalLink()->LeaveLot(context->VM(), caller);
            } else {
                // use our stub to remove the sim and potentially disconnect the client.
                context->VM()->CheckGlobalLink.LeaveLot(context->VM(), caller);
            }
            return VMPrimitiveExitCode::GOTO_TRUE;
        }

        // 26. UNUSED
        case VMGenericTSOCallMode::KickoutRoommate:
            // STUB(3ds): GlobalLink (TSO server) is always null on 3DS, so this never fires.
            if (context->VM()->GlobalLink() != nullptr)
                context->VM()->GlobalLink()->RemoveRoommate(context->VM(), HardCast<VMAvatar>(context->StackObject, context));
            return VMPrimitiveExitCode::GOTO_TRUE;

        case VMGenericTSOCallMode::KickoutVisitor:
            if (context->VM()->GlobalLink() != nullptr) {
                // STUB(3ds): C# `var server = (VMServerDriver)context.VM.Driver; server.KickUser(context.VM, context.StackObject.Name);`
                // NetPlay drivers are not ported; GlobalLink() is always null on 3DS so this branch is dead.
            }
            NotNull(HardCast<VMAvatar>(context->StackObject, context), context)->UserLeaveLot();
            return VMPrimitiveExitCode::GOTO_TRUE;

        case VMGenericTSOCallMode::StackObjectOwnerID: //29
            //attempt to find owner on lot. null stack object if not present
            if (dynamic_cast<VMAvatar*>(NotNull(context->StackObject, context)) != nullptr) context->Thread->TempRegisters[0] = 0;
            else {
                auto owner = context->VM()->GetObjectByPersist(NotNull(StateCast<VMTSOObjectState>(context->StackObject, context), context)->OwnerID);
                context->Thread->TempRegisters[0] = (owner == nullptr) ? (int16_t)0 : owner->ObjectID;
            }
            return VMPrimitiveExitCode::GOTO_TRUE;

        //30. Create Cheat Neighbour
        case VMGenericTSOCallMode::IsTemp0AvatarIgnoringTemp1Avatar: //31
            context->Thread->TempRegisters[0] = 0;
            return VMPrimitiveExitCode::GOTO_TRUE;
        //32. Play Next Song on Radio Station in Temp 0 (TODO)
        //33. Temp0 Avatar Unignore Temp1 Avatar
        case VMGenericTSOCallMode::GlobalRepairCostInTempXL0: //34
            context->Thread->TempXL[0] = 0; //TODO
            return VMPrimitiveExitCode::GOTO_TRUE;
        //35. Global Repair State (TODO)
        case VMGenericTSOCallMode::IsGlobalBroken: //36
            return VMPrimitiveExitCode::GOTO_FALSE; //TODO
        // 37. UNUSED
        case VMGenericTSOCallMode::MayAddRoommate: //38
            // TODO: Make only lot owner able to do this
            // for testing purposes, build roommates are kind of "global moderators"
            // also, can't add roommate if we have 8 roommates.
            context->Thread->TempRegisters[0] = 2;
            // 2 is "true". not sure what 1 is. (interaction shows up, but fails on trying to run it. likely "guessed" state for client)
            return VMPrimitiveExitCode::GOTO_TRUE;
        case VMGenericTSOCallMode::ReturnLotCategory: //39
            context->Thread->TempRegisters[0] = 6; //skills lot. see #Lot Types in global.iff
            //TODO: set based on lot state
            return VMPrimitiveExitCode::GOTO_TRUE;
        case VMGenericTSOCallMode::TestStackObject: //40
            return (context->StackObject != nullptr) ? VMPrimitiveExitCode::GOTO_TRUE : VMPrimitiveExitCode::GOTO_FALSE;
        case VMGenericTSOCallMode::GetCurrentValue: //41
            //stack object price in TempXL 0
            context->Thread->TempXL[0] = NotNull(NotNull(context->StackObject, context)->MultitileGroup, context)->Price;
            return VMPrimitiveExitCode::GOTO_TRUE;
        case VMGenericTSOCallMode::IsRegionEmpty: //42
            //is temp0 radius (in full tiles) around stack object empty?
            //used for resurrect. TODO.
            return VMPrimitiveExitCode::GOTO_TRUE;
        //43. Set Spotlight Status (TODO: GLOBAL SERVER)
        //44. Is Full Refund (TODO: small grace period after purchase/until user exits buy mode)
        //45. Refresh buy/build (TODO? we probably don't need this)
        case VMGenericTSOCallMode::GetLotOwner: //46
            // TODO! global lot state not in yet
            context->Thread->TempRegisters[0] = context->Caller->ObjectID;
            return VMPrimitiveExitCode::GOTO_TRUE;
        case VMGenericTSOCallMode::CopyDynObjNameFromTemp0ToTemp1: { //47
            auto obj1 = context->VM()->GetObjectById(context->Thread->TempRegisters[0]);
            auto obj2 = context->VM()->GetObjectById(context->Thread->TempRegisters[1]);
            NotNull(obj2, context)->Name(NotNull(obj1, context)->Name());
            return VMPrimitiveExitCode::GOTO_TRUE;
        }
        case VMGenericTSOCallMode::GetIsPendingDeletion: //48
            return VMPrimitiveExitCode::GOTO_FALSE;
        //49. Pet Ran Away
        //50. Set Original Purchase Price (TODO: parrot, snowman. should set to temp0)
        case VMGenericTSOCallMode::HasTemporaryID: //51
            //persist ID should be 0 til we get one.
            return (NotNull(context->StackObject, context)->PersistID == 0) ? VMPrimitiveExitCode::GOTO_TRUE : VMPrimitiveExitCode::GOTO_FALSE;
        case VMGenericTSOCallMode::SetStackObjOwnerToTemp0: { //52
            auto obj = context->VM()->GetObjectById(context->Thread->TempRegisters[0]);
            if (dynamic_cast<VMAvatar*>(NotNull(context->StackObject, context)) != nullptr || obj == nullptr) return VMPrimitiveExitCode::GOTO_TRUE;

            NotNull(StateCast<VMTSOObjectState>(context->StackObject, context), context)->OwnerID = obj->PersistID;
            return VMPrimitiveExitCode::GOTO_TRUE;
        }
        //53. Is On Editable Tile
        //54. Set Stack Object's Crafter Name To Avatar in Temp 0
        case VMGenericTSOCallMode::CalcHarvestComponents: //55
            //TODO: where does this come from?
            context->Thread->TempRegisters[0] = 1;
            context->Thread->TempRegisters[1] = 1;
            context->Thread->TempRegisters[2] = 1;
            return VMPrimitiveExitCode::GOTO_TRUE;
        case VMGenericTSOCallMode::IsStackObjectForSale: //56. TODO
            return VMPrimitiveExitCode::GOTO_FALSE;

        //TODO: may need to update in global server
        default:
            return VMPrimitiveExitCode::GOTO_TRUE;
    }
}

void VMGenericTSOCallOperand::Read(const uint8_t* bytes) {
    st::Reader io(bytes, OPERAND_SIZE, st::Endian::Little);
    Call = (VMGenericTSOCallMode)io.u8();
}

void VMGenericTSOCallOperand::Write(uint8_t* bytes) {
    st::Writer io(st::Endian::Little);
    io.WriteByte((uint8_t)Call);
    auto& a = io.ToArray();
    std::memcpy(bytes, a.data(), std::min(a.size(), OPERAND_SIZE));
}

}}}
