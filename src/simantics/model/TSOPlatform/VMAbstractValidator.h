/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Model.Platform.VMAbstractValidator / DeleteMode / PurchaseMode
#pragma once
#include <cstdint>
namespace FSO { namespace SimAntics {
class VM; class VMAvatar; class VMEntity; class VMGameObject;
namespace Model { namespace Platform {
enum class DeleteMode : uint8_t { Disallowed = 0, Sendback, Delete };
enum class PurchaseMode : uint8_t { Disallowed = 0, Normal, Donate };
class VMAbstractValidator {
public:
    explicit VMAbstractValidator(VM* vm_) : vm(vm_) {}
    virtual ~VMAbstractValidator() {}
    virtual DeleteMode GetDeleteMode(DeleteMode desired, VMAvatar* ava, VMEntity* obj) = 0;
    virtual PurchaseMode GetPurchaseMode(PurchaseMode desired, VMAvatar* ava, uint32_t guid, bool fromInventory) = 0;
    virtual bool CanBuildTool(VMAvatar* ava) = 0;
    virtual bool CanMoveObject(VMAvatar* ava, VMEntity* obj) = 0;
    virtual bool CanSendbackObject(VMAvatar* ava, VMGameObject* obj) = 0;
    virtual bool CanManageAsyncSale(VMAvatar* ava, VMGameObject* obj) = 0;
    virtual bool CanManageAdmitList(VMAvatar* ava) = 0;
    virtual bool CanManageEnvironment(VMAvatar* ava) = 0;
    virtual bool CanManageLotSize(VMAvatar* ava) = 0;
    virtual bool CanChangePermissions(VMAvatar* ava) = 0;
protected:
    VM* vm;
};
}}}}
