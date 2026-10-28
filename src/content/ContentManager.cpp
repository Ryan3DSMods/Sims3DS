/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.Content — see ContentManager.h
#include "files/SafeFile.h" // FIX(sims) SAVE
#include "content/ContentManager.h"
#include "files/FilePool.h"
#include "files/LoadProgress.h"
#include "core/io.h"
#include "content/TS1NeighbourProvider.h"
#include "content/TS1JobProvider.h"
#include "vitaboy/content/TS1AvatarProvider.h" // HOOK(vitaboy)
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <stdexcept>
#include <sys/stat.h>
// Directory scanning (Downloads/ folder). Plain arm-none-eabi newlib has no <dirent.h>; devkitARM+libctru does,
// so 3DS builds enable it with -DST_HAVE_DIRENT.
#if !defined(__3DS__) || defined(ST_HAVE_DIRENT)
#include <dirent.h>
#define ST_DIRENT 1
#endif

namespace FSO { namespace Content {

using FSO::Files::FAR1::FAR1Archive;
using FSO::Files::FAR1::FarEntry;

std::unique_ptr<Content> Content::Instance;

// ---------------------------------------------------------------- helpers

std::string ToLowerInvariant(const std::string& s) {
    std::string r = s;
    for (auto& c : r) c = (char)std::tolower((unsigned char)c);
    return r;
}

bool FileExists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && !S_ISDIR(st.st_mode);
}

bool DirectoryExists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool ReadAllBytes(const std::string& path, std::vector<uint8_t>& out) {
    // TS1(sims): counted, releases pooled handles and retries when fopen fails (src/files/FilePool.h)
    std::string err;
    if (FSO::Files::ReadWholeFile(path, out, &err)) return true;
    st::LastFileError = err;
    return false;
}

static bool EndsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

static std::string StripExtension(const std::string& name) {
    size_t dot = name.rfind('.');
    return (dot == std::string::npos) ? name : name.substr(0, dot);
}

static std::string TrimNulls(const std::string& name) {
    size_t z = name.find('\0');
    return (z == std::string::npos) ? name : name.substr(0, z);
}

// ---------------------------------------------------------------- GameIffResource

void GameIffResource::Recache() {
    TuningCache.clear();
    IffFile* iff = MainIff();
    if (iff == nullptr) return;
    auto bcons = iff->List<Chunks::BCON>();
    for (auto* table : bcons) {
        uint32_t i = ((uint32_t)table->ChunkID << 16);
        for (auto item : table->Constants) {
            TuningCache[i++] = (int16_t)item;
        }
    }
}

// ---------------------------------------------------------------- GameObjectResource

GameObjectResource::GameObjectResource(IffFile* iff, IffFile* sprites, OTFFile* tuning, const std::string& iname, bool ts1)
    : Iff(iff), Sprites(sprites), Tuning(tuning), Name(iname)
{
    if (iff == nullptr) return;
    auto GLOBChunks = iff->List<Chunks::GLOB>();
    if (!GLOBChunks.empty() && GLOBChunks[0]->Name != "") {
        auto sg = Content::Get()->WorldObjectGlobals->Get(GLOBChunks[0]->Name, ts1);
        if (sg != nullptr) SemiGlobal = sg->Resource; //used for tuning constant fetching.
    }

    auto bhavs = List<Chunks::BHAV>();
    for (auto* bhav : bhavs) {
        std::string name = TrimNulls(bhav->ChunkLabel);
        if (TreeByName.find(name) == TreeByName.end()) TreeByName.emplace(name, VMTreeByNameTableEntry(bhav));
    }
    //also add semiglobals
    if (SemiGlobal != nullptr) {
        bhavs = SemiGlobal->List<Chunks::BHAV>();
        for (auto* bhav : bhavs) {
            std::string name = TrimNulls(bhav->ChunkLabel);
            if (TreeByName.find(name) == TreeByName.end()) TreeByName.emplace(name, VMTreeByNameTableEntry(bhav));
        }
    }
}

// ---------------------------------------------------------------- WorldObjectProvider

void WorldObjectProvider::Init(bool withSprites) {
    WithSprites = withSprites;
    Entries.clear();
    Cache.clear();
    Archives.clear();

    const std::string& base = ContentManager->SimsCompleteDir;
    std::vector<std::string> FarFiles;

    // TS1(sims) hardware pass: existence checks and the Downloads scan come from directory listings
    // (files/FilePool.h ListInstallDirectory): on the 3DS each stat of a directory costs ~36 ms, and the per-entry
    // stats of this scan were most of the 7.4 s "object archives + index" step of the first hardware boot.
    using FSO::Files::InstallDirExists;
    using FSO::Files::InstallFileExists;
    if (InstallDirExists(base + "/GameData/Objects")) FarFiles.push_back(base + "/GameData/Objects/Objects.far");
    if (InstallDirExists(base + "/ExpansionPack")) FarFiles.push_back(base + "/ExpansionPack/ExpansionPack.far");
    for (int i = 2; i <= 7; i++) {
        std::string ep = base + "/ExpansionPack" + std::to_string(i);
        if (InstallDirExists(ep)) FarFiles.push_back(ep + "/ExpansionPack" + std::to_string(i) + ".far");
    }
    // TS1(sims): ExpansionShared.far is shared by the expansion packs (installed from Hot Date on): plumb bobs,
    // the Hot Date social interaction objects (HDSI_*), pet/magic/fame socials, drinks/food, help systems and the
    // updated work.iff. FreeSims never loaded it (FreeSO's TS1Provider loads every .far of the install).
    if (InstallDirExists(base + "/ExpansionShared")) FarFiles.push_back(base + "/ExpansionShared/ExpansionShared.far");
    if (InstallDirExists(base + "/Deluxe")) FarFiles.push_back(base + "/Deluxe/Deluxe.far");

#ifdef ST_DIRENT
    if (InstallDirExists(base + "/Downloads")) {
        size_t firstDownload = FarFiles.size();
        for (auto& e : FSO::Files::ListInstallDirectory(base + "/Downloads", true)) { // live: new downloads
            std::string sub = base + "/Downloads/" + e.Name;
            if (e.IsDir) {
                for (auto& e2 : FSO::Files::ListInstallDirectory(sub))
                    if (!e2.IsDir && EndsWith(ToLowerInvariant(e2.Name), ".far")) FarFiles.push_back(sub + "/" + e2.Name);
            } else if (EndsWith(ToLowerInvariant(e.Name), ".far")) {
                FarFiles.push_back(sub);
            }
        }
        // TS1(sims): directory order differs between file systems (ext4/APFS/FAT on the SD card): sort, so the
        // archive indexes in objects.cache (and the caches made by tools/simcache) match on every machine
        std::sort(FarFiles.begin() + (long)firstDownload, FarFiles.end(), [](const std::string& a, const std::string& b) {
            return ToLowerInvariant(a) < ToLowerInvariant(b);
        });
    }

#endif

    for (auto& path : FarFiles) {
        if (!InstallFileExists(path)) continue;
        Archive arch;
        arch.Path = path;
        try {
            arch.Far.reset(new FAR1Archive(path, false));
            for (auto& entry : arch.Far->GetAllFarEntries()) {
                arch.EntriesByName[ToLowerInvariant(entry.Filename)] = entry;
            }
            arch.Far->DropManifest(); // TS1(sims): EntriesByName is the index (saves ~2 MB with every archive)
        } catch (...) {
            continue; //unreadable archive
        }
        Archives.push_back(std::move(arch));
    }

    std::string cachePath = ContentManager->CacheDir + "/objects.cache";
    if (!LoadCache(cachePath)) {
        ScanArchives();
        SaveCache(cachePath);
    }

    //init local objects (loose iffs under <BasePath>/Objects)
    for (auto& entry : LooseIffPaths) AddStandaloneIff(entry, true);
    RebuildIndexes();
}

// TS1(sims): see ContentManager.h
int32_t WorldObjectProvider::AddStandaloneIff(const std::string& path, bool overwrite) {
    std::unique_ptr<IffFile> iffFile;
    try { iffFile.reset(new IffFile(path)); } catch (...) { return 0; }
    std::vector<Chunks::OBJD*> objs;
    try { objs = iffFile->List<Chunks::OBJD>(); } catch (...) { return 0; }
    int32_t n = 0;
    for (auto* obj : objs) {
        if (!overwrite && Entries.find(obj->GUID) != Entries.end()) continue;
        GameObjectReference ref;
        ref.ID = obj->GUID;
        ref.FileName = path;
        ref.Source = GameObjectSource::Standalone;
        ref.Name = TrimNulls(obj->ChunkLabel);
        ref.Group = (int16_t)obj->MasterID;
        ref.SubIndex = obj->SubIndex;
        ref.Person = obj->ObjectType == Chunks::OBJDType::Person;
        Entries[obj->GUID] = ref;
        if (ref.Person) PersonGUIDs.insert(obj->GUID);
        n++;
    }
    return n;
}

// TS1(sims): see ContentManager.h
size_t WorldObjectProvider::DropDecodedSprites() {
    size_t bytes = 0;
    for (auto& f : OwnedIffs) {
        if (!f) continue;
        for (auto* c : f->SilentListAll()) {
            if (c->ChunkType != "SPR2" || !c->ChunkProcessed) continue;
            for (auto& fr : static_cast<Chunks::SPR2*>(c)->Frames) if (fr) bytes += fr->DropDecoded();
        }
    }
    if (getenv("SIM3DS_SPRDEBUG")) fprintf(stderr, "[sprites] %.1f MB of decoded object frames freed\n", bytes / 1048576.0);
    return bytes;
}

int WorldObjectProvider::ReleaseLoaded(const std::unordered_set<uint32_t>& keepGUIDs) {
    std::unordered_set<GameObjectResource*> keepRes;
    for (auto& kv : Cache) if (keepGUIDs.count((uint32_t)kv.first) && kv.second->Resource) keepRes.insert(kv.second->Resource);
    std::unordered_set<const void*> keepFiles;
    for (auto* r : keepRes) { keepFiles.insert(r->Iff); keepFiles.insert(r->Sprites); keepFiles.insert(r->Tuning); }
    for (auto it = Cache.begin(); it != Cache.end();) {
        if (keepRes.count(it->second->Resource)) ++it; else it = Cache.erase(it);
    }
    int released = 0;
    for (auto it = ProcessedFiles.begin(); it != ProcessedFiles.end();) {
        if (keepRes.count(it->second.get())) ++it; else { it = ProcessedFiles.erase(it); released++; }
    }
    OwnedIffs.erase(std::remove_if(OwnedIffs.begin(), OwnedIffs.end(), [&](const std::unique_ptr<IffFile>& f) { return !keepFiles.count(f.get()); }), OwnedIffs.end());
    OwnedOtfs.erase(std::remove_if(OwnedOtfs.begin(), OwnedOtfs.end(), [&](const std::unique_ptr<OTFFile>& f) { return !keepFiles.count(f.get()); }), OwnedOtfs.end());
    OwnedIffs.shrink_to_fit();
    OwnedOtfs.shrink_to_fit();
    return released;
}

// TS1(sims): FreeSO TS1ObjectProvider.Init — "var globalSims = Entries.Values.Where(x => x.GlobalSimObject);
// ControllerObjects.Clear(); ControllerObjects.AddRange(globalSims);" and PersonGUIDs.
void WorldObjectProvider::RebuildIndexes() {
    ControllerObjects.clear();
    PersonGUIDs.clear();
    for (auto& kv : Entries) {
        if (kv.second.GlobalSimObject) ControllerObjects.push_back((uint32_t)kv.first);
        if (kv.second.Person) PersonGUIDs.insert((uint32_t)kv.first);
    }
    std::sort(ControllerObjects.begin(), ControllerObjects.end());
}

/// Scans every .iff in every archive for OBJD chunks. Slow, so the result is cached on disk.
/// TS1(sims): reads only each file's resource map and its OBJD chunks (IffFile::ReadFromFile "OBJD": ~20 KB
/// instead of the whole file; ~680 MB -> ~25 MB with the Complete Collection), in archive order so the SD card reads
/// forward. ScanProgress (if set) gets (files done, files total) for the loading screen.
void WorldObjectProvider::ScanArchives() {
    size_t total = 0, done = 0;
    for (auto& arch : Archives) for (auto& kv : arch.EntriesByName) if (EndsWith(kv.first, ".iff")) total++;
    for (size_t fi = 0; fi < Archives.size(); fi++) {
        auto& arch = Archives[fi];
        std::vector<const FarEntry*> files;
        for (auto& kv : arch.EntriesByName) if (EndsWith(kv.first, ".iff")) files.push_back(&kv.second);
        std::sort(files.begin(), files.end(), [](const FarEntry* a, const FarEntry* b) { return a->DataOffset < b->DataOffset; });
        for (auto* fe : files) {
            done++;
            if (ScanProgress && (done % 16 == 0 || done == total)) ScanProgress(done, total);
            IffFile iff;
            try { iff.ReadFromFile(arch.Path, fe->DataOffset, (uint32_t)fe->DataLength, true, "OBJD"); } catch (...) { continue; }
            std::vector<Chunks::OBJD*> objs;
            try { objs = iff.List<Chunks::OBJD>(); }
            catch (std::exception& e) {
                fprintf(stderr, "[content] OBJD parse failed in %s: %s (file skipped)\n", fe->Filename.c_str(), e.what());
                continue;
            }
            for (auto* obj : objs) {
                if (Entries.find(obj->GUID) != Entries.end()) continue; //first archive wins (base game before expansions)
                GameObjectReference ref;
                ref.ID = obj->GUID;
                ref.FileName = StripExtension(fe->Filename);
                ref.Source = GameObjectSource::Far;
                ref.Name = TrimNulls(obj->ChunkLabel);
                ref.Group = (int16_t)obj->MasterID;
                ref.SubIndex = obj->SubIndex;
                ref.EpObject = (fi != 0);
                ref.FarIndex = (int32_t)fi;
                // TS1(sims): FreeSO TS1ObjectProvider.Init
                ref.GlobalSimObject = obj->Global == 1 && obj->ObjectType != Chunks::OBJDType::GiftToken;
                ref.Person = obj->ObjectType == Chunks::OBJDType::Person;
                Entries[obj->GUID] = ref;
            }
        }
    }
}

// TS1(sims): "<install>/ExpansionPack/ExpansionPack.far" -> "ExpansionPack/ExpansionPack.far"
std::string WorldObjectProvider::RelativeToInstall(const std::string& path) const {
    const std::string& base = ContentManager->SimsCompleteDir;
    if (!base.empty() && path.size() > base.size() + 1 && path.compare(0, base.size(), base) == 0 && (path[base.size()] == '/' || path[base.size()] == '\\'))
        return path.substr(base.size() + 1);
    return path;
}

bool WorldObjectProvider::LoadCache(const std::string& path) {
    std::vector<uint8_t> bytes;
    if (!ReadAllBytes(path, bytes)) return false;
    std::string text(bytes.begin(), bytes.end());
    size_t pos = 0;
    bool headerOk = false;
    while (pos < text.size()) {
        size_t nl = text.find('\n', pos);
        if (nl == std::string::npos) nl = text.size();
        std::string line = text.substr(pos, nl - pos);
        pos = nl + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        if (!headerOk) {
            //header: "STOC3 <archive count>" and the archive paths must match the ones we opened.
            // TS1(sims): STOC2 adds a flags column (1 = global sim object, 2 = person); STOC1 caches are rebuilt.
            // STOC3: archive paths relative to the install (the same cache works on the host and on the SD card,
            // see src/tools/simcache.cpp).
            if (line.rfind("STOC3 ", 0) != 0) return false;
            size_t count = (size_t)atoi(line.c_str() + 6);
            if (count != Archives.size()) return false;
            for (size_t i = 0; i < count; i++) {
                size_t nl2 = text.find('\n', pos);
                if (nl2 == std::string::npos) return false;
                std::string ap = text.substr(pos, nl2 - pos);
                pos = nl2 + 1;
                if (!ap.empty() && ap.back() == '\r') ap.pop_back();
                if (ap != RelativeToInstall(Archives[i].Path)) return false;
            }
            headerOk = true;
            continue;
        }
        //guid \t far \t filename \t group \t subindex \t name
        std::vector<std::string> cols;
        size_t s = 0;
        while (true) {
            size_t t = line.find('\t', s);
            cols.push_back(line.substr(s, (t == std::string::npos) ? std::string::npos : t - s));
            if (t == std::string::npos) break;
            s = t + 1;
        }
        if (cols.size() < 7) continue;
        GameObjectReference ref;
        ref.ID = (uint64_t)strtoull(cols[0].c_str(), nullptr, 16);
        ref.FarIndex = atoi(cols[1].c_str());
        ref.FileName = cols[2];
        ref.Group = (int16_t)atoi(cols[3].c_str());
        ref.SubIndex = (int16_t)atoi(cols[4].c_str());
        int flags = atoi(cols[5].c_str());
        ref.GlobalSimObject = (flags & 1) != 0;
        ref.Person = (flags & 2) != 0;
        ref.Name = cols[6];
        for (size_t c = 7; c < cols.size(); c++) ref.Name += "\t" + cols[c];
        ref.Source = GameObjectSource::Far;
        ref.EpObject = ref.FarIndex != 0;
        Entries[ref.ID] = ref;
    }
    return headerOk && !Entries.empty();
}

void WorldObjectProvider::SaveCache(const std::string& path) {
    FILE* f = FSO::Files::SafeFile::BeginWrite(path); // FIX(sims) SAVE: a cut objects.cache = missing objects
    if (!f) return;
    fprintf(f, "STOC3 %u\n", (unsigned)Archives.size());
    for (auto& a : Archives) fprintf(f, "%s\n", RelativeToInstall(a.Path).c_str());
    for (auto& kv : Entries) {
        auto& r = kv.second;
        if (r.Source != GameObjectSource::Far) continue;
        int flags = (r.GlobalSimObject ? 1 : 0) | (r.Person ? 2 : 0);
        fprintf(f, "%llX\t%d\t%s\t%d\t%d\t%d\t%s\n", (unsigned long long)r.ID, (int)r.FarIndex, r.FileName.c_str(), (int)r.Group, (int)r.SubIndex, flags, r.Name.c_str());
    }
    FSO::Files::SafeFile::EndWrite(path, f);
}

IffFile* WorldObjectProvider::LoadIffFromArchives(const std::string& name, int32_t preferredFar) {
    std::string lname = ToLowerInvariant(name);
    auto tryArchive = [&](Archive& arch) -> IffFile* {
        auto it = arch.EntriesByName.find(lname);
        if (it == arch.EntriesByName.end()) return nullptr;
        // TS1(sims): only the chunks the VM needs are read now; sprites/bitmaps load on first use (IffFile::ReadFromFile)
        std::unique_ptr<IffFile> iff(new IffFile());
        iff->ReadFromFile(arch.Path, it->second.DataOffset, (uint32_t)it->second.DataLength, IffFile::DEFER_HEAVY_CHUNKS);
        iff->Filename = name;
        IffFile* result = iff.get();
        OwnedIffs.push_back(std::move(iff));
        return result;
    };
    if (preferredFar >= 0 && preferredFar < (int32_t)Archives.size()) {
        IffFile* r = tryArchive(Archives[preferredFar]);
        if (r != nullptr) return r;
    }
    for (auto& arch : Archives) {
        IffFile* r = tryArchive(arch);
        if (r != nullptr) return r;
    }
    return nullptr;
}

OTFFile* WorldObjectProvider::LoadOtfFromArchives(const std::string& name) {
    std::string lname = ToLowerInvariant(name);
    for (auto& arch : Archives) {
        auto it = arch.EntriesByName.find(lname);
        if (it == arch.EntriesByName.end()) continue;
        try {
            std::vector<uint8_t> bytes = arch.Far->GetEntry(it->second);
            std::unique_ptr<OTFFile> otf(new OTFFile());
            otf->Read(bytes);
            OTFFile* result = otf.get();
            OwnedOtfs.push_back(std::move(otf));
            return result;
        } catch (...) {
            //if any issues occur loading an otf, just silently ignore it.
            return nullptr;
        }
    }
    return nullptr;
}

GameObject* WorldObjectProvider::Get(uint64_t id, bool ts1) {
    auto cached = Cache.find(id);
    if (cached != Cache.end()) return cached->second.get();

    auto refIt = Entries.find(id);
    if (refIt == Entries.end()) {
        //Failed to get Object ID (no resource)
        return nullptr;
    }
    GameObjectReference& reference = refIt->second;
    GameObjectResource* resource = nullptr;

    //if a file is processed but an object in it is not in the cache, it may have changed. check for it again!
    auto pf = ProcessedFiles.find(reference.FileName);
    if (pf != ProcessedFiles.end()) resource = pf->second.get();

    if (resource == nullptr) {
        /** Better set this up! **/
        IffFile* sprites = nullptr;
        IffFile* iff = nullptr;
        OTFFile* tuning = nullptr;

        if (reference.Source == GameObjectSource::Far) {
            iff = LoadIffFromArchives(reference.FileName + ".iff", reference.FarIndex);
            if (iff == nullptr) return nullptr;
            iff->RuntimeInfo.Path = reference.FileName;
            if (WithSprites) {
                if (reference.EpObject) sprites = LoadIffFromArchives(reference.FileName + ".iff", reference.FarIndex);
                else sprites = LoadIffFromArchives(reference.FileName + ".spf", reference.FarIndex);
            }
            tuning = LoadOtfFromArchives(reference.FileName + ".otf");
        } else {
            std::unique_ptr<IffFile> loose;
            // FIX(sims) memory: read-only with deferred chunks (character files: the strings of all 20 languages
            // and the portraits stayed in memory, ~6 MB for a neighbourhood); see IffFile::DetachSource for rewrites
            try { loose = IffFile::OpenReadOnly(reference.FileName); } catch (...) { return nullptr; }
            iff = loose.get();
            OwnedIffs.push_back(std::move(loose));
            iff->RuntimeInfo.Path = reference.FileName;
            iff->RuntimeInfo.State = FSO::Files::Formats::IFF::IffRuntimeState::Standalone;
        }

        iff->RuntimeInfo.UseCase = FSO::Files::Formats::IFF::IffUseCase::Object;
        if (sprites != nullptr) sprites->RuntimeInfo.UseCase = FSO::Files::Formats::IFF::IffUseCase::ObjectSprites;

        std::unique_ptr<GameObjectResource> res(new GameObjectResource(iff, sprites, tuning, reference.FileName, ts1));
        resource = res.get();
        ProcessedFiles[reference.FileName] = std::move(res);
        FSO::Files::LoadProgress::Progress("Loading objects", ProcessedFiles.size(), 0); // TS1(sims)
    }

    for (auto* objd : resource->MainIff()->List<Chunks::OBJD>()) {
        if (Cache.find(objd->GUID) != Cache.end()) continue;
        std::unique_ptr<GameObject> item(new GameObject());
        item->GUID = objd->GUID;
        item->OBJ = objd;
        item->Resource = resource;
        Cache[item->GUID] = std::move(item);
    }
    //0x3BAA9787
    cached = Cache.find(id);
    if (cached == Cache.end()) {
        //Failed to get Object ID from resource
        return nullptr;
    }
    return cached->second.get();
}

void WorldObjectProvider::AddObject(GameObject* obj) {
    AddObject(obj->Resource->MainIff(), obj->OBJ, -1);
}

void WorldObjectProvider::AddObject(IffFile* iff, Chunks::OBJD* obj, int32_t farIndex) {
    GameObjectSource source;
    switch (iff->RuntimeInfo.State) {
        case FSO::Files::Formats::IFF::IffRuntimeState::PIFFClone: source = GameObjectSource::PIFFClone; break;
        case FSO::Files::Formats::IFF::IffRuntimeState::Standalone: source = GameObjectSource::Standalone; break;
        default: source = GameObjectSource::Far; break;
    }
    GameObjectReference ref;
    ref.ID = obj->GUID;
    ref.FileName = iff->RuntimeInfo.Path;
    ref.Source = source;
    ref.Name = obj->ChunkLabel;
    ref.Group = (int16_t)obj->MasterID;
    ref.SubIndex = obj->SubIndex;
    ref.FarIndex = farIndex;
    Entries[obj->GUID] = ref;
}

void WorldObjectProvider::RemoveObject(uint32_t GUID) {
    Entries.erase(GUID);
    Cache.erase(GUID);
}

void WorldObjectProvider::ResetFile(IffFile* iff) {
    std::vector<uint64_t> ToRemove;
    for (auto& objt : Entries) {
        if (objt.second.FileName == iff->RuntimeInfo.Path) ToRemove.push_back(objt.first);
    }
    for (auto guid : ToRemove) Entries.erase(guid);

    //add all OBJDs
    auto list = iff->List<Chunks::OBJD>();
    for (auto* obj : list) AddObject(iff, obj, -1);
}

// ---------------------------------------------------------------- WorldGlobalProvider

void WorldGlobalProvider::Init() {
    Cache.clear();
    GlobalEntries.clear();
    const std::string& base = ContentManager->SimsCompleteDir;
    std::string farPath = base + "/GameData/Global/Global.far";
    if (DirectoryExists(base + "/GameData/Global") && FileExists(farPath)) {
        try {
            GlobalFar.reset(new FAR1Archive(farPath, false));
            for (auto& entry : GlobalFar->GetAllFarEntries()) {
                GlobalEntries[ToLowerInvariant(entry.Filename)] = entry;
            }
        } catch (...) {
            GlobalFar.reset();
        }
    }
}

// TS1(sims): FreeSO WorldGlobalProvider.InitCurves / CurveFromSTR.
void WorldGlobalProvider::InitCurves() {
    if (CurvesLoaded) return;
    CurvesLoaded = true;
    auto global = Get("global", true);
    if (global == nullptr || global->Resource == nullptr) return;
    auto curveFromSTR = [&](uint16_t id, bool cache) {
        std::vector<FSO::Common::TS1::TS1Curve> result;
        auto str = global->Resource->Get<FSO::Files::Formats::IFF::Chunks::STR>(id);
        if (str == nullptr) return result;
        auto length = str->Length();
        for (int32_t i = 0; i < length; i++) {
            result.emplace_back(str->GetString(i));
            if (cache) result.back().BuildCache(-100, 100);
        }
        return result;
    };
    InteractionScore = curveFromSTR(501, true);
    HappyWeight = curveFromSTR(502, true);
    InteractionScoreChild = curveFromSTR(503, true);
    HappyWeightChild = curveFromSTR(504, true);
    HouseScore = curveFromSTR(505, false);
}

GameGlobal* WorldGlobalProvider::Get(const std::string& filenameIn, bool ts1) {
    std::string filename = ToLowerInvariant(filenameIn);
    auto cached = Cache.find(filename);
    if (cached != Cache.end()) return cached->second.get();

    IffFile* iff = nullptr;

    if (!ts1) {
        std::string filepath = ContentManager->BasePath + "/objectdata/globals/" + filename + ".iff";
        //if we can't load this let it throw an exception...
        //probably sanity check this when we add user objects.
        if (FileExists(filepath)) {
            std::unique_ptr<IffFile> loose(new IffFile(filepath));
            iff = loose.get();
            OwnedIffs.push_back(std::move(loose));
        }
    }

    if (GlobalFar && iff == nullptr) {
        auto it = GlobalEntries.find(filename + ".iff");
        if (it != GlobalEntries.end()) {
            std::unique_ptr<IffFile> Giff(new IffFile());
            Giff->ReadFromFile(GlobalFar->Path(), it->second.DataOffset, (uint32_t)it->second.DataLength, IffFile::DEFER_HEAVY_CHUNKS); // TS1(sims)
            Giff->Filename = filename + ".iff";
            iff = Giff.get();
            OwnedIffs.push_back(std::move(Giff));
        }
    }

    OTFFile* otf = nullptr;
    {
        std::string otfPath = ContentManager->BasePath + "/objectdata/globals/" + filename + ".otf";
        if (FileExists(otfPath)) {
            try {
                std::unique_ptr<OTFFile> o(new OTFFile(otfPath));
                otf = o.get();
                OwnedOtfs.push_back(std::move(o));
            } catch (...) {
                //if we can't load an otf, it probably doesn't exist.
            }
        } else if (GlobalFar) {
            auto it = GlobalEntries.find(filename + ".otf");
            if (it != GlobalEntries.end()) {
                try {
                    std::vector<uint8_t> bytes = GlobalFar->GetEntry(it->second);
                    std::unique_ptr<OTFFile> o(new OTFFile());
                    o->Read(bytes);
                    otf = o.get();
                    OwnedOtfs.push_back(std::move(o));
                } catch (...) {}
            }
        }
    }

    if (iff == nullptr && otf == nullptr) return nullptr;

    std::unique_ptr<GameGlobalResource> resource(new GameGlobalResource(iff, otf));
    std::unique_ptr<GameGlobal> item(new GameGlobal());
    item->Resource = resource.get();
    OwnedResources.push_back(std::move(resource));
    GameGlobal* result = item.get();
    Cache[filename] = std::move(item);
    return result;
}

// ---------------------------------------------------------------- STUB providers

// HOOK(vitaboy): the avatar providers below delegate to the TS1 Vitaboy content
// (src/vitaboy/content/TS1AvatarProvider: Animation.far BCF/CFP/BMF via Animation.ndx, Skins/*.cmx/.skn/.bmp).
// When no TS1 avatar data is found (e.g. headless tests without GameData) the old placeholders are used.
static FSO::Content::TS1::TS1AvatarContent* VitaboyContent() {
    auto c = Content::Get();
    if (c->SimsCompleteDir.empty()) return nullptr;
    FSO::Content::TS1::TS1AvatarContent::SetBasePath(c->SimsCompleteDir);
    auto vb = FSO::Content::TS1::TS1AvatarContent::Get();
    return vb->EnsureInit() ? vb : nullptr;
}

FSO::Vitaboy::Animation* AvatarAnimationProvider::Get(const std::string& name) {
    if (auto vb = VitaboyContent()) return vb->BCF.GetAnimation(name); // HOOK(vitaboy): real TS1 skills (null when missing, as in C#)
    // STUB(3ds): fallback placeholder when no TS1 animation data is installed.
    std::string key = ToLowerInvariant(name);
    auto it = Cache.find(key);
    if (it != Cache.end()) return it->second.get();
    std::unique_ptr<FSO::Vitaboy::Animation> anim(new FSO::Vitaboy::Animation());
    anim->Name = StripExtension(name);
    FSO::Vitaboy::AnimationMotion motion;
    motion.FrameCount = 30; //placeholder length: one second at the VM tick rate
    anim->Motions.push_back(motion);
    auto* result = anim.get();
    Cache[key] = std::move(anim);
    return result;
}

FSO::Vitaboy::Appearance* AvatarAppearanceProvider::Get(const std::string& name) {
    if (auto vb = VitaboyContent()) return vb->BCF.GetAppearance(name); // HOOK(vitaboy): TS1 suits by name ("x.apr")
    std::string key = ToLowerInvariant(name);
    auto it = Cache.find(key);
    if (it != Cache.end()) return it->second.get();
    std::unique_ptr<FSO::Vitaboy::Appearance> apr(new FSO::Vitaboy::Appearance());
    apr->Name = name;
    auto* result = apr.get();
    Cache[key] = std::move(apr);
    return result;
}
FSO::Vitaboy::Appearance* AvatarAppearanceProvider::Get(uint64_t id) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%llX", (unsigned long long)id);
    return Get(std::string(buf));
}

