/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Model.VMRoomMap
#include "simantics/model/VMRoomMap.h"
#include <list>
#include <string>
#include <cstdio>
#include <algorithm>
namespace FSO { namespace SimAntics { namespace Model {
using namespace FSO::LotView::Model;
using Routing::VMObstacle;

void VMRoomMap::GenerateMap(const std::vector<WallTile>& Walls, const std::vector<FloorTile>& Floors, int32_t width, int32_t height, std::vector<VMRoom>& rooms) {
    Map.assign((size_t)(width * height), 0); //although 0 is the base of the array, room 1 is known to simantics as room 0.
    //values of 0 indicate the room has not been chosen in that location yet.
    bool noFloorBad = (rooms.size() > 1);
    this->Width = width;
    this->Height = height;

    //flood fill recursively. Each time choose find and choose the first "0" as the base.
    //The first recursion (outside) cannot fill into diagonals.
    bool remaining = true;
    bool outside = true;

    while (remaining) {
        std::stack<st::Point> spread;
        remaining = false;
        for (size_t i = 0; i < Map.size(); i++) {
            if (Map[i] == 0) {
                ExpectedTile = Floors[i].Pattern;
                //if (ExpectedTile == 0 && noFloorBad) continue;
                if (ExpectedTile < 65534) ExpectedTile = 0;
                remaining = true;
                Map[i] = (uint16_t)rooms.size();
                spread.push(st::Point((int32_t)(i % width), (int32_t)(i / width)));
                break;
            }
        }

        if (remaining) {
            int32_t rminX = spread.top().X;
            int32_t rmaxX = rminX;
            int32_t rminY = spread.top().Y;
            int32_t rmaxY = rminY;
            std::vector<VMObstacle> wallObs;
            uint16_t area = 0;
            while (spread.size() > 0) {
                area++;
                auto item = spread.top(); spread.pop();

                if (item.X > rmaxX) rmaxX = item.X;
                if (item.X < rminX) rminX = item.X;
                if (item.Y > rmaxY) rmaxY = item.Y;
                if (item.Y < rminY) rminY = item.Y;

                auto plusX = (item.X + 1) % width;
                auto minX = (item.X + width - 1) % width;
                auto plusY = (item.Y + 1) % height;
                auto minY = (item.Y + height - 1) % height;

                const WallTile& mainWalls = Walls[item.X + item.Y * width];
                int32_t obsX = item.X << 4;
                int32_t obsY = item.Y << 4;

                if ((mainWalls.Segments & WallSegments::HorizontalDiag) > 0) {
                    wallObs.push_back(VMObstacle(obsX + 9, obsY - 1, obsX + 17, obsY + 7));
                    wallObs.push_back(VMObstacle(obsX + 4, obsY + 4, obsX + 12, obsY + 12));
                    wallObs.push_back(VMObstacle(obsX - 1, obsY + 9, obsX + 7, obsY + 17));
                }

                if ((mainWalls.Segments & WallSegments::VerticalDiag) > 0) {
                    wallObs.push_back(VMObstacle(obsX - 1, obsY - 1, obsX + 7, obsY + 7));
                    wallObs.push_back(VMObstacle(obsX + 4, obsY + 4, obsX + 12, obsY + 12));
                    wallObs.push_back(VMObstacle(obsX + 9, obsY + 9, obsX + 17, obsY + 17));
                }

                if ((uint8_t)mainWalls.Segments > 15) continue; //don't spread on diagonals for now

                const WallTile& PXWalls = Walls[plusX + item.Y * width];
                const WallTile& PYWalls = Walls[item.X + plusY * width];

                if ((mainWalls.Segments & WallSegments::TopLeft) > 0 && !mainWalls.TopLeftDoor) wallObs.push_back(VMObstacle(obsX - 3, obsY - 3, obsX + 6, obsY + 19));
                if ((mainWalls.Segments & WallSegments::TopRight) > 0 && !mainWalls.TopRightDoor) wallObs.push_back(VMObstacle(obsX - 3, obsY - 3, obsX + 19, obsY + 6));
                if ((mainWalls.Segments & WallSegments::BottomLeft) > 0 && !PYWalls.TopRightDoor) wallObs.push_back(VMObstacle(obsX - 3, obsY + 13, obsX + 19, obsY + 19));
                if ((mainWalls.Segments & WallSegments::BottomRight) > 0 && !PXWalls.TopLeftDoor) wallObs.push_back(VMObstacle(obsX + 13, obsY - 3, obsX + 19, obsY + 19));
                //
                if (((PXWalls.Segments & WallSegments::TopLeft) == 0 || PXWalls.TopLeftStyle != 1))
                    SpreadOnto(Walls, Floors, plusX, item.Y, 0, Map, width, height, spread, (uint16_t)rooms.size(), ExpectedTile, noFloorBad);

                if (((mainWalls.Segments & WallSegments::TopLeft) == 0 || mainWalls.TopLeftStyle != 1))
                    SpreadOnto(Walls, Floors, minX, item.Y, 2, Map, width, height, spread, (uint16_t)rooms.size(), ExpectedTile, noFloorBad);

                if (((PYWalls.Segments & WallSegments::TopRight) == 0 || PYWalls.TopRightStyle != 1))
                    SpreadOnto(Walls, Floors, item.X, plusY, 1, Map, width, height, spread, (uint16_t)rooms.size(), ExpectedTile, noFloorBad);

                if (((mainWalls.Segments & WallSegments::TopRight) == 0 || mainWalls.TopRightStyle != 1))
                    SpreadOnto(Walls, Floors, item.X, minY, 3, Map, width, height, spread, (uint16_t)rooms.size(), ExpectedTile, noFloorBad);
            }

            auto bounds = st::Rectangle(rminX, rminY, (rmaxX - rminX) + 1, (rmaxY - rminY) + 1);
            auto roomObs = GenerateRoomObs((uint16_t)rooms.size(), bounds);
            OptimizeObstacles(wallObs);
            OptimizeObstacles(roomObs);
            VMRoom room;
            room.IsOutside = outside || ExpectedTile > 65533;
            room.IsPool = ExpectedTile > 65533;
            room.Bounds = bounds;
            room.WallObs = wallObs;
            room.RoomObs = roomObs;
            room.Area = area;
            rooms.push_back(room);
            outside = false;
        }
    }
}

void VMRoomMap::SpreadOnto(const std::vector<WallTile>& walls, const std::vector<FloorTile>& floors, int32_t x, int32_t y, int32_t inDir, std::vector<uint32_t>& map, int32_t width, int32_t /*height*/, std::stack<st::Point>& spread, uint16_t room, uint16_t expectedTile, bool /*noAir*/) {
    const WallTile& wall = walls[x + y * width];
    auto floor = floors[x + y * width].Pattern;
    if ((expectedTile > 0 && expectedTile != floor) || (expectedTile == 0 && floor > 65533)) {
        return;
    }
    if ((wall.Segments & WallSegments::HorizontalDiag) > 0) {
        if (inDir < 2) {
            //bottom (bottom right pattern)
            if ((map[x + y * width] & (uint32_t)0x0000FFFF) > 0) return; //don't spread onto
            map[x + y * width] |= room;
        } else {
            //top (bottom left pattern)
            if ((map[x + y * width] & (uint32_t)0xFFFF0000) > 0) return; //don't spread onto
            map[x + y * width] |= (uint32_t)room << 16;
        }
        //if (!floorMode) walls[x + y * width] = wall;
    } else if ((wall.Segments & WallSegments::VerticalDiag) > 0) {
        if (inDir > 0 && inDir < 3) {
            //left
            if ((map[x + y * width] & (uint32_t)0x0000FFFF) > 0) return; //don't spread onto
            map[x + y * width] |= room;
        } else {
            //right
            if ((map[x + y * width] & (uint32_t)0xFFFF0000) > 0) return; //don't spread onto
            map[x + y * width] |= (uint32_t)room << 16;
        }
        //if (!floorMode) walls[x + y * width] = wall;
    } else {
        if (map[x + y * width] > 0) return;
        map[x + y * width] = (uint32_t)(room | (room << 16));
    }
    spread.push(st::Point(x, y));
}

std::vector<VMObstacle> VMRoomMap::GenerateRoomObs(uint16_t room, const st::Rectangle& bounds) {
    std::vector<VMObstacle> result;
    auto x1 = std::max<int32_t>(0, bounds.X - 1);
    auto x2 = std::min(Width, bounds.Right() + 1);
    auto y1 = std::max<int32_t>(0, bounds.Y - 1);
    auto y2 = std::min(Height, bounds.Bottom() + 1);
    for (int32_t y = y1; y < y2; y++) {
        int next = -1; //index into result (C# uses a reference to the last added obstacle)
        for (int32_t x = x1; x < x2; x++) {
            int32_t tRoom = (uint16_t)Map[x + y * Width];
            if (tRoom != room) {
                if (next != -1) result[next].x2 += 16;
                else {
                    result.push_back(VMObstacle((x << 4) - 3, (y << 4) - 3, (x << 4) + 19, (y << 4) + 19));
                    next = (int)result.size() - 1;
                }
            } else {
                if (next != -1) next = -1;
            }
        }
    }
    return result;
}

void VMRoomMap::OptimizeObstacles(std::vector<VMObstacle>& obs) {
    // collapses axis aligned collision bounds into single rectangles (useful for walls and rooms)
    // i have totally verified that this is reliable and optimal in every percievable way
    // O(n^2) probably
    // if you find a better way please replace, this is just sort of helpful.
    std::list<VMObstacle*> changed;
    for (auto& o : obs) changed.push_back(&o);
    std::vector<bool> removed(obs.size(), false);
    auto item = changed.begin();
    while (item != changed.end()) {
        VMObstacle* r1 = *item;
        //ok, so for each rectangle we need to find rectangles we can join with.
        auto targ = changed.begin();
        while (targ != changed.end()) {
            VMObstacle* r2 = *targ;
            auto nextT = std::next(targ);
            if (r1 != r2) {
                if (r1->x1 == r2->x1 && r1->x2 == r2->x2 && !(r1->y1 > r2->y2 || r1->y2 < r2->y1)) {
                    //intersects... combine em
                    r1->y1 = std::min(r1->y1, r2->y1);
                    r1->y2 = std::max(r1->y2, r2->y2);
                    if (nextT == item) { /* cannot happen: r1 != r2 */ }
                    removed[(size_t)(r2 - obs.data())] = true;
                    changed.erase(targ);
                } else if (r1->y1 == r2->y1 && r1->y2 == r2->y2 && !(r1->x1 > r2->x2 || r1->x2 < r2->x1)) {
                    r1->x1 = std::min(r1->x1, r2->x1);
                    r1->x2 = std::max(r1->x2, r2->x2);
                    removed[(size_t)(r2 - obs.data())] = true;
                    changed.erase(targ);
                }
            }
            targ = nextT;
        }
        item = std::next(item);
    }
    std::vector<VMObstacle> result;
    for (size_t i = 0; i < obs.size(); i++) if (!removed[i]) result.push_back(obs[i]);
    obs = result;
}

void VMRoomMap::PrintRoomMap() {
    int off = 0;
    for (int32_t y = 0; y < Height; y++) {
        std::string sb;
        for (int32_t x = 0; x < Width; x++) {
            sb += std::to_string(Map[off++]);
            if (Map[off - 1] < 10) sb += " ";
            sb += " ";
        }
        fprintf(stderr, "%s\n", sb.c_str());
    }
}
}}}
