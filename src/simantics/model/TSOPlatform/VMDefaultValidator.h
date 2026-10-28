/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Model.Platform.VMDefaultValidator
#pragma once
#include <unordered_set>
#include "simantics/model/TSOPlatform/VMAbstractValidator.h"
namespace FSO { namespace SimAntics { namespace Model { namespace Platform {
class VMDefaultValidator : public VMAbstractValidator {
public:
    explicit VMDefaultValidator(VM* vm) : VMAbstractValidator(vm) {}
    DeleteMode GetDeleteMode(DeleteMode desired, VMAvatar* ava, VMEntity* obj) override;
    PurchaseMode GetPurchaseMode(PurchaseMode desired, VMAvatar* ava, uint32_t guid, bool fromInventory) override;
    bool CanBuildTool(VMAvatar* ava) override;
    bool CanMoveObject(VMAvatar* ava, VMEntity* obj) override;
    bool CanSendbackObject(VMAvatar* ava, VMGameObject* obj) override;
    bool CanManageAsyncSale(VMAvatar* ava, VMGameObject* obj) override;
    bool CanManageAdmitList(VMAvatar* ava) override;
    bool CanManageEnvironment(VMAvatar* ava) override;
    bool CanManageLotSize(VMAvatar* ava) override;
    bool CanChangePermissions(VMAvatar* ava) override;
protected:
    static const std::unordered_set<int> RoomieWhiteList;
    static const std::unordered_set<int> BuilderWhiteList;
};
}}}}