FSO::Vitaboy::Outfit* AvatarOutfitProvider::Get(uint64_t id, bool) {
    // HOOK(vitaboy): TS1 outfits registered from body strings (TS1AvatarContent::RegisterOutfitFromBodyStrings)
    if (FSO::Content::TS1::TS1AvatarContent::IsTS1OutfitID(id)) {
        VitaboyContent(); // make sure the avatar content knows the install path before the outfit is bound
        if (auto oft = FSO::Content::TS1::TS1AvatarContent::Get()->GetOutfit(id)) return oft;
    }
    auto it = Cache.find(id);
    if (it != Cache.end()) return it->second.get();
    std::unique_ptr<FSO::Vitaboy::Outfit> oft(new FSO::Vitaboy::Outfit());
    oft->ID = id;
    auto* result = oft.get();
    Cache[id] = std::move(oft);
    return result;
}
FSO::Vitaboy::Outfit* AvatarOutfitProvider::Get(const std::string& name) {
    auto it = NameCache.find(name);
    if (it != NameCache.end()) return it->second.get();
    std::unique_ptr<FSO::Vitaboy::Outfit> oft(new FSO::Vitaboy::Outfit());
    oft->Name = name;
    auto* result = oft.get();
    NameCache[name] = std::move(oft);
    return result;
}

