/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Engine.Routing.VMRectRouter — rectangle-expansion A* pathfinder used within a room.
#pragma once
#include <cstdint>
#include <list>
#include <vector>
#include <memory>
#include "core/math.h"
#include "simantics/model/Routing/VMObstacle.h"
#include "simantics/model/Routing/VMWalkableRect.h"
namespace FSO { namespace SimAntics { namespace Engine { namespace Routing {

struct VMExtendRegion {
    int32_t a;
    int32_t b;
    Model::Routing::VMObstacle* rect;
    VMExtendRegion(int32_t a_, int32_t b_, Model::Routing::VMObstacle* r) : a(a_), b(b_), rect(r) {}
};

struct VMExtendRectResult {
    std::vector<VMExtendRegion> Best;
    int32_t BestN = 0;
};

class VMRectRouter {
public:
    /// `map` entries are not owned (they usually point into a caller-owned obstacle vector).
    explicit VMRectRouter(const std::vector<Model::Routing::VMObstacle*>& map) : Map(map) {}
    /// Returns null when no route exists.
    std::shared_ptr<std::list<st::Point>> Route(st::Point from, st::Point to);
private:
    std::vector<Model::Routing::VMObstacle*> Map;
    std::vector<std::unique_ptr<Model::Routing::VMWalkableRect>> Owned; // walkable rects created during the search
    int32_t PointDist(st::Point pt1, st::Point pt2);
    void OpenSetSortedInsert(std::vector<Model::Routing::VMWalkableRect*>& set, Model::Routing::VMWalkableRect* item);
    st::Point RectIntersect(Model::Routing::VMObstacle* r1, Model::Routing::VMObstacle* r2, st::Point destPoint);
    void ExtendFrom(Model::Routing::VMWalkableRect* source, int32_t dir);
    VMExtendRectResult ExtendRect(int32_t dir, int32_t d1, int32_t d2, int32_t p);
    void ConstructFree(Model::Routing::VMWalkableRect* rect, bool d1, bool d2, bool d3, bool d4);
    void ConstructFirstFree(Model::Routing::VMWalkableRect* rect);
};
}}}}
