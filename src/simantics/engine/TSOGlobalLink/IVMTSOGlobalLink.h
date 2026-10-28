/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Engine.TSOTransaction.IVMTSOGlobalLink and its async callback delegates.
// STUB(3ds): TSO server link. The interface is kept so TSO-flavoured primitives (TransferFunds, InventoryOperations,
// InvokePlugin, GenericTSOCall) compile; on 3DS VM::GlobalLink() is always null and they fall back to the local stub.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include "simantics/model/TSOPlatform/VMTSOAvatarState.h"
namespace FSO { namespace SimAntics {
class VM; class VMAvatar;
namespace Entities { class VMMultitileGroup; }
namespace Engine { namespace TSOTransaction {

using VMAsyncTransactionCallback = std::function<void(bool success, int32_t transferAmount, uint32_t uid1, uint32_t budget1, uint32_t uid2, uint32_t budget2)>;
using VMAsyncAvatarCallback = std::function<void(uint32_t persistID, Model::TSOPlatform::VMTSOAvatarPermissions permissions)>; //TODO: VMPersistAvatarBlock
using VMAsyncPluginLoadCallback = std::function<void(const std::vector<uint8_t>* data)>; //null if none available
using VMAsyncInventorySaveCallback = std::function<void(bool success, uint32_t objid)>; //todo: failure reasons
using VMAsyncInventoryRetrieveCallback = std::function<void(uint32_t guid, const std::vector<uint8_t>* data)>;
using VMAsyncInventoryConsumeCallback = std::function<void(bool success, int32_t result)>; //todo: failure reasons
using VMAsyncDeleteObjectCallback = std::function<void(bool success)>; //todo: failure reasons

class IVMTSOGlobalLink {
public:
    virtual ~IVMTSOGlobalLink() {}
    virtual void LeaveLot(VM* vm, VMAvatar* avatar) = 0;
    virtual void PerformTransaction(VM* vm, bool testOnly, uint32_t uid1, uint32_t uid2, int32_t amount, VMAsyncTransactionCallback callback) = 0;
    virtual void RequestRoommate(VM* vm, VMAvatar* avatar) = 0;
    virtual void RemoveRoommate(VM* vm, VMAvatar* avatar) = 0;
    virtual void ObtainAvatarFromTicket(VM* vm, const std::string& ticket, VMAsyncAvatarCallback callback) = 0;
    // QueueArchitecture(VMNetArchitectureCmd) dropped with NetPlay.
    virtual void LoadPluginPersist(VM* vm, uint32_t objectPID, uint32_t pluginID, VMAsyncPluginLoadCallback callback) = 0;
    virtual void SavePluginPersist(VM* vm, uint32_t objectPID, uint32_t pluginID, const std::vector<uint8_t>& data) = 0;
    virtual void MoveToInventory(VM* vm, Entities::VMMultitileGroup* obj, VMAsyncInventorySaveCallback callback) = 0;
    virtual void ForceInInventory(VM* vm, uint32_t objectPID, VMAsyncInventorySaveCallback callback) = 0;
    virtual void UpdateObjectPersist(VM* vm, Entities::VMMultitileGroup* obj, VMAsyncInventorySaveCallback callback) = 0;
    virtual void PurchaseFromOwner(VM* vm, Entities::VMMultitileGroup* obj, uint32_t purchaserPID, VMAsyncInventorySaveCallback callback, VMAsyncTransactionCallback tcallback) = 0;
    virtual void RetrieveFromInventoryByType(VM* vm, uint32_t ownerPID, uint32_t guid, int32_t index, bool setOnLot, VMAsyncInventoryRetrieveCallback callback) = 0;
    virtual void RetrieveFromInventory(VM* vm, uint32_t objectPID, uint32_t ownerPID, bool setOnLot, VMAsyncInventoryRetrieveCallback callback) = 0;
    virtual void ConsumeInventory(VM* vm, uint32_t ownerPID, uint32_t guid, int32_t mode, int16_t num, VMAsyncInventoryConsumeCallback callback) = 0;
    virtual void DeleteObject(VM* vm, uint32_t objectPID, VMAsyncDeleteObjectCallback callback) = 0;
    virtual void Tick(VM* vm) = 0;
};

class VMInventoryRestoreObject {};
}}}}
