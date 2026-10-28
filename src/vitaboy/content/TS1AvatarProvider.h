/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// TS1 avatar content providers.
// Translated from SimsVille/ContentManager/other/TS1Provider.cs, TS1BCFProvider.cs, TS1BMFProvider.cs,
// TS1AvatarTextureProvider.cs (FreeSims) with these 3DS-port changes (see STUBS.md):
//  * TS1BCFProvider uses the game's own index GameData/Animation/Animation.ndx (inside Animation.far)
//    instead of opening and parsing all ~1200 BCF files at startup to learn which file holds which
//    skeleton/suit/skill. Loose text .cmx files in Skins folders are still scanned (they are small).
//  * Everything is lazy: BCF files are parsed on first request, CFP keyframes are decoded only when
//    an animation is first posed (Animation::EnsureKeyframes), meshes/textures on first bind.
//  * Text formats (.cmx/.skn) are read alongside the binary ones (.bcf/.bmf).
//  * Outfits: TS1 has no outfit files; outfits built from body strings are registered here under a
//    synthetic 64-bit id so the VM can keep using uint64 BodyOutfit/HeadOutfit values.
#pragma once
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "vitaboy/BCF.h"
#include "vitaboy/Mesh.h"
#include "vitaboy/Texture.h"
#include "vitaboy/Outfit.h"
#include "vitaboy/CFP.h"

namespace FSO { namespace Files { namespace FAR1 { class FAR1Archive; } } }

namespace FSO { namespace Content { namespace TS1 {

/// FSO.Content.Framework.TS1Provider: every FAR1 archive of the install plus the bare Skins folders,
/// indexed by lower-case file name.
class TS1Provider {
public:
    TS1Provider();
    ~TS1Provider();
    /// baseDir = folder holding GameData/ (and ExpansionPack*/, ExpansionShared/).
    void Init(const std::string& baseDir);
    bool Has(const std::string& lowerName) const;
    /// Reads a file by (case-insensitive) name. Returns false when missing.
    bool GetBytes(const std::string& name, std::vector<uint8_t>& out);
    /// Lower-case names of all files with this extension (e.g. ".cmx").
    std::vector<std::string> ListExtension(const std::string& ext) const;
    std::vector<std::string> ListNdx() const; // TS1(sims): all AnimationOut.ndx entries (one per EP folder)
    /// true when the file comes from a bare folder (not a FAR).
    bool IsLoose(const std::string& lowerName) const;
    /// TS1(sims) hardware pass: the avatars.cache key of this index ("" = no cache); also keys bcfnames.cache.
    std::string IndexKey;

private:
    // FIX(sims) memory: Far < 0 = loose file, Offset = index into LoosePaths (was a std::string per entry)
    struct Entry { int32_t Far = -1; uint32_t Offset = 0, Size = 0; };
    std::vector<std::string> FarPaths;
    std::vector<std::string> LoosePaths;
    std::vector<std::string> ProbeFolders; // no-dirent builds: loose folders probed by file name
    std::unordered_map<std::string, Entry> Entries; // while Init runs; then frozen (see Freeze)
    // FIX(sims) memory: after Init the ~22k names live in one '\0'-separated pool with a sorted slot table
    // (binary search): ~1 MB instead of ~2.5 MB of hash nodes + key strings for the whole session.
    struct Slot { uint32_t Name; Entry E; };
    std::string NamePool;
    std::vector<Slot> Slots;
    void Freeze();
    const Entry* Find(const std::string& lowerName) const;
    void AddFar(const std::string& path);
    void AddFolder(const std::string& path);
    // TS1(sims) hardware pass: <cache>/avatars.cache holds the frozen index (TS1AvatarContent::InstallSignature)
    bool LoadIndexCache(const std::string& file, const std::string& baseDir, const std::string& key);
    void SaveIndexCache(const std::string& file, const std::string& baseDir, const std::string& key) const;
};

/// FSO.Content.TS1.TS1BCFProvider: skeletons, suits (appearances) and skills (animations) by name.
class TS1BCFProvider {
public:
    explicit TS1BCFProvider(TS1Provider* provider) : BaseProvider(provider) {}
    void Init();

    /// name with extension: "a2o-standing-loop.anim", "b001mafit_01.apr", "adult.skel" (case-insensitive)
    FSO::Vitaboy::Animation* GetAnimation(const std::string& name);
    FSO::Vitaboy::Appearance* GetAppearance(const std::string& name);
    FSO::Vitaboy::Skeleton* GetSkeleton(const std::string& name);

    /// CFP keyframe loading for Animation::EnsureKeyframes.
    bool EnrichAnimation(FSO::Vitaboy::Animation* anim);

    // name (lower, no extension) -> host file (lower, e.g. "ross-walking-loop.cmx")
    std::unordered_map<std::string, std::string> AnimHostBCF;
    std::unordered_map<std::string, std::string> SkinHostBCF;
    std::unordered_map<std::string, std::string> SkelHostBCF;

