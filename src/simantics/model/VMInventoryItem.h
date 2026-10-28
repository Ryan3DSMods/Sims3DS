/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Model.VMInventoryItem (serialization dropped)
#pragma once
#include <cstdint>
#include <string>
namespace FSO { namespace SimAntics { namespace Model {
class VMInventoryItem {
public:
    uint32_t ObjectPID = 0;
    std::string Name;
    uint32_t GUID = 0;
    uint16_t Graphic = 0;
    uint32_t Value = 0;
    uint64_t DynFlags1 = 0;
    uint64_t DynFlags2 = 0;
};
}}}
