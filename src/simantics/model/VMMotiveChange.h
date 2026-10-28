/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Model.VMMotiveChange
#pragma once
#include <cstdint>
#include "simantics/model/VMMotive.h"
namespace FSO { namespace SimAntics {
class VM;
class VMAvatar;
namespace Model {
class VMMotiveChange {
public:
    int16_t PerHourChange = 0;
    int16_t MaxValue = 0;
    VMMotive Motive = VMMotive::HappyLife;
    bool Ticked = false;

    void Clear();
    void Tick(VMAvatar* avatar);
    static int32_t ScaleRate(VM* vm, int32_t rate, VMMotive type);
    static int16_t ScaleMax(VM* vm, int16_t oldMax, VMMotive type);
private:
    double fractional = 0;
    static int32_t ToFixed1000(float input) { return (int32_t)(input * 1000); }
    static int32_t FracMul(int32_t input, int32_t frac) { return (int32_t)((int64_t)input * frac) / 1000; }
};
}}}
