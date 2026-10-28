/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Model.TSOPlatform.VMTSOObjectState
#include "simantics/model/TSOPlatform/VMTSOObjectState.h"
#include "simantics/model/VMBuildableAreaInfo.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMMultitileGroup.h"
namespace FSO { namespace SimAntics { namespace Model { namespace TSOPlatform {
void VMTSOObjectState::Donate(VM* vm, VMEntity* owner) {
    //remove all sellback value and set it as donated.
    owner->MultitileGroup->Price = 0;
    for (auto obj : owner->MultitileGroup->Objects) {
        auto st = (VMTSOObjectState*)obj->TSOState();
        st->ObjectFlags = (VMTSOObjectFlags)((uint8_t)st->ObjectFlags | (uint8_t)VMTSOObjectFlags::FSODonated);
    }
    VMBuildableAreaInfo::UpdateOverbudgetObjects(vm);
}
}}}}
