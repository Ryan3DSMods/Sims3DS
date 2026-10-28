/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Model.TSOPlatform.VMTSOObjectState / VMTSOObjectFlags
#pragma once
#include <cstdint>
#include "simantics/model/TSOPlatform/VMTSOEntityState.h"
namespace FSO { namespace SimAntics {
class VMEntity;
namespace Model { namespace TSOPlatform {
enum class VMTSOObjectFlags : uint8_t { FSODonated = 1 };
class VMTSOObjectState : public VMTSOEntityState {
public:
    //TODO: repair
    uint32_t OwnerID = 0;
    VMTSOObjectFlags ObjectFlags = (VMTSOObjectFlags)0;
    void Tick(VM* vm, void* owner) override { VMTSOEntityState::Tick(vm, owner); }
    void Donate(VM* vm, VMEntity* owner);
};
}}}}
