/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Engine.Routing.VMRectRouter
#include "simantics/engine/Routing/VMRectRouter.h"
#include <cmath>
#include <climits>
#include <algorithm>
namespace FSO { namespace SimAntics { namespace Engine { namespace Routing {
using namespace FSO::SimAntics::Model::Routing;
using st::Point;

std::shared_ptr<std::list<Point>> VMRectRouter::Route(Point from, Point to) {
    std::vector<VMWalkableRect*> openSet;
    Owned.emplace_back(new VMWalkableRect(from.X, from.Y, from.X, from.Y));
    auto startRect = Owned.back().get();
    ConstructFirstFree(startRect);
    startRect->Start = true;
    startRect->ParentSource = from;
    startRect->OriginalG = 0;

    openSet.push_back(startRect);

    while (openSet.size() > 0) {
        auto current = openSet[0];
        openSet.erase(openSet.begin());

        if (current->Contains(to)) {
            auto result = std::make_shared<std::list<Point>>();
            result->push_front(to);
            if (!to.Equals(current->ParentSource)) result->push_front(current->ParentSource);
            Point last = current->ParentSource;
            while (current != startRect) {
                current = current->Parent;
                if (!last.Equals(current->ParentSource)) result->push_front(current->ParentSource);
                last = current->ParentSource;
            }
            return result;
        }
        current->State = 2; //this rectangle is now closed

        //generate all adj
        ExtendFrom(current, 0);
        ExtendFrom(current, 1);
        ExtendFrom(current, 2);
        ExtendFrom(current, 3);

        auto adj = current->Adj; // copy (C# foreach over a list not modified in the loop)
        for (VMWalkableRect* r : adj) {
            if (r->State == 2) continue; //closed
            bool newcomer = (r->State == 0);

            auto parentPt = RectIntersect(r, current, current->ParentSource);
            auto originalG = current->OriginalG + PointDist(current->ParentSource, parentPt);
            auto closest = r->Closest(to.X, to.Y);
            auto newGScore = originalG + PointDist(parentPt, closest);

            if (newcomer || newGScore < r->GScore) {
                r->State = 1;
                r->ParentSource = parentPt;
                r->Parent = current;
                r->OriginalG = originalG;
                r->GScore = newGScore;
                r->FScore = newGScore + PointDist(closest, to);

                if (newcomer) {
                    OpenSetSortedInsert(openSet, r);
                } else {
                    openSet.erase(std::remove(openSet.begin(), openSet.end(), r), openSet.end());
                    OpenSetSortedInsert(openSet, r);
                }
            }
        }
    }
    return nullptr; //failed
}

int32_t VMRectRouter::PointDist(Point pt1, Point pt2) {
    Point diff = pt1 - pt2;
    return (int32_t)std::sqrt((double)(diff.X * diff.X + diff.Y * diff.Y));
}

void VMRectRouter::OpenSetSortedInsert(std::vector<VMWalkableRect*>& set, VMWalkableRect* item) {
    for (size_t i = 0; i < set.size(); i++) {
        if (set[i]->FScore > item->FScore) {
            set.insert(set.begin() + i, item);
            return;
        }
    }
    set.push_back(item);
}

Point VMRectRouter::RectIntersect(VMObstacle* r1, VMObstacle* r2, Point destPoint) {
    bool vert = true;
    int d1, d2, p = 0;
    if (r1->x1 == r2->x2) { vert = false; p = r1->x1; }
    if (r1->x2 == r2->x1) { vert = false; p = r1->x2; }
    if (r1->y1 == r2->y2) { vert = true; p = r1->y1; }
    if (r1->y2 == r2->y1) { vert = true; p = r1->y2; }

    if (vert) {
        d1 = std::max(r1->x1, r2->x1); d2 = std::min(r1->x2, r2->x2);
        return Point(std::max<int32_t>(d1, std::min<int32_t>(d2, destPoint.X)), p);
    } else {
        d1 = std::max(r1->y1, r2->y1); d2 = std::min(r1->y2, r2->y2);
        return Point(p, std::max<int32_t>(d1, std::min<int32_t>(d2, destPoint.Y)));
    }
}

void VMRectRouter::ExtendFrom(VMWalkableRect* source, int32_t dir) {
    auto free = source->Free[dir].List; // copy: subtraction below may modify other rects' lists
    for (VMFreeListRegion line : free) {
        VMExtendRectResult extension;
        VMWalkableRect* newRect = nullptr;
        std::unique_ptr<VMWalkableRect> created;
        switch (dir) {
            case 0:
                extension = ExtendRect(dir, line.a, line.b, source->y1);
                if (extension.BestN != source->y1) created.reset(new VMWalkableRect(line.a, extension.BestN, line.b, source->y1));
                break;
            case 1:
                extension = ExtendRect(dir, line.a, line.b, source->x2);
                if (extension.BestN != source->x2) created.reset(new VMWalkableRect(source->x2, line.a, extension.BestN, line.b));
                break;
            case 2:
                extension = ExtendRect(dir, line.a, line.b, source->y2);
                if (extension.BestN != source->y2) created.reset(new VMWalkableRect(line.a, source->y2, line.b, extension.BestN));
                break;
            case 3:
                extension = ExtendRect(dir, line.a, line.b, source->x1);
                if (extension.BestN != source->x1) created.reset(new VMWalkableRect(extension.BestN, line.a, source->x1, line.b));
                break;
        }

        if (created == nullptr || extension.BestN == INT_MAX || extension.BestN == INT_MIN) continue;
        newRect = created.get();
        Owned.push_back(std::move(created));

        source->Adj.push_back(newRect);
        newRect->Adj.push_back(source);

        auto bounds = ((dir % 2) == 0) ? VMFreeListRegion(newRect->x1, newRect->x2) : VMFreeListRegion(newRect->y1, newRect->y2);
        VMFreeList free2(bounds);

        for (auto& r : extension.Best) {
            free2.Subtract(VMFreeListRegion(r.a, r.b));
            auto w = dynamic_cast<VMWalkableRect*>(r.rect);
            if (w != nullptr) {
                w->Free[(dir + 2) % 4].Subtract(bounds);
                w->Adj.push_back(newRect);
                newRect->Adj.push_back(w);
            }
        }

        newRect->Free[dir] = free2;
        newRect->Free[(dir + 2) % 4] = VMFreeList(0, 0);

        ConstructFree(newRect, ((dir % 2) == 1), ((dir % 2) == 0), ((dir % 2) == 1), ((dir % 2) == 0));

        if (!source->Start) Map.push_back(newRect);
    }
}

VMExtendRectResult VMRectRouter::ExtendRect(int32_t dir, int32_t d1, int32_t d2, int32_t p) {
    int bestN = ((dir + 1) % 4 < 2) ? INT_MIN : INT_MAX;
    std::vector<VMExtendRegion> best;

    for (VMObstacle* r : Map) {
        switch (dir) {
            case 0: //top
                if (r->y2 > p) break; //bottom of rect lower than start point = no hit
                if (r->x1 >= d2 || r->x2 <= d1) break; //does not intersect
                if (r->y2 > bestN) {
                    bestN = r->y2;
                    best.clear();
                    best.push_back(VMExtendRegion(r->x1, r->x2, r));
                } else if (r->y2 == bestN) {
                    best.push_back(VMExtendRegion(r->x1, r->x2, r));
                }
                break;
            case 1: //right
                if (r->x1 < p) break; //left of rect lefter than start point = no hit
                if (r->y1 >= d2 || r->y2 <= d1) break; //does not intersect
                if (r->x1 < bestN) {
                    bestN = r->x1;
                    best.clear();
                    best.push_back(VMExtendRegion(r->y1, r->y2, r));
                } else if (r->x1 == bestN) {
                    best.push_back(VMExtendRegion(r->y1, r->y2, r));
                }
                break;
            case 2: //bottom
                if (r->y1 < p) break; //top of rect higher than start point = no hit
                if (r->x1 >= d2 || r->x2 <= d1) break; //does not intersect
                if (r->y1 < bestN) {
                    bestN = r->y1;
                    best.clear();
                    best.push_back(VMExtendRegion(r->x1, r->x2, r));
                } else if (r->y1 == bestN) {
                    best.push_back(VMExtendRegion(r->x1, r->x2, r));
                }
                break;
            case 3: //left
                if (r->x2 > p) break; //right of rect righter than start point = no hit
                if (r->y1 >= d2 || r->y2 <= d1) break; //does not intersect
                if (r->x2 > bestN) {
                    bestN = r->x2;
                    best.clear();
                    best.push_back(VMExtendRegion(r->y1, r->y2, r));
                } else if (r->x2 == bestN) {
                    best.push_back(VMExtendRegion(r->y1, r->y2, r));
                }
                break;
        }
    }
    VMExtendRectResult result;
    result.Best = best;
    result.BestN = bestN;
    return result;
}

void VMRectRouter::ConstructFree(VMWalkableRect* rect, bool d1, bool d2, bool d3, bool d4) {
    if (d1) rect->Free[0] = VMFreeList(rect->x1, rect->x2);
    if (d2) rect->Free[1] = VMFreeList(rect->y1, rect->y2);
    if (d3) rect->Free[2] = VMFreeList(rect->x1, rect->x2);
    if (d4) rect->Free[3] = VMFreeList(rect->y1, rect->y2);

    for (VMObstacle* r : Map) {
        if (r == rect) continue;
        auto w = dynamic_cast<VMWalkableRect*>(r);
        if (d1 && r->y2 == rect->y1 && !(r->x2 <= rect->x1 || r->x1 >= rect->x2)) {
            rect->Free[0].Subtract(VMFreeListRegion(r->x1, r->x2));
            if (w != nullptr) {
                w->Free[2].Subtract(VMFreeListRegion(rect->x1, rect->x2));
                rect->Adj.push_back(w);
                w->Adj.push_back(rect);
            }
        }

        if (d2 && r->x1 == rect->x2 && !(r->y2 <= rect->y1 || r->y1 >= rect->y2)) {
            rect->Free[1].Subtract(VMFreeListRegion(r->y1, r->y2));
            if (w != nullptr) {
                w->Free[3].Subtract(VMFreeListRegion(rect->y1, rect->y2));
                rect->Adj.push_back(w);
                w->Adj.push_back(rect);
            }
        }

        if (d3 && r->y1 == rect->y2 && !(r->x2 <= rect->x1 || r->x1 >= rect->x2)) {
            rect->Free[2].Subtract(VMFreeListRegion(r->x1, r->x2));
            if (w != nullptr) {
                w->Free[0].Subtract(VMFreeListRegion(rect->x1, rect->x2));
                rect->Adj.push_back(w);
                w->Adj.push_back(rect);
            }
        }

        if (d4 && r->x2 == rect->x1 && !(r->y2 <= rect->y1 || r->y1 >= rect->y2)) {
            rect->Free[3].Subtract(VMFreeListRegion(r->y1, r->y2));
            if (w != nullptr) {
                w->Free[1].Subtract(VMFreeListRegion(rect->y1, rect->y2));
                rect->Adj.push_back(w);
                w->Adj.push_back(rect);
            }
        }
    }
}

void VMRectRouter::ConstructFirstFree(VMWalkableRect* rect) {
    rect->Free[0] = VMFreeList(rect->x1);
    rect->Free[1] = VMFreeList(rect->y1);
    rect->Free[2] = VMFreeList(rect->x1);
    rect->Free[3] = VMFreeList(rect->y1);

    for (VMObstacle* r : Map) {
        if (r == rect) continue;
        if (r->y2 == rect->y1 && !(r->x2 <= rect->x1 || r->x1 >= rect->x2))
            rect->Free[0].Subtract(VMFreeListRegion(r->x1, r->x2));

        if (r->x1 == rect->x2 && !(r->y2 <= rect->y1 || r->y1 >= rect->y2))
            rect->Free[1].Subtract(VMFreeListRegion(r->y1, r->y2));

        if (r->y1 == rect->y2 && !(r->x2 <= rect->x1 || r->x1 >= rect->x2))
            rect->Free[2].Subtract(VMFreeListRegion(r->x1, r->x2));

        if (r->x2 == rect->x1 && !(r->y2 <= rect->y1 || r->y1 >= rect->y2))
            rect->Free[3].Subtract(VMFreeListRegion(r->y1, r->y2));
    }
}
}}}}
