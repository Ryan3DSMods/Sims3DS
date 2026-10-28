/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.Content (SimsVille/ContentManager) — the subset the SimAntics VM needs, backed by the
// Sims 1 FAR1 archives on disk (GameData/Objects/Objects.far, GameData/Global/Global.far,
// ExpansionPack*/*.far, Deluxe/Deluxe.far) plus loose .iff files.
//
// Translated: WorldObjectProvider (GUID -> GameObject, by scanning every OBJD and caching the
// result), WorldGlobalProvider (global / semi-global iff + otf), GameObject / GameIffResource /
// GameObjectResource / GameGlobalResource / VMTreeByNameTableEntry.
// STUB(3ds): avatar content (animations, appearances, outfits, skeletons, purchasables),
// wall/floor/roof catalogs and the TSO GlobalTuning are minimal shells (see STUBS.md).
#pragma once
#include "common/TS1/TS1Curve.h" // TS1(sims)
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <functional>
#include <type_traits>
#include "simantics/_files_contract.h"
#include "vitaboy/Animation.h"
#include "vitaboy/Appearance.h"

namespace FSO { namespace Content {

using FSO::Files::Formats::IFF::IffFile;
using FSO::Files::Formats::OTF::OTFFile;
using FSO::Files::Formats::OTF::OTFTable;
namespace Chunks = FSO::Files::Formats::IFF::Chunks;

class Content;
class GameGlobalResource;
// TS1(sims): neighbourhood / careers (content/TS1NeighbourProvider.h, content/TS1JobProvider.h)
namespace TS1 { class TS1NeighborhoodProvider; class TS1JobProvider; }

class VMTreeByNameTableEntry {
public:
    Chunks::BHAV* bhav;
    explicit VMTreeByNameTableEntry(Chunks::BHAV* b) : bhav(b) {}
};

class GameIffResource {
public:
    virtual ~GameIffResource() {}
    virtual IffFile* MainIff() = 0;
    virtual IffFile* SpritesIff() { return nullptr; }
    virtual OTFFile* TuningFile() { return nullptr; }

    std::unordered_map<uint32_t, int16_t> TuningCache;
    GameGlobalResource* SemiGlobal = nullptr;

    // C# generic Get<T>: OTFTable comes from the tuning file, chunks from the iff (then sprites iff).
    template<class T> T* Get(uint16_t id) {
        if constexpr (std::is_same<T, OTFTable>::value) {
            OTFFile* tuning = TuningFile();
            if (tuning != nullptr) return tuning->GetTable(id);
            return nullptr;
        } else {
            IffFile* iff = MainIff();
            if (iff != nullptr) {
                T* item1 = iff->template Get<T>(id);
                if (item1 != nullptr) return item1;
            }
            IffFile* sprites = SpritesIff();
            if (sprites != nullptr) {
                T* item2 = sprites->template Get<T>(id);
                if (item2 != nullptr) return item2;
            }
            return nullptr;
        }
    }

    template<class T> std::vector<T*> List() {
        if constexpr (std::is_same<T, Chunks::SPR2>::value || std::is_same<T, Chunks::SPR>::value || std::is_same<T, Chunks::DGRP>::value) {
            IffFile* sprites = SpritesIff();
            if (sprites != nullptr) return sprites->template List<T>();
        }
        IffFile* iff = MainIff();
        if (iff == nullptr) return std::vector<T*>();
        return iff->template List<T>();
    }

    template<class T> std::vector<T*> ListArray() { return List<T>(); }

    virtual void Recache();
};

/// A global can be an OTF (Object Tuning File) or an IFF.
class GameGlobalResource : public GameIffResource {
public:
    IffFile* Iff;
    OTFFile* Tuning;
    GameGlobalResource(IffFile* iff, OTFFile* tuning) : Iff(iff), Tuning(tuning) {}
    IffFile* MainIff() override { return Iff; }
    OTFFile* TuningFile() override { return Tuning; }
};

class GameGlobal {
public:
    GameGlobalResource* Resource = nullptr;
};

/// The resource for an object in the game world.
class GameObjectResource : public GameIffResource {
public:
    //DO NOT USE THESE, THEY ARE ONLY PUBLIC FOR DEBUG UTILITIES
    IffFile* Iff;
    IffFile* Sprites;
    OTFFile* Tuning;