FSO::Vitaboy::Skeleton* AvatarSkeletonProvider::Get(const std::string& name) {
    if (auto vb = VitaboyContent()) { // HOOK(vitaboy): "adult.skel" / "child.skel" from Animation.far
        if (auto skel = vb->BCF.GetSkeleton(name)) return skel;
    }
    auto it = Cache.find(name);
    if (it != Cache.end()) return it->second.get();
    std::unique_ptr<FSO::Vitaboy::Skeleton> skel(new FSO::Vitaboy::Skeleton());
    skel->Name = name;
    auto* result = skel.get();
    Cache[name] = std::move(skel);
    return result;
}

// ---------------------------------------------------------------- Content

Content::Content() {
    WorldObjects.reset(new WorldObjectProvider(this));
    WorldObjectGlobals.reset(new WorldGlobalProvider(this));
    WorldWalls.reset(new WorldWallProvider(this));
    WorldFloors.reset(new WorldFloorProvider(this));
    WorldRoofs.reset(new WorldRoofProvider());
    AvatarAnimations.reset(new AvatarAnimationProvider());
    AvatarAppearances.reset(new AvatarAppearanceProvider());
    AvatarOutfits.reset(new AvatarOutfitProvider());
    AvatarSkeletons.reset(new AvatarSkeletonProvider());
    AvatarPurchasables.reset(new AvatarPurchasableProvider());
    GlobalTuning.reset(new GlobalTuningProvider());
    WorldCatalog.reset(new WorldObjectCatalog());
}

