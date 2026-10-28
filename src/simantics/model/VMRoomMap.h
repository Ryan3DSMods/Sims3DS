/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Model.VMRoomMap — generates and manages a room map for a specified level.
#pragma once
#include <cstdint>
#include <vector>
#include <stack>
#include "core/math.h"
#include "world/model/WallTile.h"
#include "world/model/FloorTile.h"
#include "simantics/model/VMRoomInfo.h"
namespace FSO { namespace SimAntics { namespace Model {
class VMRoomMap {
public:
    std::vector<uint32_t> Map;
    int32_t Width = 0;
    int32_t Height = 0;

    /// Generates the room map for the specified walls array.
    void GenerateMap(const std::vector<FSO::LotView::Model::WallTile>& Walls, const std::vector<FSO::LotView::Model::FloorTile>& Floors, int32_t width, int32_t height, std::vector<VMRoom>& rooms); //for first floor gen, curRoom should be 1. For floors above, it should be the last genmap result
    std::vector<Routing::VMObstacle> GenerateRoomObs(uint16_t room, const st::Rectangle& bounds);
    void OptimizeObstacles(std::vector<Routing::VMObstacle>& obs);
    void PrintRoomMap();
private:
    uint16_t ExpectedTile = 0;
    static void SpreadOnto(const std::vector<FSO::LotView::Model::WallTile>& walls, const std::vector<FSO::LotView::Model::FloorTile>& floors, int32_t x, int32_t y, int32_t inDir, std::vector<uint32_t>& map, int32_t width, int32_t height, std::stack<st::Point>& spread, uint16_t room, uint16_t expectedTile, bool noAir);
};
}}}
