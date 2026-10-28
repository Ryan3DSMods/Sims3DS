/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Model.TSOPlatform.VMAsyncState (serialization dropped)
#pragma once
#include <cstdint>
namespace FSO { namespace SimAntics { namespace Model { namespace TSOPlatform {
enum class VMAsyncStateType : uint8_t { TransferFunds = 0, DialogResult = 1, PluginState = 2,
    InventoryOpState = 3 // ADDED(primitives): C# VMInventoryOpState has no VMAsyncStateType (it is not in TypeResolve); StateType() is pure here, so it needs a tag.
};
class VMAsyncState {
public:
    virtual ~VMAsyncState() {}
    bool Responded = false;
    int32_t WaitTime = 0;
    int32_t Version = 0;
    virtual VMAsyncStateType StateType() const = 0;
};
}}}}
