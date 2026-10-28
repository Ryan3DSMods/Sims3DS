/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Utils.VMArchitectureTools
#include "simantics/utils/VMArchitectureTools.h"
#include <map>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include "simantics/VMArchitecture.h"
#include "simantics/VMArchitecture.h"
#include "simantics/VMContext.h"
#include "content/ContentManager.h"
#include <algorithm>
namespace FSO { namespace SimAntics { namespace Utils {
using namespace FSO::LotView::Model;
using st::Point;

/*
Look at this way up <----
Starting at % line, going cw. Middle is (0,0), and below it is the tile (0,0)..

       /\
      /  \ +x
     /\  %\
    /  \%  \
    \  /\  /
     \/  \/
      \  / +y
       \/
*/
const Point VMArchitectureTools::WLStartOff[8] = {
    Point(0, 0), Point(0, 0), Point(0, 0), Point(-1, 0), Point(-1, 0), Point(-1, -1), Point(0, -1), Point(0, -1),
};
const WallSegments VMArchitectureTools::WLMainSeg[8] = {
    WallSegments::TopRight, WallSegments::VerticalDiag, WallSegments::TopLeft, WallSegments::HorizontalDiag,
    WallSegments::TopRight, WallSegments::VerticalDiag, WallSegments::TopLeft, WallSegments::HorizontalDiag
};
const Point VMArchitectureTools::WLSubOff[4] = { Point(0, -1), Point(-1, 0), Point(0, -1), Point(-1, 0) };
const WallSegments VMArchitectureTools::WLSubSeg[4] = { WallSegments::BottomLeft, WallSegments::BottomRight, WallSegments::BottomLeft, WallSegments::BottomRight };
const Point VMArchitectureTools::WLStep[8] = {
    Point(1, 0), Point(1, 1), Point(0, 1), Point(-1, 1), Point(-1, 0), Point(-1, -1), Point(0, -1), Point(1, -1),
};
const WallSegments VMArchitectureTools::AnyDiag = WallSegments::HorizontalDiag | WallSegments::VerticalDiag;
const int32_t VMArchitectureTools::CutCheckDir[4][2] = { {-1, -1}, {-1, 1}, {1, 1}, {1, -1} };

int32_t VMArchitectureTools::DrawWall(VMArchitecture* target, Point pos, int32_t length, int32_t direction, uint16_t pattern, uint16_t style, int8_t level, bool force) {
    if (!force && !VerifyDrawWall(target, pos, length, direction, level)) return 0;

    int totalWalls = 0;
    pos += WLStartOff[direction];
    bool diagCheck = (direction % 2 == 1);
    for (int i = 0; i < length; i++) {
        auto wall = target->GetWall((int16_t)pos.X, (int16_t)pos.Y, level);
        if ((wall.Segments & (WLMainSeg[direction] | AnyDiag)) == 0 && (!diagCheck || (wall.Segments == 0))) {
            //no wall here already, apply it.
            wall.Segments |= WLMainSeg[direction];
            if (diagCheck) {
                wall.TopRightStyle = style;
                wall.BottomLeftPattern = pattern;
                wall.BottomRightPattern = pattern;
            } else if (WLMainSeg[direction] == WallSegments::TopRight) {
                wall.TopRightStyle = style;
                wall.TopRightPattern = pattern;
            } else {
                wall.TopLeftStyle = style;
                wall.TopLeftPattern = pattern;
            }

            totalWalls++;
            target->SetWall((int16_t)pos.X, (int16_t)pos.Y, level, wall);

            if (!diagCheck) {
                auto tPos = pos + WLSubOff[direction / 2];
                wall = target->GetWall((int16_t)tPos.X, (int16_t)tPos.Y, level);
                wall.Segments |= WLSubSeg[direction / 2];

                if (WLSubSeg[direction / 2] == WallSegments::BottomRight) wall.BottomRightPattern = pattern;
                else wall.BottomLeftPattern = pattern;

                target->SetWall((int16_t)tPos.X, (int16_t)tPos.Y, level, wall);
            }
        }
        pos += WLStep[direction];
    }
    return totalWalls;
}

int32_t VMArchitectureTools::EraseWall(VMArchitecture* target, Point pos, int32_t length, int32_t direction, uint16_t /*pattern*/, uint16_t /*style*/, int8_t level) {
    if (!VerifyEraseWall(target, pos, length, direction, level)) return 0;

    int totalWalls = 0;
    pos += WLStartOff[direction];
    bool diagCheck = (direction % 2 == 1);
    for (int i = 0; i < length; i++) {
        auto wall = target->GetWall((int16_t)pos.X, (int16_t)pos.Y, level);
        if ((wall.Segments & WLMainSeg[direction]) > 0) totalWalls++;
        wall.Segments &= ~WLMainSeg[direction];
        target->SetWall((int16_t)pos.X, (int16_t)pos.Y, level, wall);

        if (!diagCheck) {
            auto tPos = pos + WLSubOff[direction / 2];
            wall = target->GetWall((int16_t)tPos.X, (int16_t)tPos.Y, level);
            wall.Segments &= ~WLSubSeg[direction / 2];

            target->SetWall((int16_t)tPos.X, (int16_t)tPos.Y, level, wall);
        }
        pos += WLStep[direction];
    }
    return totalWalls;
}

bool VMArchitectureTools::VerifyEraseWall(VMArchitecture* target, Point pos, int32_t length, int32_t direction, int8_t level) {
    pos += WLStartOff[direction];
    bool diagCheck = (direction % 2 == 1);
    for (int i = 0; i < length; i++) {
        if (pos.X <= 0 || pos.X >= target->Width || pos.Y <= 0 || pos.Y >= target->Height) return false;
        if (target->OutsideClip((int16_t)pos.X, (int16_t)pos.Y, level)) return false; // TS1(sims): FreeSO OutsideClip
        auto wall = target->GetWall((int16_t)pos.X, (int16_t)pos.Y, level);
        wall.Segments &= ~WLMainSeg[direction];
        if (!target->Context->CheckWallValid(LotTilePos::FromBigTile((int16_t)pos.X, (int16_t)pos.Y, level), wall)) return false;

        if (!diagCheck) {
            auto tPos = pos + WLSubOff[direction / 2];
            if (target->OutsideClip((int16_t)tPos.X, (int16_t)tPos.Y, level)) return false; // TS1(sims): FreeSO
            wall = target->GetWall((int16_t)tPos.X, (int16_t)tPos.Y, level);
            wall.Segments &= ~WLSubSeg[direction / 2];

            if (!target->Context->CheckWallValid(LotTilePos::FromBigTile((int16_t)tPos.X, (int16_t)tPos.Y, level), wall)) return false;
        }
        pos += WLStep[direction];
    }
    return true;
}

bool VMArchitectureTools::VerifyDrawWall(VMArchitecture* target, Point pos, int32_t length, int32_t direction, int8_t level) {
    pos += WLStartOff[direction];
    bool diagCheck = (direction % 2 == 1);
    for (int i = 0; i < length; i++) {
        if (pos.X <= 0 || pos.X >= target->Width || pos.Y <= 0 || pos.Y >= target->Height) return false;
        if (target->OutsideClip((int16_t)pos.X, (int16_t)pos.Y, level)) return false; // TS1(sims): FreeSO OutsideClip
        auto wall = target->GetWall((int16_t)pos.X, (int16_t)pos.Y, level);
        if ((wall.Segments & AnyDiag) == 0 && (!diagCheck || (wall.Segments == 0))) {
            wall.Segments &= ~WLMainSeg[direction];
            if (!target->Context->CheckWallValid(LotTilePos::FromBigTile((int16_t)pos.X, (int16_t)pos.Y, level), wall)) return false;
            if (!diagCheck) {
                auto tPos = pos + WLSubOff[direction / 2];
            if (target->OutsideClip((int16_t)tPos.X, (int16_t)tPos.Y, level)) return false; // TS1(sims): FreeSO
                wall = target->GetWall((int16_t)tPos.X, (int16_t)tPos.Y, level);
                if (!(level == 1 || target->Supported[level - 2][pos.Y * target->Width + pos.X] || target->Supported[level - 2][tPos.Y * target->Width + tPos.X])) return false;
                if ((wall.Segments & AnyDiag) == 0) {
                    wall.Segments |= WLSubSeg[direction / 2];
                    if (!target->Context->CheckWallValid(LotTilePos::FromBigTile((int16_t)tPos.X, (int16_t)tPos.Y, level), wall)) return false;
                }
                else return false;
            } else {
                if (!(level == 1 || target->Supported[level - 2][pos.Y * target->Width + pos.X])) return false;
            }
        }
        else return false;
        pos += WLStep[direction];
    }
    return true;
}

int32_t VMArchitectureTools::DrawWallRect(VMArchitecture* target, const st::Rectangle& rect, uint16_t pattern, uint16_t style, int8_t level) {
    if (!(
        VerifyDrawWall(target, Point(rect.X, rect.Y), rect.Width, 0, level) &&
        VerifyDrawWall(target, Point(rect.X, rect.Y), rect.Height, 2, level) &&
        VerifyDrawWall(target, Point(rect.X, rect.Y + rect.Height), rect.Width, 0, level) &&
        VerifyDrawWall(target, Point(rect.X + rect.Width, rect.Y), rect.Height, 2, level)
        )) return 0;
    int totalWalls = 0;
    totalWalls += DrawWall(target, Point(rect.X, rect.Y), rect.Width, 0, pattern, style, level, true);
    totalWalls += DrawWall(target, Point(rect.X, rect.Y), rect.Height, 2, pattern, style, level, true);
    totalWalls += DrawWall(target, Point(rect.X, rect.Y + rect.Height), rect.Width, 0, pattern, style, level, true);
    totalWalls += DrawWall(target, Point(rect.X + rect.Width, rect.Y), rect.Height, 2, pattern, style, level, true);
    return totalWalls;
}

int32_t VMArchitectureTools::GetPatternDirection(VMArchitecture* target, Point pos, uint16_t /*pattern*/, int32_t direction, int32_t altDir, int8_t level) {
    if (pos.X < 0 || pos.X >= target->Width || pos.Y < 0 || pos.Y >= target->Height) return -1;

    auto wall = target->GetWall((int16_t)pos.X, (int16_t)pos.Y, level);
    if ((wall.Segments & WallSegments::HorizontalDiag) > 0) {
        return direction;
    } else if ((wall.Segments & WallSegments::VerticalDiag) > 0) {
        return direction;
    }

    if ((wall.Segments & (WallSegments)(1 << direction)) > 0) { }
    else if ((wall.Segments & (WallSegments)(1 << altDir)) > 0) direction = altDir;
    else {
        return -1;
    }

    return direction;
}

PatternReplaceCount VMArchitectureTools::WallPatternDot(VMArchitecture* target, Point pos, uint16_t pattern, int32_t direction, int32_t altDir, int8_t level) {
    PatternReplaceCount fail; fail.Total = -1;
    if (pos.X < 0 || pos.X >= target->Width || pos.Y < 0 || pos.Y > target->Height) return fail;

    //pattern replace count used a little differently here. cost still stores replaced cost, but total stores replaced direction.
    PatternReplaceCount replaceCost(false);
    uint16_t replaced = 0;
    auto wall = target->GetWall((int16_t)pos.X, (int16_t)pos.Y, level);
    //direction starts lefttop, righttop
    if ((wall.Segments & WallSegments::HorizontalDiag) > 0 && wall.TopRightStyle == 1) {
        if (direction < 2) {
            //bottom (bottom right pattern)
            replaced = wall.BottomRightPattern;
            wall.BottomRightPattern = pattern;
        } else {
            //top (bottom left pattern)
            replaced = wall.BottomLeftPattern;
            wall.BottomLeftPattern = pattern;
        }
        target->SetWall((int16_t)pos.X, (int16_t)pos.Y, level, wall);
        if (replaced == pattern) return fail;
        replaceCost.Add(replaced);
        replaceCost.Total = direction;
        return replaceCost;
    } else if ((wall.Segments & WallSegments::VerticalDiag) > 0 && wall.TopRightStyle == 1) {
        if (direction > 0 && direction < 3) {
            //left
            replaced = wall.BottomLeftPattern;
            wall.BottomLeftPattern = pattern;
        } else {
            //right
            replaced = wall.BottomRightPattern;
            wall.BottomRightPattern = pattern;
        }
        target->SetWall((int16_t)pos.X, (int16_t)pos.Y, level, wall);
        if (replaced == pattern) return fail;
        replaceCost.Add(replaced);
        replaceCost.Total = direction;
        return replaceCost;
    }

    if ((wall.Segments & (WallSegments)(1 << direction)) > 0) { }
    else if ((wall.Segments & (WallSegments)(1 << altDir)) > 0) direction = altDir;
    else {
        return fail;
    }

    if (direction == 0 && wall.TopLeftThick()) { replaced = wall.TopLeftPattern; wall.TopLeftPattern = pattern; }
    else if (direction == 1 && wall.TopRightThick()) { replaced = wall.TopRightPattern; wall.TopRightPattern = pattern; }
    else if (direction == 2 && pos.X < target->Width && target->GetWall((int16_t)(pos.X + 1), (int16_t)pos.Y, level).TopLeftThick()) { replaced = wall.BottomRightPattern; wall.BottomRightPattern = pattern; }
    else if (direction == 3 && pos.Y < target->Height && target->GetWall((int16_t)pos.X, (int16_t)(pos.Y + 1), level).TopRightThick()) { replaced = wall.BottomLeftPattern; wall.BottomLeftPattern = pattern; }
    target->SetWall((int16_t)pos.X, (int16_t)pos.Y, level, wall);

    if (replaced == pattern) return fail;
    replaceCost.Add(replaced);
    replaceCost.Total = direction;
    return replaceCost;
}

PatternReplaceCount VMArchitectureTools::WallPatternFill(VMArchitecture* target, Point pos, uint16_t pattern, int8_t level) {
    if (pos.X < 0 || pos.X >= target->Width || pos.Y < 0 || pos.Y >= target->Height) return PatternReplaceCount();

    pos.X = std::max<int32_t>(std::min<int32_t>(pos.X, target->Width - 1), 0);
    pos.Y = std::max<int32_t>(std::min<int32_t>(pos.Y, target->Height - 1), 0);
    auto& walls = target->Walls[level - 1];

    auto width = target->Width;
    auto height = target->Height;
    PatternReplaceCount wallsCovered(false);

    std::vector<uint8_t> Map((size_t)(target->Width * target->Height), 0);

    //flood fill recursively. Each time choose find and choose the first "0" as the base.
    //The first recursion (outside) cannot fill into diagonals.
    std::stack<Point> spread;
    spread.push(pos);
    while (spread.size() > 0) {
        auto item = spread.top(); spread.pop();

        auto plusX = (item.X + 1) % width;
        auto minX = (item.X + width - 1) % width;
        auto plusY = (item.Y + 1) % height;
        auto minY = (item.Y + height - 1) % height;

        auto mainWalls = walls[item.X + item.Y * width];
        if ((uint8_t)mainWalls.Segments > 15) continue; //don't spread on diagonals for now

        auto PXWalls = walls[plusX + item.Y * width];
        auto PYWalls = walls[item.X + plusY * width];

        if (Map[plusX + item.Y * width] < 3 && ((PXWalls.Segments & WallSegments::TopLeft) == 0 || PXWalls.TopLeftStyle != 1))
            wallsCovered += SpreadOnto(walls, plusX, item.Y, 0, Map, width, height, spread, pattern, false);
        else {
            if (mainWalls.BottomRightPattern != pattern && PXWalls.TopLeftThick()) {
                wallsCovered.Add(mainWalls.BottomRightPattern);
                mainWalls.BottomRightPattern = pattern;
            }
        }

        if (Map[minX + item.Y * width] < 3 && ((mainWalls.Segments & WallSegments::TopLeft) == 0 || mainWalls.TopLeftStyle != 1))
            wallsCovered += SpreadOnto(walls, minX, item.Y, 2, Map, width, height, spread, pattern, false);
        else {
            if (mainWalls.TopLeftPattern != pattern && mainWalls.TopLeftThick()) {
                wallsCovered.Add(mainWalls.TopLeftPattern);
                mainWalls.TopLeftPattern = pattern;
            }
        }

        if (Map[item.X + plusY * width] < 3 && ((PYWalls.Segments & WallSegments::TopRight) == 0 || PYWalls.TopRightStyle != 1))
            wallsCovered += SpreadOnto(walls, item.X, plusY, 1, Map, width, height, spread, pattern, false);
        else {
            if (mainWalls.BottomLeftPattern != pattern && PYWalls.TopRightThick()) {
                wallsCovered.Add(mainWalls.BottomLeftPattern);
                mainWalls.BottomLeftPattern = pattern;
            }
        }

        if (Map[item.X + minY * width] < 3 && ((mainWalls.Segments & WallSegments::TopRight) == 0 || mainWalls.TopRightStyle != 1))
            wallsCovered += SpreadOnto(walls, item.X, minY, 3, Map, width, height, spread, pattern, false);
        else {
            if (mainWalls.TopRightPattern != pattern && mainWalls.TopRightThick()) {
                wallsCovered.Add(mainWalls.TopRightPattern);
                mainWalls.TopRightPattern = pattern;
            }
        }

        walls[item.X + item.Y * width] = mainWalls;
    }
    return wallsCovered;
}

PatternReplaceCount VMArchitectureTools::SpreadOnto(std::vector<WallTile>& walls, int32_t x, int32_t y, int32_t inDir, std::vector<uint8_t>& map, int32_t width, int32_t /*height*/, std::stack<Point>& spread, uint16_t pattern, bool floorMode) {
    PatternReplaceCount filled(false);
    auto wall = walls[x + y * width];
    if ((wall.Segments & WallSegments::HorizontalDiag) > 0 && (wall.TopRightStyle == 1 || floorMode)) {
        if (inDir < 2) {
            //bottom (bottom right pattern)
            if (!floorMode && wall.BottomLeftPattern != pattern) {
                filled.Add(wall.BottomLeftPattern);
                wall.BottomLeftPattern = pattern;
            }
            map[x + y * width] |= 1;
        } else {
            //top (bottom left pattern)
            if (!floorMode && wall.BottomRightPattern != pattern) {
                filled.Add(wall.BottomRightPattern);
                wall.BottomRightPattern = pattern;
            }
            map[x + y * width] |= 2;
        }
        if (!floorMode) walls[x + y * width] = wall;
    } else if ((wall.Segments & WallSegments::VerticalDiag) > 0 && (wall.TopRightStyle == 1 || floorMode)) {
        if (inDir > 0 && inDir < 3) {
            //left
            if (!floorMode && wall.BottomRightPattern != pattern) {
                filled.Add(wall.BottomRightPattern);
                wall.BottomRightPattern = pattern;
            }
            map[x + y * width] |= 1;
        } else {
            //right
            if (!floorMode && wall.BottomLeftPattern != pattern) {
                filled.Add(wall.BottomLeftPattern);
                wall.BottomLeftPattern = pattern;
            }
            map[x + y * width] |= 2;
        }
        if (!floorMode) walls[x + y * width] = wall;
    } else {
        map[x + y * width] = 3;
    }

    spread.push(Point(x, y));
    return filled;
}

PatternReplaceCount VMArchitectureTools::FloorPatternFill(VMArchitecture* target, Point pos, uint16_t pattern, int8_t level) {
    if (pattern > 65533 || target->OutsideClip((int16_t)pos.X, (int16_t)pos.Y, level)) return PatternReplaceCount(true); // TS1(sims): FreeSO
    if (pattern > 65533 || pos.X < 0 || pos.X >= target->Width || pos.Y < 0 || pos.Y >= target->Height) return PatternReplaceCount();

    pos.X = std::max<int32_t>(std::min<int32_t>(pos.X, target->Width - 1), 0);
    pos.Y = std::max<int32_t>(std::min<int32_t>(pos.Y, target->Height - 1), 0);
    auto& walls = target->Walls[level - 1];

    auto width = target->Width;
    auto height = target->Height;
    PatternReplaceCount floorsCovered(true);

    std::vector<uint8_t> Map((size_t)(target->Width * target->Height), 0);

    //flood fill recursively. Each time choose find and choose the first "0" as the base.
    //The first recursion (outside) cannot fill into diagonals.
    std::stack<Point> spread;
    spread.push(pos);
    while (spread.size() > 0) {
        auto item = spread.top(); spread.pop();
        if (target->OutsideClip((int16_t)item.X, (int16_t)item.Y, level)) continue; //do not spread into OOB (TS1(sims): FreeSO)

        auto plusX = (item.X + 1) % width;
        auto minX = (item.X + width - 1) % width;
        auto plusY = (item.Y + 1) % height;
        auto minY = (item.Y + height - 1) % height;

        auto mainWalls = walls[item.X + item.Y * width];
        auto floor = target->GetFloor((int16_t)item.X, (int16_t)item.Y, level);
        if ((uint8_t)mainWalls.Segments > 15) {
            //draw floor onto a diagonal;
            auto wall = walls[item.X + item.Y * width];
            uint8_t flags = Map[item.X + item.Y * width];

            if (flags == 3) continue;
            if ((mainWalls.Segments & WallSegments::HorizontalDiag) > 0) flags = (uint8_t)(3 - flags);

            if ((flags & 1) == 1 && wall.TopLeftPattern != pattern) {
                floorsCovered.Add(wall.TopLeftPattern);
                wall.TopLeftPattern = pattern;
            } else if ((flags & 2) == 2 && wall.TopLeftStyle != pattern) {
                floorsCovered.Add(wall.TopLeftStyle);
                wall.TopLeftStyle = pattern;
            }

            walls[item.X + item.Y * width] = wall;
            continue; //don't spread on diagonals for now
        } else {
            //normal tile, draw a floor here.
            if (floor.Pattern != pattern) {
                auto old = floor.Pattern;
                floor.Pattern = pattern;
                if (target->SetFloor((int16_t)item.X, (int16_t)item.Y, level, floor, false)) floorsCovered.DAdd(old);
            }
        }

        if (Map[plusX + item.Y * width] < 3 && (mainWalls.Segments & WallSegments::BottomRight) == 0)
            SpreadOnto(walls, plusX, item.Y, 0, Map, width, height, spread, pattern, true);

        if (Map[minX + item.Y * width] < 3 && (mainWalls.Segments & WallSegments::TopLeft) == 0)
            SpreadOnto(walls, minX, item.Y, 2, Map, width, height, spread, pattern, true);

        if (Map[item.X + plusY * width] < 3 && (mainWalls.Segments & WallSegments::BottomLeft) == 0)
            SpreadOnto(walls, item.X, plusY, 1, Map, width, height, spread, pattern, true);

        if (Map[item.X + minY * width] < 3 && (mainWalls.Segments & WallSegments::TopRight) == 0)
            SpreadOnto(walls, item.X, minY, 3, Map, width, height, spread, pattern, true);

        walls[item.X + item.Y * width] = mainWalls;
    }
    return floorsCovered;
}

PatternReplaceCount VMArchitectureTools::FloorPatternRect(VMArchitecture* target, const st::Rectangle& rect, uint16_t dir, uint16_t pattern, int8_t level) {
    PatternReplaceCount floorsCovered(true);
    if (rect.Width == 0 && rect.Height == 0) {
        //dot mode, just fill a tile. can be a diagonal.
        if (rect.X < 0 || rect.X >= target->Width || rect.Y < 0 || rect.Y >= target->Width) return floorsCovered;
        if (target->OutsideClip((int16_t)rect.X, (int16_t)rect.Y, level)) return floorsCovered; // TS1(sims): FreeSO
        auto wall = target->GetWall((int16_t)rect.X, (int16_t)rect.Y, level);
        if ((wall.Segments & AnyDiag) > 0 && pattern < 65534) {
            bool side = ((wall.Segments & WallSegments::HorizontalDiag) > 0) ? (dir < 2) : (dir < 1 || dir > 2);
            if (side) {
                if (wall.TopLeftStyle != pattern) {
                    floorsCovered.Add(wall.TopLeftStyle);
                    wall.TopLeftStyle = pattern;
                }
            } else {
                if (wall.TopLeftPattern != pattern) {
                    floorsCovered.Add(wall.TopLeftPattern);
                    wall.TopLeftPattern = pattern;
                }
            }
            target->SetWall((int16_t)rect.X, (int16_t)rect.Y, level, wall);
        } else if ((wall.Segments & AnyDiag) == 0) {
            auto floor = target->GetFloor((int16_t)rect.X, (int16_t)rect.Y, level);
            if (floor.Pattern != pattern) {
                auto old = floor.Pattern;
                floor.Pattern = pattern;
                if (target->SetFloor((int16_t)rect.X, (int16_t)rect.Y, level, floor, false)) floorsCovered.DAdd(old);
            }
        }
        return floorsCovered;
    }

    auto xEnd = std::min(target->Width, rect.X + rect.Width + 1);
    auto yEnd = std::min(target->Height, rect.Y + rect.Height + 1);
    for (int y = std::max<int32_t>(0, rect.Y); y < yEnd; y++) {
        for (int x = std::max<int32_t>(0, rect.X); x < xEnd; x++) {
            if (target->OutsideClip((int16_t)x, (int16_t)y, level)) continue; // TS1(sims): never on the street
            auto wall = target->GetWall((int16_t)x, (int16_t)y, level);
            if ((wall.Segments & AnyDiag) > 0) //diagonal floors are stored in walls
            {
                if (pattern < 65534) continue;
                if (wall.TopLeftStyle != pattern) {
                    wall.TopLeftStyle = pattern;
                    floorsCovered.Add(wall.TopLeftStyle);
                }

                if (wall.TopLeftPattern != pattern) {
                    wall.TopLeftPattern = pattern;
                    floorsCovered.Add(wall.TopLeftPattern);
                }
                target->SetWall((int16_t)x, (int16_t)y, level, wall);
            } else {
                auto floor = target->GetFloor((int16_t)x, (int16_t)y, level);
                if (floor.Pattern != pattern) {
                    auto old = floor.Pattern;
                    floor.Pattern = pattern;
                    if (target->SetFloor((int16_t)x, (int16_t)y, level, floor, false)) floorsCovered.DAdd(old);
                }
            }
        }
    }
    return floorsCovered;
}

//==== TERRAIN ==== TS1(sims): FreeSO VMArchitectureTools
static double DistanceToRect(Point pt, const st::Rectangle& rect) {
    int xDist = 0;
    if (pt.X < rect.Left()) xDist = pt.X - rect.Left();
    else if (pt.X > rect.Right()) xDist = rect.Right() - pt.X;
    int yDist = 0;
    if (pt.Y < rect.Top()) yDist = pt.Y - rect.Top();
    else if (pt.Y > rect.Bottom()) yDist = rect.Bottom() - pt.Y;
    return std::sqrt((double)(xDist * xDist + yDist * yDist));
}

int32_t VMArchitectureTools::DotTerrain(VMArchitecture* target, Point pos, int16_t mod) {
    if (pos.X < 0 || pos.Y < 0 || pos.X >= target->Width || pos.Y >= target->Height) return 0;
    int current = target->GetTerrainGrass((int16_t)pos.X, (int16_t)pos.Y);
    auto n = (uint8_t)std::max(0, std::min(255, current + mod));
    target->SetTerrainGrass((int16_t)pos.X, (int16_t)pos.Y, n);
    return (std::abs(current - n) + 63) / 64;
}

int32_t VMArchitectureTools::RaiseTerrain(VMArchitecture* target, st::Rectangle pos, int16_t height, bool smoothMode) {
    //does a sort of flood fill from the start point to ensure the raise is valid
    // TS1(sims): the shipped lots have up to 20 altitude units between neighbouring vertices (FreeSO: 5)
    int constrain = 20 * 10;
    if (smoothMode) constrain = 1 * 10;
    // TS1(sims): a vertex is editable when the four tiles around it are user buildable and nothing that needs
    // level ground touches it (FreeSO skips both checks in TS1 mode: DisableClip)
    auto editable = [&](int x, int y) {
        for (int dy = -1; dy <= 0; dy++) for (int dx = -1; dx <= 0; dx++)
            if (target->OutsideClip((int16_t)(x + dx), (int16_t)(y + dy), 1)) return false;
        return true;
    };
    auto& tl = target->TerrainLimit;
    if (pos.X < tl.X || pos.Y < tl.Y || pos.X + pos.Width >= tl.Right() || pos.Y + pos.Height >= tl.Bottom()) return 0;
    std::map<std::pair<int, int>, int16_t> considered;
    std::stack<Point> stack;
    auto& tr = *target->Terrain;
    for (int x = 0; x <= pos.Width; x++) for (int y = 0; y <= pos.Height; y++) {
        Point p(pos.X + x, pos.Y + y);
        stack.push(p);
        considered[{p.X, p.Y}] = height;
        auto ht0 = tr.Heights[(size_t)(p.Y * tr.Width + p.X)];
        bool ignoreSlope = !(x == 0 || x == pos.Width || y == 0 || y == pos.Height);
        if (getenv("ST_DEBUG_TERRAIN")) printf("[terrain] vertex %d,%d h %d -> %d editable %d slopeok %d\n", p.X, p.Y, ht0, height, (int)editable(p.X, p.Y), (int)target->Context->SlopeVertexCheck(p.X, p.Y));
        if (ht0 != height && (!editable(p.X, p.Y) || !(ignoreSlope || target->Context->SlopeVertexCheck(p.X, p.Y)))) {
            target->LastFailReason = editable(p.X, p.Y) ? 1 : 2;
            return 0;
        }
    }
    while (!stack.empty()) {
        auto p = stack.top(); stack.pop();
        auto myHeight = considered[{p.X, p.Y}];
        Point adj[4] = { Point(p.X - 1, p.Y), Point(p.X + 1, p.Y), Point(p.X, p.Y - 1), Point(p.X, p.Y + 1) };
        for (auto& a : adj) {
            if (a.X < 0 || a.Y < 0 || a.X >= tr.Width || a.Y >= tr.Height) { target->LastFailReason = 3; return 0; }
            int16_t ht;
            auto it = considered.find({a.X, a.Y});
            ht = (it != considered.end()) ? it->second : tr.Heights[(size_t)(a.Y * tr.Width + a.X)];
            int diff = myHeight - ht;
            int first = height - ht;
            if (!(a.X >= tl.X && a.Y >= tl.Y && a.X < tl.Right() && a.Y < tl.Bottom())) {
                if (std::abs(diff) > 100 * 10) { target->LastFailReason = 2; return 0; }
                continue;
            }
            if (diff * first <= 0) continue;
            if (smoothMode) constrain = std::min(5, std::max(1, (int)std::round(DistanceToRect(a, pos)))) * 10;
            int16_t oht = ht;
            if (diff > constrain) ht = (int16_t)(myHeight - constrain);
            else if (diff < -constrain) ht = (int16_t)(myHeight + constrain);
            else continue;
            if (ht != oht && (!editable(a.X, a.Y) || !target->Context->SlopeVertexCheck(a.X, a.Y))) {
                target->LastFailReason = editable(a.X, a.Y) ? 1 : 2;
                return 0;
            }
            considered[{a.X, a.Y}] = ht;
            stack.push(a);
        }
    }
    int cost = 0;
    for (auto& change : considered) {
        int changedBy = std::abs(target->GetTerrainHeight((int16_t)change.first.first, (int16_t)change.first.second) - change.second);
        cost += (changedBy + 19) / 20;
        target->SetTerrainHeight((int16_t)change.first.first, (int16_t)change.first.second, change.second);
        // FreeSO: disturbed lawn turns brown (grows back in TS1)
        int gx = change.first.first - 1, gy = change.first.second - 1;
        if (gx >= 0 && gy >= 0 && changedBy > 0)
            target->SetTerrainGrass((int16_t)gx, (int16_t)gy, (uint8_t)std::min(255, target->GetTerrainGrass((int16_t)gx, (int16_t)gy) + changedBy * 6));
    }
    return cost;
}

std::vector<bool> VMArchitectureTools::GenerateRoomCut(VMArchitecture* target, int8_t floor, FSO::LotView::WorldRotation dir, const std::unordered_set<uint32_t>& cutRooms) {
    std::vector<bool> result((size_t)(target->Width * target->Height), false);
    int offset = 0;
    auto& roommap = target->Rooms[floor - 1].Map;
    auto cutDir = CutCheckDir[(int)dir];
    auto& walls = target->Walls[floor - 1];

    auto width = target->Width;

    for (int y1 = 0; y1 < target->Height; y1++) {
        for (int x1 = 0; x1 < target->Width; x1++) {
            if (walls[offset].Segments == 0
                && (offset + width < (int)walls.size() && walls[offset + width].Segments == 0)
                && (offset + 1 < (int)walls.size() && walls[offset + 1].Segments == 0)
                && (offset - width > 0 && walls[offset - width].Segments == 0)
                && (offset - 1 > 0 && walls[offset - 1].Segments == 0)
                ) {
                offset++;
                continue; //ignore empty tiles as an optimisation
            }
            bool cut = false;

            for (int i = 0; i < 3; i++) {
                auto x = x1 + ((i == 1) ? cutDir[0] : 0);
                auto y = y1 + ((i == 2) ? cutDir[1] : 0);
                for (int j = 0; j < ((i > 0) ? 4 : 5); j++) {
                    if (x < 0 || x >= target->Width || y < 0 || y >= target->Height) break;
                    if (cutRooms.count(roommap[y * target->Width + x] & 65535)) {
                        cut = true;
                        break;
                    }
                    x += cutDir[0];
                    y += cutDir[1];
                }
                if (cut) break;
            }
            result[offset++] = cut;
        }
    }
    return result;
}

bool VMArchitectureTools::ApplyCutRectangle(VMArchitecture* target, int8_t floor, std::vector<bool>& cuts, const st::Rectangle& rect) {
    auto& walls = target->Walls[floor - 1];
    auto width = target->Width;
    bool change = false;
    for (int x = rect.Left(); x < rect.Right(); x++) {
        for (int y = rect.Top(); y < rect.Bottom(); y++) {
            if (x < 0 || x >= target->Width || y < 0 || y >= target->Height) continue;
            auto offset = (y * target->Width + x);
            if (walls[offset].Segments == 0
                && (offset + width < (int)walls.size() && walls[offset + width].Segments == 0)
                && (offset + 1 < (int)walls.size() && walls[offset + 1].Segments == 0)
                && (offset - width > 0 && walls[offset - width].Segments == 0)
                && (offset - 1 > 0 && walls[offset - 1].Segments == 0)
                ) continue;

            if (!cuts[offset]) {
                cuts[offset] = true;
                change = true;
            }
        }
    }
    return change;
}

// ---- PatternReplaceCount

void PatternReplaceCount::Add(uint16_t id) {
    int value = (FloorMode) ? GetFloorPrice(id) : GetPatternPrice(id);
    Cost += value;
    Total++;
}

void PatternReplaceCount::DAdd(uint16_t id) {
    int value = (FloorMode) ? GetFloorPrice(id) : GetPatternPrice(id);
    Cost += value * 2;
    Total += 2;
}

int32_t PatternReplaceCount::GetPatternPrice(uint16_t id) {
    auto& e = FSO::Content::Content::Get()->WorldWalls->Entries;
    auto it = e.find(id);
    return (it == e.end()) ? 0 : it->second.Price;
}

int32_t PatternReplaceCount::GetFloorPrice(uint16_t id) {
    auto& e = FSO::Content::Content::Get()->WorldFloors->Entries;
    auto it = e.find(id);
    return (it == e.end()) ? 0 : it->second.Price;
}
}}}
