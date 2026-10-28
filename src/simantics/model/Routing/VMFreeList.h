/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Model.Routing.VMFreeList / VMFreeListRegion
#pragma once
#include <cstdint>
#include <list>
namespace FSO { namespace SimAntics { namespace Model { namespace Routing {

class VMFreeListRegion {
public:
    int32_t a;
    int32_t b;
    VMFreeListRegion(int32_t a_, int32_t b_) : a(a_), b(b_) {}
};

//used for keeping track of free edges on walkable rectangles (to extend further rectangles out of)
class VMFreeList {
public:
    std::list<VMFreeListRegion> List;
    VMFreeList() {}
    explicit VMFreeList(int32_t point) { //forces a free section with 0 interior.
        List.push_front(VMFreeListRegion(point, point));
    }
    VMFreeList(int32_t start, int32_t end) {
        if (start != end) List.push_front(VMFreeListRegion(start, end));
    }
    explicit VMFreeList(const VMFreeListRegion& region) {
        if (region.a != region.b) List.push_front(region);
    }
    void Subtract(const VMFreeListRegion& region) {
        auto elem = List.begin();
        while (elem != List.end()) {
            VMFreeListRegion& part = *elem;
            auto next = std::next(elem);
            if (!(region.a >= part.b || region.b <= part.a)) {
                if (part.a >= region.a && region.b < part.b) {
                    //erases starting from left
                    part.a = region.b;
                    if (part.a == part.b) List.erase(elem);
                } else if (region.b >= part.b && region.a > part.a) {
                    //erases starting from right
                    part.b = region.a;
                    if (part.a == part.b) List.erase(elem);
                } else if (region.a <= part.a && region.b >= part.b) {
                    //region entirely erases part
                    List.erase(elem);
                } else if (region.a > part.a && region.b < part.b) {
                    //region internally erases part (result is two)
                    auto temp = part.b;
                    part.b = region.a;
                    List.insert(next, VMFreeListRegion(region.b, temp));
                }
            }
            elem = next;
        }
    }
};
}}}}
