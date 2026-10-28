/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Model.TSOPlatform.VMTSOEntityState
#pragma once
#include "simantics/model/VMPlatformState.h"
#include "simantics/model/VMBudget.h"
namespace FSO { namespace SimAntics { namespace Model { namespace TSOPlatform {
class VMTSOEntityState : public VMPlatformState {
public:
    VMBudget Budget;
    bool LimitExceeded() const override { return _LimitExceeded; }
    void LimitExceeded(bool value) override { _LimitExceeded = value; }
    void Tick(VM*, void*) override {}
    void ActivateValidator(VM*) override {}
    bool IsTSOEntityState() const override { return true; }
private:
    bool _LimitExceeded = false;
};
}}}}
