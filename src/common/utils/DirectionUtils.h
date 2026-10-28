/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.Common.Utils.DirectionUtils
#pragma once
#include <cmath>
#include "core/math.h"
namespace FSO { namespace Common { namespace Utils {
class DirectionUtils {
public:
    /// Calculates the mathematical modulus of a value.
    static double PosMod(double x, double m) { return std::fmod(std::fmod(x, m) + m, m); }
    /// Finds the difference between two radian directions.
    static double Difference(double a, double b) {
        double value = PosMod(b - a, M_PI * 2);
        if (value > M_PI) value -= M_PI * 2;
        return value;
    }
    /// Normalizes a direction to the range -PI through PI.
    static double Normalize(double dir) {
        dir = PosMod(dir, M_PI * 2);
        if (dir > M_PI) dir -= M_PI * 2;
        return dir;
    }
    /// Normalizes a direction in degrees to the range -180 through 180.
    static double NormalizeDegrees(double dir) {
        dir = PosMod(dir, 360);
        if (dir > 180) dir -= 360;
        return dir;
    }
};
}}}
