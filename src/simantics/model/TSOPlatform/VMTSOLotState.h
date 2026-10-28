/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Model.TSOPlatform.VMTSOLotState
#pragma once
#include <cstdint>
#include <string>
#include <unordered_set>
#include "simantics/model/VMPlatformState.h"
namespace FSO { namespace SimAntics { namespace Model { namespace TSOPlatform {
class VMTSOLotState : public VMPlatformState {
public:
    std::string Name = "Lot";
    uint32_t LotID = 0;
    uint8_t TerrainType = 0;
    uint8_t PropertyCategory = 0;
    int32_t Size = 8;
    int32_t ObjectLimit = 0, DonateLimit = 0;
    bool LimitExceeded() const override { return _LimitExceeded; }
    void LimitExceeded(bool value) override { _LimitExceeded = value; }
    uint32_t OwnerID = 0;
    std::unordered_set<uint32_t> Roommates;
    std::unordered_set<uint32_t> BuildRoommates;

    void ActivateValidator(VM* vm) override;
    bool CanPlaceNewUserObject(VM* vm) override;
    bool CanPlaceNewDonatedObject(VM* vm) override;
    void Tick(VM*, void*) override {}
    bool IsTSOLotState() const override { return true; }
private:
    bool _LimitExceeded = false;
};
}}}}
