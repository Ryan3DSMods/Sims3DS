/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Model.VMTuningCache
#pragma once
#include <cstdint>
#include <unordered_map>
#include "simantics/model/VMMotive.h"
namespace FSO { namespace SimAntics {
class VM;
namespace Model {
class VMTuningCache {
public:
    std::unordered_map<int32_t, float> MotiveOverfill;
    void UpdateTuning(VM* vm);
    int16_t GetLimit(VMMotive motive) const {
        auto it = MotiveOverfill.find((int32_t)motive);
        if (it != MotiveOverfill.end()) return (int16_t)it->second;
        return 100;
    }
};
}}}
