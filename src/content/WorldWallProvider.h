/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.Content.WorldWallProvider (SimsVille/ContentManager/WorldWallProvider.cs)
// Provides access to wall patterns and wall styles.
//
// TS1 layout (instead of FreeSims' TSO paths objectdata/globals + housedata/walls*.far):
//   GameData/walls.iff  - global styles (SPR# 2..511 step 2, +1 = walls down, +512 medium, +1024 near),
//                         global patterns (SPR# 1536+i far, +256 medium, +512 near), junctions (SPR# 4096..4098)
//   GameData/Build.iff  - catalog strings (STR# 0x81 styles, 0x83 patterns)
//   GameData/Walls/*.wll, Downloads/**.wll - custom patterns (IFF: SPR# 1 far, 1793 medium, 2049 near; STR# 0 catalog),
//                         ids from 256 in sorted file name order; DynamicWallFromID maps the lowercase file
//                         name (no extension) to that id, which is what VMWorldActivator.BuildWallDict uses for WALm.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <unordered_map>
#include "content/model/WallFloor.h"

namespace FSO { namespace Files { namespace Formats { namespace IFF { class IffFile; } } } }

namespace FSO { namespace Content {

class Content;
class WorldWallProvider;

class WallReference {
public:
    uint16_t ID = 0;
    std::string FileName;

    int32_t Price = 0; //remember these, just in place of a catalog
    std::string Name;
    std::string Description;

    WorldWallProvider* Provider = nullptr;
    Wall* Get(bool ts1);
};

class WorldWallProvider {
public:
    explicit WorldWallProvider(Content* contentManager = nullptr) : ContentManager(contentManager) {}

    Wall Junctions;
    std::unordered_map<std::string, uint16_t> DynamicWallFromID;
    std::unordered_map<uint16_t, WallReference> Entries;
    int32_t NumWalls = 0;

    static const uint16_t WallStyleIDs[5];
    static const uint16_t WallStylePatterns[5];
    std::unordered_map<uint16_t, int32_t> WallStyleToIndex;

    /// Initiates loading of walls. basePath = folder containing GameData/.
    void Init();
    void Init(const std::string& basePath);

    /// Adds a dynamic wall style to WorldWallProvider. Returns the ID of the wallstyle.
    uint16_t AddDynamicWallStyle(WallStyle input);

    /// Gets a wallstyle instance from WorldWallProvider.
    WallStyle* GetWallStyle(uint64_t id);

    Wall* Get(uint64_t id, bool ts1);

    // FSO_CPP: kept for the old stub's callers (iteration over styles).
    std::map<uint16_t, WallStyle>& Styles() { return StyleById; }
    /// TS1(sims): walls.iff (catalogue BMP_ of the wall styles)
    FSO::Files::Formats::IFF::IffFile* Globals() { return WallGlobals.get(); }

private:
    Content* ContentManager;
    std::unique_ptr<FSO::Files::Formats::IFF::IffFile> WallGlobals;
    std::map<uint16_t, WallStyle> StyleById;
    std::unordered_map<uint16_t, Wall> ById;
    uint16_t DynamicStyleID = 256;
    // custom wall files (FAR1Provider<IffFile> in C#): loaded on first use, kept for the session.
    std::unordered_map<std::string, std::unique_ptr<FSO::Files::Formats::IFF::IffFile>> WallFiles;

    void AddWall(const Wall& wall) { ById[wall.ID] = wall; }
    void AddWallStyle(const WallStyle& wall) { StyleById[wall.ID] = wall; }
};

}}