    //use this tho
    std::string Name;
    std::unordered_map<std::string, VMTreeByNameTableEntry> TreeByName;

    GameObjectResource(IffFile* iff, IffFile* sprites, OTFFile* tuning, const std::string& iname, bool ts1);
    IffFile* MainIff() override { return Iff; }
    IffFile* SpritesIff() override { return Sprites; }
    OTFFile* TuningFile() override { return Tuning; }
};

/// An object in the game world.
class GameObject {
public:
    uint64_t GUID = 0;
    Chunks::OBJD* OBJ = nullptr;
    GameObjectResource* Resource = nullptr;
};

enum class GameObjectSource { Far, PIFFClone, Standalone };

class GameObjectReference {
public:
    uint64_t ID = 0;
    std::string FileName;   // entry name inside the far (without .iff) or loose path
    GameObjectSource Source = GameObjectSource::Far;
    std::string Name;
    int16_t Group = 0;
    int16_t SubIndex = 0;
    bool EpObject = false;
    int32_t FarIndex = -1;  // which archive in WorldObjectProvider::Archives (-1 = loose file)
    // TS1(sims): FreeSO TS1ObjectProvider — OBJD.Global == 1 && ObjectType != GiftToken: a "global sim
    // object" (controllers, phone plugins, help systems, plumb bobs) that every lot creates on load.
    bool GlobalSimObject = false;
    bool Person = false;    // OBJD.ObjectType == Person
};

/// Provides access to object (*.iff, *.otf) data in FAR1 archives.
class WorldObjectProvider {
public:
    explicit WorldObjectProvider(Content* contentManager) : ContentManager(contentManager) {}

    std::unordered_map<uint64_t, GameObjectReference> Entries;

    /// Initiates loading of world objects: opens the archives and builds the GUID table
    /// (from <cacheDir>/objects.cache when present, otherwise by scanning every OBJD).
    void Init(bool withSprites);

    GameObject* Get(uint32_t id, bool ts1) { return Get((uint64_t)id, ts1); }
    GameObject* Get(uint64_t id, bool ts1);
    GameObject* Get(uint32_t type, uint32_t fileID, bool ts1) { (void)type; return Get((uint64_t)fileID, ts1); }

    void AddObject(GameObject* obj);
    void AddObject(IffFile* iff, Chunks::OBJD* obj, int32_t farIndex);
    void RemoveObject(uint32_t GUID);
    void ResetFile(IffFile* iff);

    struct Archive {
        std::string Path;
        std::unique_ptr<FSO::Files::FAR1::FAR1Archive> Far;
        std::unordered_map<std::string, FSO::Files::FAR1::FarEntry> EntriesByName; // lowercase name -> entry
    };
    std::vector<Archive> Archives;
    std::vector<std::string> LooseIffPaths;
    /// TS1(sims): progress of the first-boot object scan (files done, files total).
    std::function<void(size_t, size_t)> ScanProgress;

    // TS1(sims): FreeSO AbstractObjectProvider.ControllerObjects / TS1ObjectProvider.PersonGUIDs,
    // rebuilt from Entries by Init (sorted by GUID so lots create them in a stable order).
    std::vector<uint32_t> ControllerObjects;
    std::unordered_set<uint32_t> PersonGUIDs;
    void RebuildIndexes();
    /// TS1(sims): registers every OBJD of a loose iff (character files, TemplateNPCs) as a standalone object.
    int32_t AddStandaloneIff(const std::string& path, bool overwrite);
    /// TS1(sims): frees the object files loaded for a lot (C# keeps them forever; on the 3DS that is 10-40 MB
    /// per lot visited). Call only when no VM / World is alive: GameObject / OBJD / BHAV pointers become invalid.
    /// Files holding a GUID in keepGUIDs (e.g. the controller objects every lot creates) stay loaded.
    /// Returns the number of files released.
    int ReleaseLoaded(const std::unordered_set<uint32_t>& keepGUIDs);
    size_t LoadedFileCount() const { return ProcessedFiles.size(); }
    /// TS1(sims): frees the decoded pixels of every object sprite frame (SPR2Frame::DropDecoded; they decode
    /// again on next use). Only when nothing references them: right after a zoom change (World::InvalidateZoom
    /// cleared the texture cache and invalidated the renderers). Returns the bytes freed.
    size_t DropDecodedSprites();

private:
    bool WithSprites = false;
    Content* ContentManager;
    std::unordered_map<uint64_t, std::unique_ptr<GameObject>> Cache;
    std::unordered_map<std::string, std::unique_ptr<GameObjectResource>> ProcessedFiles;
    std::vector<std::unique_ptr<IffFile>> OwnedIffs;
    std::vector<std::unique_ptr<OTFFile>> OwnedOtfs;

