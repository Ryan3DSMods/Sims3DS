/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.Content.WorldFloorProvider — see WorldFloorProvider.h
#include "content/WorldFloorProvider.h"
#include "content/ContentManager.h"
#include "content/WorldContentFiles.h"
#include "content/TS1ExpansionContent.h"
#include "files/formats/iff/IffFile.h"
#include "files/formats/iff/chunks/SPR2.h"
#include "files/formats/iff/chunks/STR.h"
#include <cstdlib>
#include <cstdio>

namespace FSO { namespace Content {

using FSO::Files::Formats::IFF::IffFile;
using FSO::Files::Formats::IFF::Chunks::SPR2;
using FSO::Files::Formats::IFF::Chunks::STR;
namespace WCF = WorldContentFiles;

Floor* FloorReference::Get(bool ts1) { return Provider->Get(ID, ts1); }

void WorldFloorProvider::Init() {
    Content* c = (ContentManager != nullptr) ? ContentManager : Content::Get();
    Init(c->BasePath);
}

static std::string SafeString(STR* s, int32_t i) { return (s != nullptr && i < s->Length()) ? s->GetString(i) : std::string(); }

void WorldFloorProvider::Init(const std::string& basePath) {
    Entries.clear();
    ById.clear();
    DynamicFloorFromID.clear();
    FloorFiles.clear();

    auto floorGlobalsPath = WCF::FindInstall(basePath, "GameData/floors.iff");
    if (floorGlobalsPath.empty()) {
        // STUB(3ds): C# throws when the file is missing; we keep an empty catalog so the VM still runs.
        printf("WorldFloorProvider: GameData/floors.iff not found under %s\n", basePath.c_str());
        return;
    }
    FloorGlobals = IffFile::OpenReadOnly(floorGlobalsPath); // TS1(sims): deferred pictures

    auto buildGlobalsPath = WCF::FindInstall(basePath, "GameData/Build.iff");
    std::unique_ptr<IffFile> buildGlobals;
    if (!buildGlobalsPath.empty()) buildGlobals = IffFile::OpenReadOnly(buildGlobalsPath); //todo: centralize?

    /** There is a small handful of floors in a global file for some reason **/
    uint16_t floorID = 1;
    // TS1(sims): FreeSims (TSO build.iff) reads floor names from STR# 0x83 and loops i < Length/3.
    // In TS1 Build.iff, 0x83 is "Wallpaper" and floors are STR# 0x82 ("Floor"); floors.iff holds SPR2 1..29.
    // So we register every floor id 1..255 that has sprites in floors.iff, named from STR# 0x82.
    STR* floorStrs = buildGlobals ? buildGlobals->Get<STR>(0x82) : nullptr;
    for (uint16_t i = 1; i < 256; i++) {
        Floor f;
        f.ID = floorID;
        f.Far = FloorGlobals->Get<SPR2>(i);
        f.Medium = FloorGlobals->Get<SPR2>((uint16_t)(i + 256));
        f.Near = FloorGlobals->Get<SPR2>((uint16_t)(i + 512)); //2048 is water tile
        if (f.Far == nullptr && f.Medium == nullptr && f.Near == nullptr) { floorID++; continue; }
        AddFloor(f);

        FloorReference ref;
        ref.ID = floorID;
        ref.FileName = "global";
        ref.Name = SafeString(floorStrs, (i - 1) * 3 + 1);
        ref.Price = atoi(SafeString(floorStrs, (i - 1) * 3 + 0).c_str());
        ref.Description = SafeString(floorStrs, (i - 1) * 3 + 2);
        ref.Provider = this;
        Entries[floorID] = ref;

        floorID++;
    }

    STR* waterStrs = buildGlobals ? buildGlobals->Get<STR>(0x85) : nullptr;
    //add pools for catalog logic
    {
        FloorReference ref;
        ref.ID = 65535;
        ref.FileName = "global";
        ref.Price = atoi(SafeString(waterStrs, 0).c_str());
        ref.Name = SafeString(waterStrs, 1);
        ref.Description = SafeString(waterStrs, 2);
        ref.Provider = this;
        Entries[65535] = ref;
    }
    {   // TS1(sims): Hot Date water tool (Build.iff STR# 133 entries 3..5: "15", "Water Tool")
        FloorReference ref;
        ref.ID = 65534;
        ref.FileName = "global";
        ref.Price = atoi(SafeString(waterStrs, 3).c_str());
        ref.Name = SafeString(waterStrs, 4);
        ref.Description = SafeString(waterStrs, 5);
        ref.Provider = this;
        Entries[65534] = ref;
    }

    floorID = 256;

    // C#: housedata/floors{,2,3,4}/floors*.far (TSO). TS1 keeps each custom floor as a loose .flr IFF.
    std::vector<std::string> files = WCF::FilesWithExt(WCF::FindInstall(basePath, "GameData/Floors"), ".flr", false, true);
    for (auto& f : WCF::FilesWithExt(WCF::FindInstall(basePath, "Downloads"), ".flr", true, true)) files.push_back(f);

    for (auto& path : files) {
        auto key = WCF::Lower(WCF::BaseNameNoExt(path));
        if (DynamicFloorFromID.count(key)) continue; //first wins
        DynamicFloorFromID[key] = floorID;

        std::unique_ptr<IffFile> iff = IffFile::OpenReadOnly(path); // TS1(sims): sprites read when first drawn
        auto catStrings = iff->Get<STR>(0);

        FloorReference ref;
        ref.ID = floorID;
        ref.FileName = path;
        ref.Name = SafeString(catStrings, 0);
        ref.Price = atoi(SafeString(catStrings, 1).c_str());
        ref.Description = SafeString(catStrings, 2);
        ref.Provider = this;
        Entries[floorID] = ref;
        FloorFiles[path] = std::move(iff);

        floorID++;
    }

    // TS1(sims): the expansion packs' walls/floors inside their .far archives (TS1ExpansionContent.h): catalog
    // strings from the cache, the iff itself is read when first drawn (Get).
    {
        Content* c = (ContentManager != nullptr) ? ContentManager : Content::Get();
        for (auto& item : TS1::TS1ExpansionContent::ListArchiveItems(c, ".flr")) {
            if (DynamicFloorFromID.count(item.Key)) continue; //first wins (loose GameData/Downloads files first)
            DynamicFloorFromID[item.Key] = floorID;
            FloorReference ref;
            ref.ID = floorID;
            ref.FileName = TS1::TS1ExpansionContent::MakeReference(item);
            ref.Name = item.Name;
            ref.Price = item.Price;
            ref.Description = item.Description;
            ref.Provider = this;
            Entries[floorID] = ref;
            floorID++;
        }
    }
    NumFloors = floorID;
}

SPR2* WorldFloorProvider::GetGlobalSPR(uint16_t id) {
    return FloorGlobals ? FloorGlobals->Get<SPR2>(id) : nullptr;
}

Floor* WorldFloorProvider::Get(uint64_t id, bool ts1) {
    (void)ts1;
    auto it = ById.find((uint16_t)id);
    if (it != ById.end()) return &it->second;
    //get from iff
    auto ent = Entries.find((uint16_t)id);
    if (ent == Entries.end()) return nullptr;
    auto fit = FloorFiles.find(ent->second.FileName);
    IffFile* iff = (fit == FloorFiles.end()) ? nullptr : fit->second.get();
    if (iff == nullptr && TS1::TS1ExpansionContent::IsArchiveReference(ent->second.FileName)) {
        // TS1(sims): expansion wall/floor, read on first use
        auto loaded = TS1::TS1ExpansionContent::LoadArchiveIff((ContentManager != nullptr) ? ContentManager : Content::Get(), ent->second.FileName);
        iff = loaded.get();
        if (iff != nullptr) FloorFiles[ent->second.FileName] = std::move(loaded);
    }
    if (iff == nullptr) return nullptr;

    Floor f;
    f.ID = (uint16_t)id;
    f.Far = iff->Get<SPR2>(1);
    f.Medium = iff->Get<SPR2>(257);
    f.Near = iff->Get<SPR2>(513);
    ById[(uint16_t)id] = f;
    return &ById[(uint16_t)id];
}

}}
