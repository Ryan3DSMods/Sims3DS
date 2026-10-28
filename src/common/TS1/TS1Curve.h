/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// TS1(sims): translated from FreeSO tso.common/TS1/TS1Curve.cs (FreeSims has no TS1 autonomy curves).
// A piecewise linear curve read from a Global.iff STR# entry such as "(-100;-100) (-72;90) (100;90)".
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace FSO { namespace Common { namespace TS1 {

class TS1Curve {
public:
    struct Point { int32_t X = 0; int32_t Y = 0; };
    std::vector<Point> Points; // ordered by x

    int32_t CacheStart = 0;
    std::vector<float> Cache;
    std::vector<int32_t> CacheFixed;

    TS1Curve() {}
    explicit TS1Curve(const std::string& input);

    float GetPoint(float input) const;
    int32_t GetPointFixed(int32_t input) const;

    // CACHED ACCESS
    void BuildCache(int32_t min, int32_t max);
    float GetCachedPoint(int32_t point) const;
    int32_t GetCachedPointFixed(int32_t point) const;
};

}}}