    bool LoadCache(const std::string& path);
public:
    std::string RelativeToInstall(const std::string& path) const; // TS1(sims): also keys avatars.cache
private:
    void SaveCache(const std::string& path);
    void ScanArchives();
    IffFile* LoadIffFromArchives(const std::string& name, int32_t preferredFar);
    OTFFile* LoadOtfFromArchives(const std::string& name);
};

/// Provides access to global (*.otf, *.iff) data (Global.far + objectdata/globals/).
class WorldGlobalProvider {
public:
    explicit WorldGlobalProvider(Content* contentManager) : ContentManager(contentManager) {}
    std::unique_ptr<FSO::Files::FAR1::FAR1Archive> GlobalFar;

    /// Creates a new cache for loading of globals.
    void Init();
    bool GetProperGlobalName(const std::string& name) { return name == "PhoneGlobals"; }

    /// Gets a resource by filename (minus extension), e.g. "global", "PhoneGlobals".
    GameGlobal* Get(const std::string& filename, bool ts1);

    // TS1(sims): FreeSO WorldGlobalProvider — TS1 autonomy curves from Global.iff STR# 501..505.
    std::vector<FSO::Common::TS1::TS1Curve> HappyWeight;
    std::vector<FSO::Common::TS1::TS1Curve> HappyWeightChild;
    std::vector<FSO::Common::TS1::TS1Curve> InteractionScore;
    std::vector<FSO::Common::TS1::TS1Curve> InteractionScoreChild;
    std::vector<FSO::Common::TS1::TS1Curve> HouseScore;
    bool CurvesLoaded = false;
    /// C# InitCurves (called from Content.Init in FreeSO). Here: called on first use (Global.iff must be loadable).
    void InitCurves();

private:
    std::unordered_map<std::string, std::unique_ptr<GameGlobal>> Cache; //indexed by lowercase filename, minus directory and extension.
    std::vector<std::unique_ptr<GameGlobalResource>> OwnedResources;
    std::vector<std::unique_ptr<IffFile>> OwnedIffs;
    std::vector<std::unique_ptr<OTFFile>> OwnedOtfs;
    std::unordered_map<std::string, FSO::Files::FAR1::FarEntry> GlobalEntries; // lowercase name -> entry
    Content* ContentManager;
};

// ---- STUB(3ds) providers below: enough surface for the VM, no real content behind them. ----

// Wall/floor catalogs: real providers (walls.iff / floors.iff / Build.iff / *.wll / *.flr), see those headers.
}}
#include "content/WorldWallProvider.h"
#include "content/WorldFloorProvider.h"
namespace FSO { namespace Content {

// STUB(3ds): FSO.Content.WorldRoofProvider.
// FSO.Content.WorldRoofProvider (FreeSO, TS1: FileProvider over GameData/Roofs/*.bmp). Ids are the indices of the
// lowercase file names in sorted order; scanned on first use (src/content/WorldRoofProvider.cpp).
class WorldRoofProvider {
public:
    std::map<std::string, int32_t> Names; // lowercase "r1_.bmp" -> id
    std::vector<std::string> Items;       // id -> lowercase file name
    std::vector<std::string> Paths;       // id -> path on disk
    /// Unknown names give 0 (the first roof) instead of FreeSO's -1.
    int32_t NameToID(const std::string& name);
    std::string IDToName(int32_t id);
    /// Path of the roof texture (BMP) for a style id ("" if there are no roofs).
    std::string GetPath(int32_t id);
    int32_t Count();
private:
    bool Scanned = false;
    void Scan();
};

// STUB(3ds): FSO.Content.AvatarAnimationProvider. Hands out placeholder Animation objects (named,
// NumFrames from a tiny default) until the Vitaboy .anim reader is ported.
class AvatarAnimationProvider {
public:
    FSO::Vitaboy::Animation* Get(const std::string& name);
private:
    std::unordered_map<std::string, std::unique_ptr<FSO::Vitaboy::Animation>> Cache;
};

// STUB(3ds): FSO.Content.AvatarAppearanceProvider / AvatarOutfitProvider / AvatarSkeletonProvider / AvatarPurchasables.
class AvatarAppearanceProvider {
public:
    FSO::Vitaboy::Appearance* Get(const std::string& name);
    FSO::Vitaboy::Appearance* Get(uint64_t id);
private:
    std::unordered_map<std::string, std::unique_ptr<FSO::Vitaboy::Appearance>> Cache;
};
class AvatarOutfitProvider {
public:
    FSO::Vitaboy::Outfit* Get(uint64_t id, bool ts1);
    FSO::Vitaboy::Outfit* Get(const std::string& name);
private:
    std::unordered_map<uint64_t, std::unique_ptr<FSO::Vitaboy::Outfit>> Cache;
    std::unordered_map<std::string, std::unique_ptr<FSO::Vitaboy::Outfit>> NameCache;
};
class AvatarSkeletonProvider {
public:
    FSO::Vitaboy::Skeleton* Get(const std::string& name);
private:
    std::unordered_map<std::string, std::unique_ptr<FSO::Vitaboy::Skeleton>> Cache;
};
class AvatarPurchasable { public: uint64_t OutfitID = 0; };
class AvatarPurchasableProvider {
public:
    AvatarPurchasable* Get(uint64_t, bool) { return nullptr; }
};

// STUB(3ds): TSO tuning.dat entries (lotmotives/simmotives). GetNum returns 0 for everything.
class TuningEntry {
public:
    std::map<std::string, float> Values;
    float GetNum(const std::string& key) const { auto it = Values.find(key); return it == Values.end() ? 0.0f : it->second; }
};
class GlobalTuningProvider {
public:
    std::map<std::string, TuningEntry> EntriesByName;
    GlobalTuningProvider() { EntriesByName["lotmotives"]; EntriesByName["simmotives"]; }
};

// STUB(3ds): FSO.Content.WorldObjectCatalog (TSO catalog xml).
class WorldObjectCatalog {
public:
    class ObjectCatalogItem { public: uint32_t GUID = 0; int32_t Category = 0; std::string Name; };
    ObjectCatalogItem* GetItemByGUID(uint32_t) { return nullptr; }
};

/// Content manager singleton (FSO.Content.Content).
class Content {
public:
    static Content* Get();
    /// Points the content manager at The Sims installation (folder containing GameData/, ExpansionPack*/ ...).
    static void Init(const std::string& simsCompleteDir, const std::string& cacheDir = "");

