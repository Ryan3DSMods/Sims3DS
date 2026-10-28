/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Engine.TSOTransaction.VMTSOGlobalLinkStub
#include "simantics/engine/TSOGlobalLink/VMTSOGlobalLinkStub.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/entities/VMMultitileGroup.h"
#include "simantics/model/TSOPlatform/VMTSOLotState.h"
#include "simantics/model/TSOPlatform/VMTSOEntityState.h"
#include <climits>
namespace FSO { namespace SimAntics { namespace Engine { namespace TSOTransaction {
using namespace FSO::SimAntics::Model::TSOPlatform;

void VMTSOGlobalLinkStub::PerformTransaction(VM* vm, bool testOnly, uint32_t uid1, uint32_t uid2, int32_t amount, VMAsyncTransactionCallback callback) {
    auto result = PerformTransaction(vm, testOnly, uid1, uid2, amount);
    if (callback) {
        auto obj1 = vm->GetObjectByPersist(uid1);
        auto obj2 = vm->GetObjectByPersist(uid2);
        auto finalAmount = amount;
        // C# invoked the callback on a new thread; single-threaded here, call synchronously.
        callback(result, finalAmount,
            uid1, (obj1 == nullptr || obj1->TSOState() == nullptr) ? 0 : obj1->TSOState()->Budget.Value,
            uid2, (obj2 == nullptr || obj2->TSOState() == nullptr) ? 0 : obj2->TSOState()->Budget.Value);
    }
}

bool VMTSOGlobalLinkStub::PerformTransaction(VM* vm, bool testOnly, uint32_t uid1, uint32_t uid2, int32_t amount) {
    auto obj1 = vm->GetObjectByPersist(uid1);
    auto obj2 = vm->GetObjectByPersist(uid2);

    // max value ID is "maxis"
    if ((uid1 != UINT32_MAX && obj1 == nullptr) || (uid2 != UINT32_MAX && obj2 == nullptr)) return false;

    // fail if the target is an object and they're not on lot
    if (obj1 != nullptr && obj1->TSOState() != nullptr && !obj1->TSOState()->Budget.CanTransact(-amount)) return false;
    if (obj2 != nullptr && obj2->TSOState() != nullptr && !obj2->TSOState()->Budget.CanTransact(amount)) return false;

    if (!testOnly) {
        if (obj1 != nullptr && obj1->TSOState() != nullptr) obj1->TSOState()->Budget.Transaction(-amount);
        if (obj2 != nullptr && obj2->TSOState() != nullptr) obj2->TSOState()->Budget.Transaction(amount);
    }
    return true;
}

void VMTSOGlobalLinkStub::Tick(VM* /*vm*/) {
    // STUB(3ds): drained the queued VMNetArchitectureCmd buffer, charging budgets and forwarding net commands.
}

void VMTSOGlobalLinkStub::LeaveLot(VM* vm, VMAvatar* avatar) {
    //TODO: in the global server, this will save the avatar (and possibly lot) states and send back to server.
    // STUB(3ds): vm.CloseNet(LeaveLot) when it's us; VMChatEvent Leave.
    avatar->Delete(true, vm->Context);
}

void VMTSOGlobalLinkStub::RequestRoommate(VM* /*vm*/, VMAvatar* avatar) {
    //in final game: signal to city server persistant roommate request state.
    //right now: immedaiately add as roommate (C# forwarded a VMChangePermissionsCmd)
    avatar->AvatarState->Permissions = VMTSOAvatarPermissions::Roommate;
}

void VMTSOGlobalLinkStub::RemoveRoommate(VM* /*vm*/, VMAvatar* avatar) {
    //in final game: signal to city server persistant roommate request state.
    //right now: immedaiately remove
    avatar->AvatarState->Permissions = VMTSOAvatarPermissions::Visitor;
}

void VMTSOGlobalLinkStub::ObtainAvatarFromTicket(VM* vm, const std::string& ticket, VMAsyncAvatarCallback callback) {
    //gets an avatar from our stub database based on their ticket
    auto uid = Database.FindOrAddAvatar(ticket);

    auto permissions = VMTSOAvatarPermissions::Visitor;
    if (Database.Administrators.count(uid)) permissions = VMTSOAvatarPermissions::Admin;
    else if (vm->TSOState()->BuildRoommates.count(uid)) permissions = VMTSOAvatarPermissions::BuildBuyRoommate;
    else if (vm->TSOState()->Roommates.count(uid)) permissions = VMTSOAvatarPermissions::Roommate;

    if (callback) callback(uid, permissions);
}

void VMTSOGlobalLinkStub::LoadPluginPersist(VM* /*vm*/, uint32_t objectPID, uint32_t pluginID, VMAsyncPluginLoadCallback callback) {
    auto dat = Database.LoadPluginPersist(objectPID, pluginID);
    if (callback) callback(dat);
}

void VMTSOGlobalLinkStub::SavePluginPersist(VM* /*vm*/, uint32_t objectPID, uint32_t pluginID, const std::vector<uint8_t>& data) {
    Database.SavePluginPersist(objectPID, pluginID, data);
}

void VMTSOGlobalLinkStub::MoveToInventory(VM* /*vm*/, Entities::VMMultitileGroup* obj, VMAsyncInventorySaveCallback callback) {
    if (callback) callback(true, obj->BaseObject()->PersistID);
    //todo: nice stub for this using database?
}

void VMTSOGlobalLinkStub::PurchaseFromOwner(VM* /*vm*/, Entities::VMMultitileGroup* obj, uint32_t /*purchaserPID*/, VMAsyncInventorySaveCallback callback, VMAsyncTransactionCallback /*tcallback*/) {
    if (callback) callback(true, obj->BaseObject()->PersistID);
    //todo: nice stub for this using database?
}

void VMTSOGlobalLinkStub::RetrieveFromInventory(VM*, uint32_t, uint32_t, bool, VMAsyncInventoryRetrieveCallback callback) {
    //todo: nice stub for this using database?
    if (callback) callback(0, nullptr);
}

void VMTSOGlobalLinkStub::ForceInInventory(VM*, uint32_t, VMAsyncInventorySaveCallback) {
    //todo: nice stub for this using database?
}

void VMTSOGlobalLinkStub::DeleteObject(VM*, uint32_t, VMAsyncDeleteObjectCallback) {
    //todo: delete local data
}

void VMTSOGlobalLinkStub::UpdateObjectPersist(VM*, Entities::VMMultitileGroup*, VMAsyncInventorySaveCallback) {
}

void VMTSOGlobalLinkStub::RetrieveFromInventoryByType(VM*, uint32_t, uint32_t, int32_t, bool, VMAsyncInventoryRetrieveCallback) {
}

void VMTSOGlobalLinkStub::ConsumeInventory(VM*, uint32_t, uint32_t, int32_t, int16_t, VMAsyncInventoryConsumeCallback) {
    //todo: nice stub for this using database?
}
}}}}
