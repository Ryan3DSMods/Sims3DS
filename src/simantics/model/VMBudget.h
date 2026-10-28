/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Model.VMBudget
#pragma once
#include <cstdint>
namespace FSO { namespace SimAntics { namespace Model {
class VMBudget {
public:
    uint32_t Value = 0;
    bool CanTransact(int32_t value) const { return ((int64_t)Value + value >= 0); }
    bool Transaction(int32_t value) {
        if (!CanTransact(value)) return false;
        Value = (uint32_t)(Value + value);
        return true;
    }
};
}}}
