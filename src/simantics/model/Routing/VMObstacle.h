/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Model.Routing.VMObstacle
#pragma once
#include <cstdint>
#include <algorithm>
#include "core/math.h"
namespace FSO { namespace SimAntics { namespace Model { namespace Routing {
class VMObstacle { //like an XNA rectangle but a little different for our purposes
public:
    int32_t x1, x2, y1, y2;
    VMObstacle() : x1(0), x2(0), y1(0), y2(0) {}
    VMObstacle(int32_t x1_, int32_t y1_, int32_t x2_, int32_t y2_) {
        if (x1_ > x2_) { auto temp = x1_; x1_ = x2_; x2_ = temp; }
        if (y1_ > y2_) { auto temp = y1_; y1_ = y2_; y2_ = temp; }
        x1 = x1_; y1 = y1_; x2 = x2_; y2 = y2_;
    }
    virtual ~VMObstacle() {}
    bool Contains(const st::Point& pt) const { return (pt.X >= x1 && pt.X <= x2) && (pt.Y >= y1 && pt.Y <= y2); }
    bool HardContains(const st::Point& pt) const { return (pt.X > x1 && pt.X < x2) && (pt.Y > y1 && pt.Y < y2); }
    st::Point Closest(int32_t x, int32_t y) const { return st::Point(std::max(std::min(x2, x), x1), std::max(std::min(y2, y), y1)); }
    bool Intersects(const VMObstacle& other) const { return !((other.x1 >= x2 || other.x2 <= x1) || (other.y1 >= y2 || other.y2 <= y1)); }
};
}}}}
