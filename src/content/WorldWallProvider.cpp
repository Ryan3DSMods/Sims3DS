/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.Content.WorldWallProvider — see WorldWallProvider.h
#include "content/WorldWallProvider.h"
#include "content/ContentManager.h"
#include "content/WorldContentFiles.h"
#include "content/TS1ExpansionContent.h"
#include "files/formats/iff/IffFile.h"
#include "files/formats/iff/chunks/SPR.h"
#include "files/formats/iff/chunks/STR.h"
#include <cstdlib>
#include <cstdio>

namespace FSO { namespace Content {

using FSO::Files::Formats::IFF::IffFile;
using FSO::Files::Formats::IFF::Chunks::SPR;
using FSO::Files::Formats::IFF::Chunks::STR;
namespace WCF = WorldContentFiles;

const uint16_t WorldWallProvider::WallStyleIDs[5] = {
    0x1, //wall
    0x2, //picket fence
    0xD, //iron fence
    0xC, //privacy fence
    0xE  //banisters
};

const uint16_t WorldWallProvider::WallStylePatterns[5] = {
    0,   //wall
    248, //picket fence
    250, //iron fence
    249, //privacy fence
    251, //banisters
};

Wall* WallReference::Get(bool ts1) { return Provider->Get(ID, ts1); }

void WorldWallProvider::Init() {
    Content* c = (ContentManager != nullptr) ? ContentManager : Content::Get();
    Init(c->BasePath);
}

static std::string SafeString(STR* s, int32_t i) { return (s != nullptr && i < s->Length()) ? s->GetString(i) : std::string(); }

void WorldWallProvider::Init(const std::string& basePath) {
    WallStyleToIndex.clear();
    for (int i = 0; i < 5; i++) WallStyleToIndex[WallStyleIDs[i]] = i;

    Entries.clear();
    ById.clear();
    DynamicWallFromID.clear();
    StyleById.clear();
    WallFiles.clear();

    auto wallGlobalsPath = WCF::FindInstall(basePath, "GameData/walls.iff");
    if (wallGlobalsPath.empty()) {
        // STUB(3ds): C# throws when the file is missing; we keep an empty catalog so the VM still runs.
        printf("WorldWallProvider: GameData/walls.iff not found under %s\n", basePath.c_str());
        return;
    }
    WallGlobals = IffFile::OpenReadOnly(wallGlobalsPath); // TS1(sims): deferred pictures

    auto buildGlobalsPath = WCF::FindInstall(basePath, "GameData/Build.iff");
    std::unique_ptr<IffFile> buildGlobals;
    if (!buildGlobalsPath.empty()) buildGlobals = IffFile::OpenReadOnly(buildGlobalsPath); //todo: centralize?

    /** Get wall styles from globals file **/
    STR* styleStrs = buildGlobals ? buildGlobals->Get<STR>(0x81) : nullptr;
    uint16_t wallID = 1;
    for (uint16_t i = 2; i < 512; i += 2) {
        auto far = WallGlobals->Get<SPR>(i);
        auto medium = WallGlobals->Get<SPR>((uint16_t)(i + 512));
        auto near = WallGlobals->Get<SPR>((uint16_t)(i + 1024));

        auto fard = WallGlobals->Get<SPR>((uint16_t)(i + 1));
        auto mediumd = WallGlobals->Get<SPR>((uint16_t)(i + 513));
        auto neard = WallGlobals->Get<SPR>((uint16_t)(i + 1025));

        if (fard == nullptr) { //no walls down, just render exactly the same
            fard = far;
            mediumd = medium;
            neard = near;
        }

        WallStyle style;
        style.ID = wallID;
        style.WallsUpFar = far;
        style.WallsUpMedium = medium;
        style.WallsUpNear = near;
        style.WallsDownFar = fard;
        style.WallsDownMedium = mediumd;
        style.WallsDownNear = neard;
        style.Price = -1;

        auto bi = WallStyleToIndex.find(wallID);
        if (bi != WallStyleToIndex.end()) {
            int buyIndex = bi->second;
            style.Price = atoi(SafeString(styleStrs, buyIndex * 3).c_str());
            style.Name = SafeString(styleStrs, buyIndex * 3 + 1);
            style.Description = SafeString(styleStrs, buyIndex * 3 + 2);
        }
        AddWallStyle(style);
        wallID++;
    }

    DynamicStyleID = 256; //styles loaded from objects start at 256. The objd reference is dynamically altered to reference this new id,
    //so only refresh wall cache at same time as obj cache! (do this on lot unload)

    /** Get wall patterns from globals file **/
    STR* wallStrs = buildGlobals ? buildGlobals->Get<STR>(0x83) : nullptr;
    int32_t numStrs = (wallStrs != nullptr) ? wallStrs->Length() : 0;

    wallID = 0;
    for (uint16_t i = 0; i < 256; i++) {
        Wall w;
        w.ID = wallID;
        w.Far = WallGlobals->Get<SPR>((uint16_t)(i + 1536));
        w.Medium = WallGlobals->Get<SPR>((uint16_t)(i + 1536 + 256));
        w.Near = WallGlobals->Get<SPR>((uint16_t)(i + 1536 + 512));
        AddWall(w);

        if (i > 0 && i < (numStrs / 3) + 1) {
            WallReference ref;
            ref.ID = wallID;
            ref.FileName = "global";
            ref.Name = SafeString(wallStrs, (i - 1) * 3 + 1);
            ref.Price = atoi(SafeString(wallStrs, (i - 1) * 3 + 0).c_str());
            ref.Description = SafeString(wallStrs, (i - 1) * 3 + 2);
            ref.Provider = this;
            Entries[wallID] = ref;
        }
        wallID++;
    }

    Junctions.ID = wallID;
    Junctions.Far = WallGlobals->Get<SPR>(4096);
    Junctions.Medium = WallGlobals->Get<SPR>(4097);
    Junctions.Near = WallGlobals->Get<SPR>(4098);

    wallID = 256;

    // C#: housedata/walls{,2,3,4}/walls*.far (TSO). TS1 keeps each custom wall as a loose .wll IFF.
    std::vector<std::string> files = WCF::FilesWithExt(WCF::FindInstall(basePath, "GameData/Walls"), ".wll", false, true);
    for (auto& f : WCF::FilesWithExt(WCF::FindInstall(basePath, "Downloads"), ".wll", true, true)) files.push_back(f);

    for (auto& path : files) {
        auto key = WCF::Lower(WCF::BaseNameNoExt(path));
        if (DynamicWallFromID.count(key)) continue; //same wall twice (Downloads copy): first wins
        DynamicWallFromID[key] = wallID;

        // C# reads each iff once at init for its catalog strings; we do the same and keep it (the sprites
        // stay compressed until a frame is first drawn).
        std::unique_ptr<IffFile> iff = IffFile::OpenReadOnly(path); // TS1(sims): sprites read when first drawn
        auto catStrings = iff->Get<STR>(0);

        WallReference ref;
        ref.ID = wallID;
        ref.FileName = path;
        ref.Name = SafeString(catStrings, 0);
        ref.Price = atoi(SafeString(catStrings, 1).c_str());
        ref.Description = SafeString(catStrings, 2);
        ref.Provider = this;
        Entries[wallID] = ref;
        WallFiles[path] = std::move(iff);

        wallID++;
    }
    // TS1(sims): the expansion packs' walls/floors inside their .far archives (TS1ExpansionContent.h): catalog
    // strings from the cache, the iff itself is read when first drawn (Get).
    {
        Content* c = (ContentManager != nullptr) ? ContentManager : Content::Get();
        for (auto& item : TS1::TS1ExpansionContent::ListArchiveItems(c, ".wll")) {
            if (DynamicWallFromID.count(item.Key)) continue; //first wins (loose GameData/Downloads files first)
            DynamicWallFromID[item.Key] = wallID;
            WallReference ref;
            ref.ID = wallID;
            ref.FileName = TS1::TS1ExpansionContent::MakeReference(item);
            ref.Name = item.Name;
            ref.Price = item.Price;
            ref.Description = item.Description;
            ref.Provider = this;
            Entries[wallID] = ref;
            wallID++;
        }
    }
    NumWalls = wallID;
}

uint16_t WorldWallProvider::AddDynamicWallStyle(WallStyle input) { //adds a new wall and returns its id
    input.ID = DynamicStyleID++;
    AddWallStyle(input);
    return (uint16_t)(DynamicStyleID - 1);
}

WallStyle* WorldWallProvider::GetWallStyle(uint64_t id) {
    auto it = StyleById.find((uint16_t)id);
    return (it == StyleById.end()) ? nullptr : &it->second;
}

Wall* WorldWallProvider::Get(uint64_t id, bool ts1) {
    (void)ts1;
    auto it = ById.find((uint16_t)id);
    if (it != ById.end()) return &it->second;
    //get from iff
    auto ent = Entries.find((uint16_t)id);
    if (ent == Entries.end()) return nullptr; // C# throws KeyNotFound here; the renderer treats null as "no pattern"
    auto fit = WallFiles.find(ent->second.FileName);
    IffFile* iff = (fit == WallFiles.end()) ? nullptr : fit->second.get();
    if (iff == nullptr && TS1::TS1ExpansionContent::IsArchiveReference(ent->second.FileName)) {
        // TS1(sims): expansion wall/floor, read on first use
        auto loaded = TS1::TS1ExpansionContent::LoadArchiveIff((ContentManager != nullptr) ? ContentManager : Content::Get(), ent->second.FileName);
        iff = loaded.get();
        if (iff != nullptr) WallFiles[ent->second.FileName] = std::move(loaded);
    }
    if (iff == nullptr) return nullptr;

    Wall w;
    w.ID = (uint16_t)id;
    w.Far = iff->Get<SPR>(1);
    w.Medium = iff->Get<SPR>(1793);
    w.Near = iff->Get<SPR>(2049);
    ById[(uint16_t)id] = w;
    return &ById[(uint16_t)id];
}

}}
