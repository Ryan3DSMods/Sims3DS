/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.Content.Model.Wall / WallStyle / Floor (SimsVille/ContentManager/model/{Wall,WallStyle,Floor}.cs)
#pragma once
#include <cstdint>
#include <string>

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks { class SPR; class SPR2; } } } } }

namespace FSO { namespace Content {

/// A wall pattern (wallpaper). One SPR# per zoom; frames are the 4 wall sides
/// (0 = top left, 1 = top right, 2 = horizontal diagonal, 3 = vertical diagonal).
class Wall {
public:
    uint16_t ID = 0;
    FSO::Files::Formats::IFF::Chunks::SPR* Near = nullptr;
    FSO::Files::Formats::IFF::Chunks::SPR* Medium = nullptr;
    FSO::Files::Formats::IFF::Chunks::SPR* Far = nullptr;
};

/// A wall style (the alpha masks for normal walls, fences, cutaway shapes...).
class WallStyle {
public:
    uint16_t ID = 0;
    FSO::Files::Formats::IFF::Chunks::SPR* WallsUpNear = nullptr;
    FSO::Files::Formats::IFF::Chunks::SPR* WallsUpMedium = nullptr;
    FSO::Files::Formats::IFF::Chunks::SPR* WallsUpFar = nullptr;
    FSO::Files::Formats::IFF::Chunks::SPR* WallsDownNear = nullptr;
    FSO::Files::Formats::IFF::Chunks::SPR* WallsDownMedium = nullptr;
    FSO::Files::Formats::IFF::Chunks::SPR* WallsDownFar = nullptr;

    int32_t Price = 0;
    std::string Name;
    std::string Description;
};

/// A floor pattern. One SPR2 per zoom (frame 0 used).
class Floor {
public:
    uint16_t ID = 0;
    FSO::Files::Formats::IFF::Chunks::SPR2* Near = nullptr;
    FSO::Files::Formats::IFF::Chunks::SPR2* Medium = nullptr;
    FSO::Files::Formats::IFF::Chunks::SPR2* Far = nullptr;
};

}}