    size_t LoadedFiles() const { return BCFCache.size(); }
    /// FIX(sims) memory: frees the parsed BCF files (animation headers, suits, skeletons) not used since the last
    /// call (the previous lot's working set stays: re-reading every host file on each lot cost ~1 s of SD time per
    /// load); see ReleaseCaches.
    size_t TrimCache();

private:
    TS1Provider* BaseProvider;
    struct CachedBCF { std::unique_ptr<FSO::Vitaboy::BCF> B; uint32_t Gen = 0; }; // FIX(sims): see TrimCache
    std::unordered_map<std::string, CachedBCF> BCFCache; // host file -> parsed
    uint32_t CacheGen = 1;
    FSO::Vitaboy::BCF* GetBCF(const std::string& host);
    void ReadIndex(const std::vector<uint8_t>& ndx);
};

/// FSO.Content.TS1.TS1BMFProvider (+ text .skn): meshes by name ("xskin-...-PELVIS-BODY").
class TS1BMFProvider {
public:
    explicit TS1BMFProvider(TS1Provider* provider) : BaseProvider(provider) {}
    FSO::Vitaboy::Mesh* Get(const std::string& name);
    void Clear() { Cache.clear(); } // FIX(sims): see TS1AvatarContent::ReleaseCaches
private:
    TS1Provider* BaseProvider;
    std::unordered_map<std::string, std::unique_ptr<FSO::Vitaboy::Mesh>> Cache;
};

/// FSO.Content.TS1.TS1AvatarTextureProvider: .bmp (then .tga, unsupported) by name.
class TS1AvatarTextureProvider {
public:
    explicit TS1AvatarTextureProvider(TS1Provider* provider) : BaseProvider(provider) {}
    FSO::Vitaboy::Texture* Get(const std::string& name);
    size_t MemoryBytes() const;
    void Clear() { Cache.clear(); } // FIX(sims): see TS1AvatarContent::ReleaseCaches
private:
    TS1Provider* BaseProvider;
    std::unordered_map<std::string, std::unique_ptr<FSO::Vitaboy::Texture>> Cache;
};

/// The avatar half of FSO.Content.Content for TS1 data (singleton).
class TS1AvatarContent {
public:
    static TS1AvatarContent* Get();

    /// Sets the install folder; Init happens lazily on first use (EnsureInit).
    static void SetBasePath(const std::string& baseDir);
    /// TS1(sims): where the loose-CMX name index is cached (cmxnames.cache); empty = no cache.
    static std::string CacheDir;
    /// TS1(sims) hardware pass: the install's archive list (set by Content::Init); keys <CacheDir>/avatars.cache, the
    /// avatar file index (22k names), so the first Sim of a boot (Create-A-Sim, a lot) does not walk the install tree
    /// (~360 directory operations, ~20 s on the 3DS SD card) and open ~80 archives. Empty = no cache.
    static std::string InstallSignature;
    /// Returns true when the TS1 avatar data (Animation.far / Skins) was found.
    bool EnsureInit();
    bool Initialized() const { return m_Initialized && m_HaveData; }

    TS1Provider TS1Global;
    TS1BCFProvider BCF;
    TS1BMFProvider AvatarMeshes;
    TS1AvatarTextureProvider AvatarTextures;

    // ---- outfits (TS1: built from body strings) ----
    /// Registers (or finds) the TS1 outfit with these strings and returns its synthetic id.
    uint64_t RegisterOutfit(const std::string& aprName, const std::string& texName, bool withHands,
                            const std::string handStrings[6]);
    /// Outfit for an id from RegisterOutfit, or nullptr.
    FSO::Vitaboy::Outfit* GetOutfit(uint64_t id);
    /// Builds+registers the body (head=false) or head outfit from a person's body strings (STR#200).
    uint64_t RegisterOutfitFromBodyStrings(FSO::Files::Formats::IFF::Chunks::STR* bodyStrings, bool head);
    static bool IsTS1OutfitID(uint64_t id) { return (id >> 48) == 0x7531; }

    /// FIX(sims) memory: frees the avatar meshes, textures, parsed BCF files (animations, suits, skeletons) and the
    /// loaded animation keyframes (all reload on demand).
    /// Only when no avatar exists (a lot was left: its VM is gone; not while Create-A-Sim shows Sims). They were
    /// kept for the whole session: ~11 MB still in use back in the neighbourhood after a long session.
    size_t ReleaseCaches();

private:
    TS1AvatarContent();
    static std::string s_BasePath;
    bool m_Initialized = false;
    bool m_HaveData = false;
    std::unordered_map<uint64_t, std::unique_ptr<FSO::Vitaboy::Outfit>> Outfits;
};

}}}
