/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.LotView.Model.XmlHouseData and friends (World/model/XmlHouse.cs) — data only.
// STUB(3ds): the System.Xml (de)serializer (XmlHouseData.Parse/Save) is not ported; fill the structs yourself.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <cstdlib>
#include "world/model/Direction.h"
#include "world/model/WallTile.h"
namespace FSO { namespace LotView { namespace Model {
class XmlSoundData { public: uint32_t ID = 0; int32_t On = 0; };
class XmlHouseDataObject {
public:
    uint32_t GUIDInt() const { return (uint32_t)strtoul(GUID.c_str(), nullptr, 16); }
    std::string GUID;
    int32_t Level = 0, X = 0, Y = 0, Dir = 0, Group = 0;
    FSO::LotView::Model::Direction Direction() const {
        switch (Dir) {
            case 6: return Direction::WEST;
            case 4: return Direction::SOUTH;
            case 2: return Direction::EAST;
            case 0: return Direction::NORTH;
        }
        return Direction::WEST;
    }
};
class XmlHouseDataFloor { public: int32_t Level = 0; int16_t X = 0, Y = 0; int32_t Value = 0; };
class XmlHouseDataPool { public: int16_t X = 0, Y = 0; int32_t Value = 0; };
class XmlHouseDataWall {
public:
    int32_t Level = 0, X = 0, Y = 0;
    WallSegments Segments = (WallSegments)0;
    int32_t Placement = 0, LeftStyle = 0, RightStyle = 0;
    int32_t TopLeftPattern = 0, TopRightPattern = 0, BottomRightPattern = 0, BottomLeftPattern = 0;
};
class XmlHouseDataWorld {
public:
    std::vector<XmlHouseDataFloor> Floors;
    std::vector<XmlHouseDataWall> Walls;
    std::vector<XmlHouseDataPool> Pools;
};
class XmlHouseData {
public:
    int32_t Size = 0;
    int32_t Category = 0;
    XmlHouseDataWorld World;
    std::vector<XmlHouseDataObject> Objects;
    std::vector<XmlSoundData> Sounds;
};
}}}