    std::string BasePath;          // content dir for loose files (objectdata/...)
    std::string SimsCompleteDir;   // the sims install
    std::string CacheDir;          // where objects.cache goes (defaults to SimsCompleteDir)

    std::unique_ptr<WorldObjectProvider> WorldObjects;
    std::unique_ptr<WorldGlobalProvider> WorldObjectGlobals;
    std::unique_ptr<WorldWallProvider> WorldWalls;
    std::unique_ptr<WorldFloorProvider> WorldFloors;
    std::unique_ptr<WorldRoofProvider> WorldRoofs;
    std::unique_ptr<AvatarAnimationProvider> AvatarAnimations;
    std::unique_ptr<AvatarAppearanceProvider> AvatarAppearances;
    std::unique_ptr<AvatarOutfitProvider> AvatarOutfits;
    std::unique_ptr<AvatarSkeletonProvider> AvatarSkeletons;
    std::unique_ptr<AvatarPurchasableProvider> AvatarPurchasables;
    std::unique_ptr<GlobalTuningProvider> GlobalTuning;
    std::unique_ptr<WorldObjectCatalog> WorldCatalog;

    /** TS1 **/
    // C#: `TS1NeighborhoodProvider Neighborhood;` / `TS1JobProvider Jobs;` (Content.cs). Created by Init once the
    // install path is known (C# built them in the constructor, reading the static FSOEnvironment paths).
    std::unique_ptr<TS1::TS1NeighborhoodProvider> Neighborhood;
    std::unique_ptr<TS1::TS1JobProvider> Jobs;

private:
    Content();
    static std::unique_ptr<Content> Instance;
};

// helpers shared by the providers
std::string ToLowerInvariant(const std::string& s);
bool FileExists(const std::string& path);
bool DirectoryExists(const std::string& path);
bool ReadAllBytes(const std::string& path, std::vector<uint8_t>& out);

}}
