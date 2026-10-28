/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Utils.VMArchitectureTools / PatternReplaceCount
#pragma once
#include <cstdint>
#include <vector>
#include <stack>
#include <unordered_set>
#include "core/math.h"
#include "world/model/WallTile.h"
#include "world/model/WorldRotation.h"
namespace FSO { namespace SimAntics {
class VMArchitecture;
namespace Utils {

struct PatternReplaceCount {
    int32_t Cost = 0;
    int32_t Total = 0;
    bool FloorMode = false;
    PatternReplaceCount() {}
    explicit PatternReplaceCount(bool floor) : Cost(0), Total(0), FloorMode(floor) {}
    PatternReplaceCount operator+(const PatternReplaceCount& p2) const { PatternReplaceCount p1 = *this; p1.Cost += p2.Cost; p1.Total += p2.Total; return p1; }
    PatternReplaceCount& operator+=(const PatternReplaceCount& p2) { Cost += p2.Cost; Total += p2.Total; return *this; }
    void Add(uint16_t id);
    void DAdd(uint16_t id);
private:
    static int32_t GetPatternPrice(uint16_t id);
    static int32_t GetFloorPrice(uint16_t id);
};

class VMArchitectureTools {
public:
    //things 2 note
    //default style is 1
    //default pattern is 0
    //mid drawing pattern/style is 255
    static int32_t DrawWall(VMArchitecture* target, st::Point pos, int32_t length, int32_t direction, uint16_t pattern, uint16_t style, int8_t level, bool force);
    static int32_t EraseWall(VMArchitecture* target, st::Point pos, int32_t length, int32_t direction, uint16_t pattern, uint16_t style, int8_t level);
    static bool VerifyEraseWall(VMArchitecture* target, st::Point pos, int32_t length, int32_t direction, int8_t level);
    static bool VerifyDrawWall(VMArchitecture* target, st::Point pos, int32_t length, int32_t direction, int8_t level);
    static int32_t DrawWallRect(VMArchitecture* target, const st::Rectangle& rect, uint16_t pattern, uint16_t style, int8_t level);
    static int32_t GetPatternDirection(VMArchitecture* target, st::Point pos, uint16_t pattern, int32_t direction, int32_t altDir, int8_t level);
    static PatternReplaceCount WallPatternDot(VMArchitecture* target, st::Point pos, uint16_t pattern, int32_t direction, int32_t altDir, int8_t level);
    /// Fills a room with a certain wall pattern. Returns walls covered
    static PatternReplaceCount WallPatternFill(VMArchitecture* target, st::Point pos, uint16_t pattern, int8_t level);
    /// Fills a room with a certain Floor pattern. Returns floors covered
    static PatternReplaceCount FloorPatternFill(VMArchitecture* target, st::Point pos, uint16_t pattern, int8_t level);
    static PatternReplaceCount FloorPatternRect(VMArchitecture* target, const st::Rectangle& rect, uint16_t dir, uint16_t pattern, int8_t level);
    // TS1(sims): FreeSO terrain tools
    static int32_t DotTerrain(VMArchitecture* target, st::Point pos, int16_t mod);
    static int32_t RaiseTerrain(VMArchitecture* target, st::Rectangle pos, int16_t height, bool smoothMode);
    static const int32_t CutCheckDir[4][2];
    static std::vector<bool> GenerateRoomCut(VMArchitecture* target, int8_t floor, FSO::LotView::WorldRotation dir, const std::unordered_set<uint32_t>& cutRooms);
    /// Cuts the specified rectangle. Returns true if a *potentially noticable change* occurred as a result.
    static bool ApplyCutRectangle(VMArchitecture* target, int8_t floor, std::vector<bool>& cuts, const st::Rectangle& rect);
private:
    static const st::Point WLStartOff[8];
    static const FSO::LotView::Model::WallSegments WLMainSeg[8];
    static const st::Point WLSubOff[4];
    static const FSO::LotView::Model::WallSegments WLSubSeg[4];
    static const st::Point WLStep[8];
    static const FSO::LotView::Model::WallSegments AnyDiag;
    static PatternReplaceCount SpreadOnto(std::vector<FSO::LotView::Model::WallTile>& walls, int32_t x, int32_t y, int32_t inDir, std::vector<uint8_t>& map, int32_t width, int32_t height, std::stack<st::Point>& spread, uint16_t pattern, bool floorMode);
};
}}}
