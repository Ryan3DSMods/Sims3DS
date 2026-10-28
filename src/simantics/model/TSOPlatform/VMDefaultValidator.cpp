/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Model.Platform.VMDefaultValidator
#include "simantics/model/TSOPlatform/VMDefaultValidator.h"
#include "simantics/model/TSOPlatform/VMTSOAvatarState.h"
#include "simantics/model/TSOPlatform/VMTSOObjectState.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/entities/VMGameObject.h"
#include "simantics/VM.h"
#include "content/ContentManager.h"
namespace FSO { namespace SimAntics { namespace Model { namespace Platform {
using namespace FSO::SimAntics::Model::TSOPlatform;

const std::unordered_set<int> VMDefaultValidator::RoomieWhiteList = { 12, 13, 14, 15, 16, 17, 18, 19, 20 };
const std::unordered_set<int> VMDefaultValidator::BuilderWhiteList = {
    12, 13, 14, 15, 16, 17, 18, 19, 20,
    0, 1, 2, 3, 4, 5, 7, 8, 9 //29 is terrain tool
};

bool VMDefaultValidator::CanBuildTool(VMAvatar* ava) { return ava->AvatarState->Permissions >= VMTSOAvatarPermissions::BuildBuyRoommate; }
bool VMDefaultValidator::CanChangePermissions(VMAvatar* ava) { return ava->AvatarState->Permissions >= VMTSOAvatarPermissions::Owner; }
bool VMDefaultValidator::CanManageAdmitList(VMAvatar* ava) { return ava->AvatarState->Permissions >= VMTSOAvatarPermissions::Roommate; }
bool VMDefaultValidator::CanManageAsyncSale(VMAvatar* ava, VMGameObject* obj) {
    if (ava == nullptr || obj == nullptr || obj->PersistID == 0) return false;
    return ava->AvatarState->Permissions >= VMTSOAvatarPermissions::Roommate && ava->PersistID == ((VMTSOObjectState*)obj->TSOState())->OwnerID;
}
bool VMDefaultValidator::CanManageEnvironment(VMAvatar* ava) { return ava->AvatarState->Permissions >= VMTSOAvatarPermissions::Roommate; }
bool VMDefaultValidator::CanManageLotSize(VMAvatar* ava) { return ava->AvatarState->Permissions >= VMTSOAvatarPermissions::Owner; }
bool VMDefaultValidator::CanMoveObject(VMAvatar* ava, VMEntity*) { return ava->AvatarState->Permissions >= VMTSOAvatarPermissions::Roommate; }
bool VMDefaultValidator::CanSendbackObject(VMAvatar* ava, VMGameObject*) { return ava->AvatarState->Permissions >= VMTSOAvatarPermissions::Roommate; }

DeleteMode VMDefaultValidator::GetDeleteMode(DeleteMode desired, VMAvatar* ava, VMEntity* obj) {
    if (desired > DeleteMode::Delete) return DeleteMode::Disallowed;
    if (obj == nullptr || ava == nullptr) return DeleteMode::Disallowed;
    bool isAvatar = dynamic_cast<VMAvatar*>(obj) != nullptr;
    if (desired == DeleteMode::Sendback) {
        if (obj->PersistID == 0 || isAvatar || !CanSendbackObject(ava, (VMGameObject*)obj)) desired = DeleteMode::Disallowed;
    } else if (desired == DeleteMode::Delete) {
        if (isAvatar)
            return (ava->AvatarState->Permissions < VMTSOAvatarPermissions::Admin) ? DeleteMode::Disallowed : DeleteMode::Delete;
        if (ava->AvatarState->Permissions < VMTSOAvatarPermissions::Roommate)
            desired = DeleteMode::Disallowed;
        else if (obj->PersistID != 0 && ava->PersistID != ((VMTSOObjectState*)obj->TSOState())->OwnerID && ava->AvatarState->Permissions != VMTSOAvatarPermissions::Admin)
            desired = DeleteMode::Sendback;
    }
    return desired;
}

PurchaseMode VMDefaultValidator::GetPurchaseMode(PurchaseMode desired, VMAvatar* ava, uint32_t guid, bool fromInventory) {
    if (desired > PurchaseMode::Donate) return PurchaseMode::Disallowed;
    if (ava == nullptr) return PurchaseMode::Disallowed;
    if (ava->AvatarState->Permissions < VMTSOAvatarPermissions::Roommate) return PurchaseMode::Disallowed;
    //todo: build/buy limits for inventory, but allow rare/non-catalog placement
    if (!vm->TS1 && !fromInventory) {
        auto catalog = FSO::Content::Content::Get()->WorldCatalog.get();
        auto item = catalog->GetItemByGUID(guid);
        auto& whitelist = (ava->AvatarState->Permissions == VMTSOAvatarPermissions::Roommate) ? RoomieWhiteList : BuilderWhiteList;
        if (item == nullptr || whitelist.find(item->Category) == whitelist.end()) {
            if (ava->AvatarState->Permissions != VMTSOAvatarPermissions::Admin) return PurchaseMode::Disallowed;
        }
    }
    return PurchaseMode::Normal;
}
}}}}
