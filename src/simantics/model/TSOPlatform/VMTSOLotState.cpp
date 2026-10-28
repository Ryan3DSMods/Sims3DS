/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Model.TSOPlatform.VMTSOLotState
#include "simantics/model/TSOPlatform/VMTSOLotState.h"
#include "simantics/model/TSOPlatform/VMDefaultValidator.h"
#include "simantics/model/VMObjectQueries.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"
namespace FSO { namespace SimAntics { namespace Model { namespace TSOPlatform {
void VMTSOLotState::ActivateValidator(VM* vm) {
    switch (PropertyCategory) {
        default:
            Validator = std::make_shared<Platform::VMDefaultValidator>(vm); break;
    }
}
bool VMTSOLotState::CanPlaceNewUserObject(VM* vm) { return (vm->Context->ObjectQueries->NumUserObjects() < ObjectLimit); }
bool VMTSOLotState::CanPlaceNewDonatedObject(VM* vm) { return (vm->Context->ObjectQueries->NumDonatedObjects() < DonateLimit); }
}}}}
