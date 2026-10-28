/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.LotView.Model.LotTilePos
#pragma once
#include <cstdint>
#include <cmath>
#include "core/math.h"

namespace FSO { namespace LotView { namespace Model {

struct LotTilePos {
    int16_t x = 0;
    int16_t y = 0;
    int8_t Level = 0;

    LotTilePos() {}
    LotTilePos(int16_t x_, int16_t y_, int8_t level) : x(x_), y(y_), Level(level) {}

    int32_t TileID() const { return (int32_t)TileX() | ((int32_t)TileY() << 8) | ((int32_t)Level << 16); }

    static LotTilePos FromBigTile(int16_t x, int16_t y, int8_t level) {
        return LotTilePos((int16_t)((x << 4) + 8), (int16_t)((y << 4) + 8), level);
    }

    //TODO: uses of the below indicate unsafe operations. We shouldn't have any of these by the time we go live.
    static LotTilePos FromVec3(const st::Vector3& pos) {
        return LotTilePos((int16_t)std::nearbyint(pos.X * 16), (int16_t)std::nearbyint(pos.Y * 16), (int8_t)(std::nearbyint(pos.Z / 2.95) + 1));
    }

    static int32_t Distance(const LotTilePos& a, const LotTilePos& b) {
        return (int32_t)std::sqrt((double)((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y)));
        //TODO: consider level? does anything need this?
    }

    LotTilePos operator+(const LotTilePos& c2) const { return LotTilePos((int16_t)(x + c2.x), (int16_t)(y + c2.y), (int8_t)(Level + c2.Level)); } //use for offsets ONLY!
    LotTilePos operator*(int c2) const { return LotTilePos((int16_t)(x * c2), (int16_t)(y * c2), (int8_t)(Level * c2)); } //use for offsets ONLY!
    LotTilePos operator-(const LotTilePos& c2) const { return LotTilePos((int16_t)(x - c2.x), (int16_t)(y - c2.y), (int8_t)(Level - c2.Level)); } //use for offsets ONLY!
    LotTilePos operator/(int div) const { return LotTilePos((int16_t)(x / div), (int16_t)(y / div), (int8_t)(Level / div)); } //use for offsets ONLY!
    bool operator==(const LotTilePos& c2) const { return x == c2.x && y == c2.y && Level == c2.Level; }
    bool operator!=(const LotTilePos& c2) const { return !(*this == c2); }

    int16_t TileX() const { return (int16_t)(x >> 4); }
    void TileX(int16_t value) { x = (int16_t)(value << 4); }
    int16_t TileY() const { return (int16_t)(y >> 4); }
    void TileY(int16_t value) { y = (int16_t)(value << 4); }

    st::Vector3 ToVector3() const { return st::Vector3(x / 16.0f, y / 16.0f, (Level - 1) * 2.95f); }

    static const LotTilePos OUT_OF_WORLD;
};

inline const LotTilePos LotTilePos::OUT_OF_WORLD = LotTilePos(-32768, -32768, 1);

}}}
