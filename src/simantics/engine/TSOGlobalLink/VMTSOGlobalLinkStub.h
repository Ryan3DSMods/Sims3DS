/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Engine.TSOTransaction.VMTSOGlobalLinkStub — local stand-in for the TSO server.
// Budget transactions are translated (they back Transfer Funds); network/chat parts are STUB(3ds).
#pragma once
#include "simantics/engine/TSOGlobalLink/IVMTSOGlobalLink.h"
#include "simantics/engine/TSOGlobalLink/VMTSOStandaloneDatabase.h"
namespace FSO { namespace SimAntics { namespace Engine { namespace TSOTransaction {
class VMTSOGlobalLinkStub : public IVMTSOGlobalLink {
public:
    VMTSOStandaloneDatabase Database;

    void PerformTransaction(VM* vm, bool testOnly, uint32_t uid1, uint32_t uid2, int32_t amount, VMAsyncTransactionCallback callback) override;
    bool PerformTransaction(VM* vm, bool testOnly, uint32_t uid1, uint32_t uid2, int32_t amount);
    void Tick(VM* vm) override;
    void LeaveLot(VM* vm, VMAvatar* avatar) override;
    void RequestRoommate(VM* vm, VMAvatar* avatar) override;
    void RemoveRoommate(VM* vm, VMAvatar* avatar) override;
    void ObtainAvatarFromTicket(VM* vm, const std::string& ticket, VMAsyncAvatarCallback callback) override;
    void LoadPluginPersist(VM* vm, uint32_t objectPID, uint32_t pluginID, VMAsyncPluginLoadCallback callback) override;
    void SavePluginPersist(VM* vm, uint32_t objectPID, uint32_t pluginID, const std::vector<uint8_t>& data) override;
    void MoveToInventory(VM* vm, Entities::VMMultitileGroup* obj, VMAsyncInventorySaveCallback callback) override;
    void PurchaseFromOwner(VM* vm, Entities::VMMultitileGroup* obj, uint32_t purchaserPID, VMAsyncInventorySaveCallback callback, VMAsyncTransactionCallback tcallback) override;
    void RetrieveFromInventory(VM* vm, uint32_t objectPID, uint32_t ownerPID, bool setOnLot, VMAsyncInventoryRetrieveCallback callback) override;
    void ForceInInventory(VM* vm, uint32_t objectPID, VMAsyncInventorySaveCallback callback) override;
    void DeleteObject(VM* vm, uint32_t objectPID, VMAsyncDeleteObjectCallback callback) override;
    void UpdateObjectPersist(VM* vm, Entities::VMMultitileGroup* obj, VMAsyncInventorySaveCallback callback) override;
    void RetrieveFromInventoryByType(VM* vm, uint32_t ownerPID, uint32_t guid, int32_t index, bool setOnLot, VMAsyncInventoryRetrieveCallback callback) override;
    void ConsumeInventory(VM* vm, uint32_t ownerPID, uint32_t guid, int32_t mode, int16_t num, VMAsyncInventoryConsumeCallback callback) override;
};
}}}}
