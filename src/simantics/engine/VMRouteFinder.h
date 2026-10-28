/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// TSO.Simantics.engine.VMRouteFinder (engine/VMRouteFinder.cs) — legacy route finder, superseded by
// VMSlotParser and not referenced by the FreeSims VM. Translated for completeness in its original namespace.
#pragma once
#include <vector>
#include <algorithm>
#include <cmath>
#include "core/math.h"
#include "simantics/_files_contract.h"
namespace TSO { namespace Simantics { namespace engine {
using FSO::Files::Formats::IFF::Chunks::SLOTFlags;

class VMFindLocationResult {
public:
    SLOTFlags Direction = (SLOTFlags)0;
    st::Vector2 Position;
    int32_t Proximity = 0;
};

class VMProximitySorter {
public:
    explicit VMProximitySorter(int32_t desiredProximity) : DesiredProximity(desiredProximity) {}
    int Compare(const VMFindLocationResult& x, const VMFindLocationResult& y) const {
        auto distanceX = std::abs(x.Proximity - DesiredProximity);
        auto distanceY = std::abs(y.Proximity - DesiredProximity);
        if (distanceX < distanceY) {
            return -1;
        } else if (distanceX > distanceY) {
            return -1;
        } else {
            return ((int)x.Direction < (int)y.Direction) ? -1 : (((int)x.Direction > (int)y.Direction) ? 1 : 0);
        }
    }
private:
    int32_t DesiredProximity;
};

class VMRouteFinder {
public:
    /// This method will find all the avaliable locations within the criteria ordered by proximity to the optimal proximity
    static std::vector<VMFindLocationResult> FindAvaliableLocations(st::Vector2 center, SLOTFlags flags, int32_t minProximity, int32_t maxProximity, int32_t desiredProximity) {
        /**
         * Start at min proximity and circle around the object to find the avaliable locations.
         * Then pick the one nearest to the optimal value
         */
        std::vector<VMFindLocationResult> result;
        auto proximity = minProximity;
        auto proximityIncrement = 16;
        auto proximityNudge = proximityIncrement * 0.25f;
        auto currentDepth = 1.0f;

        while (proximity <= maxProximity) {
            auto angle = 0.0f;
            /** Every time we move out by 1 tile in proximity, there will be more tiles to look at **/
            auto angleIncrement = 360.0f / (currentDepth * 8);
            while (angle < 360.0f) {
                auto radians = angle * (M_PI / 180.0f);
                auto radius = proximity + proximityNudge;
                auto xpos = std::nearbyint(radius * std::cos(radians));
                auto ypos = std::nearbyint(radius * std::sin(radians));

                auto tileX = (float)std::nearbyint(xpos / 16.0f) + center.X;
                auto tileY = (float)std::nearbyint(ypos / 16.0f) + center.Y;
                auto direction = GetDirection(center, st::Vector2(tileX, tileY));
                if ((flags & direction) == (int64_t)direction) {
                    //TODO: Check if slot is occupied or out of bounds
                    /** This is acceptible to the slot :) **/
                    VMFindLocationResult r;
                    r.Direction = direction;
                    r.Position = st::Vector2(tileX, tileY);
                    r.Proximity = proximity;
                    result.push_back(r);
                }
                angle += angleIncrement;
            }
            proximity += proximityIncrement;
        }

        /** Sort by how close they are to desired proximity **/
        VMProximitySorter sorter(desiredProximity);
        std::stable_sort(result.begin(), result.end(), [&](const VMFindLocationResult& a, const VMFindLocationResult& b) { return sorter.Compare(a, b) < 0 && sorter.Compare(b, a) >= 0; });
        return result;
    }

    /// Returns which direction (n/s/e/w/ne/nw/se/sw) a target is in relation to an object.
    static SLOTFlags GetDirection(st::Vector2 center, st::Vector2 target) {
        target.X -= center.X;
        target.Y -= center.Y;
        if (target.Y < 0) {
            if (target.X > 0) {
                return SLOTFlags::NORTH_EAST;
            } else if (target.X < 0) {
                return SLOTFlags::NORTH_WEST;
            }
            return SLOTFlags::NORTH;
        } else if (target.Y > 0) {
            if (target.X > 0) {
                return SLOTFlags::SOUTH_EAST;
            } else if (target.Y < 0) {
                return SLOTFlags::SOUTH_WEST;
            }
            return SLOTFlags::SOUTH;
        }
        if (target.X < 0) { return SLOTFlags::WEST; }
        return SLOTFlags::EAST;
    }
};
}}}
