/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.Common.Utils.TextureGenerator (sims.common/utils/TextureGenerator.cs) — only the world z-buffer
// generators used by the 2D world: GetWallZBuffer (walls, floors, junctions, pool corners, balloons) and
// GetAirTiles. Generated once as CPU byte arrays (Alpha8 for depth, Color for the air tiles).
#pragma once
#include <cstdint>
#include <vector>
#include "core/math.h"
#include "world/utils/_2DSprite.h"

namespace FSO { namespace Common { namespace Utils {

class TextureGenerator {
public:
    static const float FLAT_Z_INC;
    // format: width, height, startIntensity, Xdiff, Ydiff
    static const float WallZBufferConfig[31][5];

    /// 31 depth textures, indices as in the C# config (0-11 walls, 12-14 floors, 15-17 flipped floors,
    /// 18-20 junctions walls up, 21-29 pool corners, 30 balloon).
    static std::vector<FSO::LotView::Utils::Texture2D>& GetWallZBuffer();
    static std::vector<FSO::LotView::Utils::Texture2D>& GetAirTiles();

private:
    static std::vector<std::vector<uint8_t>> WallZBufferData;
    static std::vector<FSO::LotView::Utils::Texture2D> WallZBuffer;
    static std::vector<std::vector<st::Color>> AirTileData;
    static std::vector<FSO::LotView::Utils::Texture2D> AirTiles;
    static void GenerateAirTile(int width, int height, std::vector<st::Color>& data);
};

}}}
