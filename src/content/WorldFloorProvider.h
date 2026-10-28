/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.Content.WorldFloorProvider (SimsVille/ContentManager/WorldFloorProvider.cs)
// Provides access to floor patterns.
//
// TS1 layout (instead of FreeSims' TSO paths objectdata/globals + housedata/floors*.far):
//   GameData/floors.iff - global floors (SPR2 i far, i+256 medium, i+512 near), pool/water tiles (SPR2 0x400..),
//   GameData/Build.iff  - catalog strings (STR# 0x83 floors, 0x85 water)
//   GameData/Floors/*.flr, Downloads/**.flr - custom floors (IFF: SPR2 1 far, 257 medium, 513 near; STR# 0),
//                         ids from 256 in sorted file name order; DynamicFloorFromID maps the lowercase file
//                         name (no extension) to that id, which is what VMWorldActivator.BuildFloorDict uses for FLRm.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include "content/model/WallFloor.h"

namespace FSO { namespace Files { namespace Formats { namespace IFF { class IffFile; } } } }

namespace FSO { namespace Content {

class Content;
class WorldFloorProvider;

class FloorReference {
public:
    uint16_t ID = 0;
    std::string FileName;

    int32_t Price = 0; //remember these, just in place of a catalog
    std::string Name;
    std::string Description;

    WorldFloorProvider* Provider = nullptr;
    Floor* Get(bool ts1);
};

class WorldFloorProvider {
public:
    explicit WorldFloorProvider(Content* contentManager = nullptr) : ContentManager(contentManager) {}

    std::unordered_map<uint16_t, FloorReference> Entries;
    std::unordered_map<std::string, uint16_t> DynamicFloorFromID;
    int32_t NumFloors = 0;

    /// Initiates loading of floors. basePath = folder containing GameData/.
    void Init();
    void Init(const std::string& basePath);

    FSO::Files::Formats::IFF::Chunks::SPR2* GetGlobalSPR(uint16_t id);

    Floor* Get(uint64_t id, bool ts1);

private:
    Content* ContentManager;
    std::unique_ptr<FSO::Files::Formats::IFF::IffFile> FloorGlobals;
    std::unordered_map<uint16_t, Floor> ById;
    std::unordered_map<std::string, std::unique_ptr<FSO::Files::Formats::IFF::IffFile>> FloorFiles;

    void AddFloor(const Floor& floor) { ById[floor.ID] = floor; }
};

}}
