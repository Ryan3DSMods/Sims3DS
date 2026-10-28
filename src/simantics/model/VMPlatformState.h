/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Model.VMPlatformState (serialization dropped)
#pragma once
#include <memory>
namespace FSO { namespace SimAntics {
class VM;
namespace Model {
namespace Platform { class VMAbstractValidator; }
class VMPlatformState {
public:
    virtual ~VMPlatformState() {}
    std::shared_ptr<Platform::VMAbstractValidator> Validator;
    virtual bool LimitExceeded() const = 0;
    virtual void LimitExceeded(bool value) = 0;
    virtual bool CanPlaceNewUserObject(VM* /*vm*/) { return true; }
    virtual bool CanPlaceNewDonatedObject(VM* /*vm*/) { return false; }
    virtual void ActivateValidator(VM* vm) = 0;
    virtual void Tick(VM* vm, void* owner) = 0;
    virtual bool IsTSOEntityState() const { return false; } // C# `is VMTSOEntityState`
    virtual bool IsTSOLotState() const { return false; }    // C# `is VMTSOLotState`
};
}}}
