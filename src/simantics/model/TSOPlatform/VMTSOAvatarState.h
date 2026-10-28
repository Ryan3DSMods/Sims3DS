/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Model.TSOPlatform.VMTSOAvatarState / VMTSOAvatarPermissions
#pragma once
#include <cstdint>
#include <unordered_set>
#include "core/math.h"
#include "simantics/model/TSOPlatform/VMTSOEntityState.h"
namespace FSO { namespace SimAntics { namespace Model { namespace TSOPlatform {
enum class VMTSOAvatarPermissions : uint8_t { Visitor = 0, Roommate = 1, BuildBuyRoommate = 2, Owner = 3, Admin = 4 };
class VMTSOAvatarState : public VMTSOEntityState {
public:
    VMTSOAvatarPermissions Permissions = VMTSOAvatarPermissions::Visitor;
    std::unordered_set<uint32_t> IgnoredAvatars;
    st::Color ChatColor = st::Color::White();
    void Tick(VM* vm, void* owner) override { VMTSOEntityState::Tick(vm, owner); }
};
}}}}
