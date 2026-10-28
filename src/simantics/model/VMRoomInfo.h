/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Model.VMRoomInfo / VMRoom / VMRoomPortal
#pragma once
#include <cstdint>
#include <vector>
#include <functional>
#include "core/math.h"
#include "world/model/RoomLighting.h"
#include "simantics/model/Routing/VMObstacle.h"
namespace FSO { namespace SimAntics {
class VMEntity;
namespace Model {

struct VMRoom {
    uint16_t RoomID = 0;
    uint16_t LightBaseRoom = 0;
    bool IsOutside = false;
    uint16_t Area = 0;
    bool IsPool = false;
    bool Unroutable = false;
    std::vector<Routing::VMObstacle> WallObs;
    std::vector<Routing::VMObstacle> RoomObs;
    st::Rectangle Bounds;
};

class VMRoomPortal {
public:
    int16_t ObjectID = 0;
    uint16_t TargetRoom = 0;
    VMRoomPortal() {}
    VMRoomPortal(int16_t obj, uint16_t target) : ObjectID(obj), TargetRoom(target) {}
    bool operator==(const VMRoomPortal& c2) const { return ObjectID == c2.ObjectID && TargetRoom == c2.TargetRoom; }
    bool operator!=(const VMRoomPortal& c2) const { return !(*this == c2); }
    int32_t GetHashCode() const { return ObjectID + TargetRoom; }
};

struct VMRoomInfo {
    std::vector<VMRoomPortal> Portals;
    std::vector<VMEntity*> Entities;
    FSO::LotView::Model::RoomLighting Light;
    VMRoom Room;
};

}}}
namespace std {
template<> struct hash<FSO::SimAntics::Model::VMRoomPortal> {
    size_t operator()(const FSO::SimAntics::Model::VMRoomPortal& p) const { return (size_t)p.GetHashCode(); }
};
}