Content* Content::Get() {
    if (!Instance) Instance.reset(new Content());
    return Instance.get();
}

void Content::Init(const std::string& simsCompleteDir, const std::string& cacheDir) {
    using FSO::Files::LoadProgress; // TS1(sims): step log + loading-screen progress
    Content* c = Get();
    c->SimsCompleteDir = simsCompleteDir;
    c->BasePath = simsCompleteDir;
    c->CacheDir = cacheDir.empty() ? simsCompleteDir : cacheDir;
    FSO::Content::TS1::TS1AvatarContent::CacheDir = c->CacheDir; // TS1(sims): cmxnames.cache
    FSO::Files::InstallDirCache::File = c->CacheDir + "/dirs.cache"; // TS1(sims) hardware pass: directory listings
    FSO::Files::InstallDirCache::Root = simsCompleteDir;
    LoadProgress::Step("content: Global.far");
    c->WorldObjectGlobals->Init();
    LoadProgress::Step("content: object archives + index");
    c->WorldObjects->ScanProgress = [](size_t done, size_t total) { LoadProgress::Progress("Indexing objects (first run only)", done, total); };
    c->WorldObjects->Init(false);
    c->WorldObjects->ScanProgress = nullptr;
    {   // TS1(sims) hardware pass: the archive list keys <cache>/avatars.cache (the avatar file index, TS1AvatarProvider)
        std::string sig;
        for (auto& a : c->WorldObjects->Archives) sig += c->WorldObjects->RelativeToInstall(a.Path) + "|";
        FSO::Content::TS1::TS1AvatarContent::InstallSignature = sig;
    }
    LoadProgress::Step("content: walls");
    c->WorldWalls->Init();   // walls.iff + Build.iff + Walls/*.wll (VMWorldActivator.BuildWallDict needs DynamicWallFromID)
    LoadProgress::Step("content: floors");
    c->WorldFloors->Init();  // floors.iff + Build.iff + Floors/*.flr
    // TS1(sims): neighbourhood + careers. LoadCharacters registers UserData/Characters/User*.iff with
    // WorldObjects (FreeSims: "todo: manage avatar iffs here").
    LoadProgress::Step("content: characters");
    c->Neighborhood.reset(new TS1::TS1NeighborhoodProvider(c));
    c->Neighborhood->LoadCharacters();
    c->WorldObjects->RebuildIndexes(); // TS1(sims): characters/templates are persons too
    LoadProgress::Step("content: careers");
    c->Jobs.reset(new TS1::TS1JobProvider(c));
    c->Jobs->Init();
    FSO::Files::InstallDirCache::Save();
    LoadProgress::Step("content: ready");
}

}}
