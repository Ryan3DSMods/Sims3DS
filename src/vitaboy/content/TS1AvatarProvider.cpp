/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// TS1 avatar content providers (see TS1AvatarProvider.h)
#include "files/SafeFile.h" // FIX(sims) SAVE
#include "files/FilePool.h"
#include "files/LoadProgress.h"
#include "vitaboy/content/TS1AvatarProvider.h"
#include "vitaboy/BCFReadProxy.h"
#include "files/FAR1/FAR1Archive.h"
#include "files/formats/iff/chunks/STR.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <functional>
#include <sys/stat.h>
// Plain arm-none-eabi newlib has no <dirent.h>; devkitARM+libctru does (3DS builds pass -DST_HAVE_DIRENT,
// same switch as ContentManager.cpp). Without it the known archives/folders are probed by name.
#if !defined(__3DS__) || defined(ST_HAVE_DIRENT)
#include <dirent.h>
#define ST_VB_DIRENT 1
#endif

namespace FSO { namespace Content { namespace TS1 {

using namespace FSO::Vitaboy;

static std::string Lower(std::string s) {
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}
static bool EndsWith(const std::string& s, const std::string& suf) {
    return s.size() >= suf.size() && s.compare(s.size() - suf.size(), suf.size(), suf) == 0;
}
static std::string FileNameOf(const std::string& path) {
    auto p = path.find_last_of("/\\");
    return p == std::string::npos ? path : path.substr(p + 1);
}
static bool ReadFile(const std::string& path, std::vector<uint8_t>& out) {
    return FSO::Files::ReadWholeFile(path, out); // TS1(sims): counted, retries after releasing pooled handles
}
// file types the avatar system reads
static bool Relevant(const std::string& lowerName) {
    static const char* exts[] = { ".bcf", ".cfp", ".bmf", ".bmp", ".tga", ".ndx", ".cmx", ".skn" };
    for (auto e : exts) if (EndsWith(lowerName, e)) return true;
    return false;
}

// ---------------------------------------------------------------- TS1Provider

TS1Provider::TS1Provider() {}
TS1Provider::~TS1Provider() {
    for (auto& p : FarPaths) FSO::Files::FilePool::Close(p);
}

void TS1Provider::AddFar(const std::string& path) {
    std::unique_ptr<FSO::Files::FAR1::FAR1Archive> far;
    try { far.reset(new FSO::Files::FAR1::FAR1Archive(path, false)); }
    catch (...) { return; }
    int32_t index = (int32_t)FarPaths.size();
    bool any = false;
    for (auto& e : far->GetAllFarEntries()) {
        auto name = Lower(FileNameOf(e.Filename));
        if (!Relevant(name)) continue;
        any = true;
        // TS1(sims): every expansion pack folder of every .far has its own "AnimationOut.ndx" (Animations\,
        // Skins\, Accessories\, ExpansionPackAssets\ ...): keep them all (made unique), or only the first archive's
        // animations/suits would be indexed.
        if (EndsWith(name, ".ndx") && Entries.count(name)) {
            int n = 2;
            while (Entries.count(name + "#" + std::to_string(n))) n++;
            name = name + "#" + std::to_string(n);
        }
        if (Entries.count(name)) continue; // first archive wins
        Entry en; en.Far = index; en.Offset = (uint32_t)e.DataOffset; en.Size = (uint32_t)e.DataLength;
        Entries[name] = en;
    }
    far.reset(); // the manifest is indexed above; entries are read directly by offset
    if (!any) return; // e.g. Objects.far / Global.far: nothing for us
    FarPaths.push_back(path); // TS1(sims): read through the shared FilePool (no FILE* held per archive)
}

void TS1Provider::AddFolder(const std::string& path) {
    for (auto& de : FSO::Files::ListInstallDirectory(path)) { // TS1(sims): remembered listing (files/FilePool.h)
        if (de.IsDir) continue;
        const std::string& n = de.Name;
        auto name = Lower(n);
        if (!Relevant(name)) continue;
        if (Entries.count(name)) continue;
        Entry en; en.Far = -1; en.Offset = (uint32_t)LoosePaths.size(); // FIX(sims): see TS1AvatarProvider.h
        LoosePaths.push_back(path + "/" + n);
        Entries[name] = en;
    }
}

void TS1Provider::Init(const std::string& baseDir) {
    // TS1(sims) hardware pass: the index of the last boot, while the install's archive list is the same
    std::string cacheFile, key;
    if (!TS1AvatarContent::CacheDir.empty() && !TS1AvatarContent::InstallSignature.empty()) {
        cacheFile = TS1AvatarContent::CacheDir + "/avatars.cache";
        uint64_t h = 1469598103934665603ULL;
        for (unsigned char c : TS1AvatarContent::InstallSignature) { h ^= c; h *= 1099511628211ULL; }
        char kb[64];
        snprintf(kb, sizeof kb, "STAV1 %zu %016llx", TS1AvatarContent::InstallSignature.size(), (unsigned long long)h);
        key = kb;
        if (LoadIndexCache(cacheFile, baseDir, key)) { IndexKey = key + " " + std::to_string(Slots.size()) + " " + std::to_string(LoosePaths.size()); return; }
        FSO::Files::LoadProgress::Step("avatars: indexing the Sim files (first run only)");
    }
    // C#: FAR1Provider over every *.far of the install + bare folders for .bmp/.cmx/.skn in
    // GameData/Skins/. We walk the install (3 levels, skipping user data) for FARs and for
    // Skins/SkinsBuy folders (ExpansionShared/SkinsBuy holds buyable skins in the EPs).
    // TS1(sims) hardware pass: typed directory listings (no stat per entry: 3,600 stats + 360 directory operations,
    // ~20 s on the 3DS) and the folders that hold no avatar data (music, sounds, saves, templates) are not entered.
    std::vector<std::string> fars, folders;
    std::function<void(const std::string&, int)> walk = [&](const std::string& dir, int depth) {
        for (auto& e : FSO::Files::ListInstallDirectory(dir)) {
            auto full = dir + "/" + e.Name;
            auto ln = Lower(e.Name);
            if (e.IsDir) {
                if (ln.compare(0, 8, "userdata") == 0 || ln.compare(0, 8, "template") == 0) continue;
                if (ln == "music" || ln == "sounddata" || ln == "sound" || ln == "saves" || ln == "cache") continue;
                if (ln == "skins" || ln == "skinsbuy") folders.push_back(full);
                if (depth < 3) walk(full, depth + 1);
            } else if (EndsWith(ln, ".far")) {
                fars.push_back(full);
            }
        }
    };
    walk(baseDir, 0);
#ifndef ST_VB_DIRENT
    // no directory listing: the shipped layout (the 3DS SD card is case-insensitive)
    for (auto f : { "/GameData/Animation/Animation.far", "/GameData/Textures/Textures.far",
                    "/ExpansionShared/ExpansionShared.far", // TS1(sims): + every expansion pack's far
                    "/ExpansionPack/ExpansionPack.far", "/ExpansionPack2/ExpansionPack2.far", "/ExpansionPack3/ExpansionPack3.far",
                    "/ExpansionPack4/ExpansionPack4.far", "/ExpansionPack5/ExpansionPack5.far", "/ExpansionPack6/ExpansionPack6.far",
                    "/ExpansionPack7/ExpansionPack7.far", "/Deluxe/Deluxe.far" })
        if (FILE* t = fopen((baseDir + f).c_str(), "rb")) { fclose(t); fars.push_back(baseDir + f); }
    for (auto f : { "/GameData/Skins", "/ExpansionShared/SkinsBuy", "/ExpansionPack2/Skins" }) ProbeFolders.push_back(baseDir + f);
#endif
    // base game archives first so they win name clashes
    std::stable_sort(fars.begin(), fars.end(), [](const std::string& a, const std::string& b) {
        bool ga = Lower(a).find("/gamedata/") != std::string::npos, gb = Lower(b).find("/gamedata/") != std::string::npos;
        return ga && !gb;
    });
    for (size_t i = 0; i < fars.size(); i++) {
        FSO::Files::LoadProgress::Progress("Indexing Sim files (first run only)", i, fars.size());
        AddFar(fars[i]);
    }
    for (auto& f : folders) AddFolder(f);
    Freeze();
    if (!cacheFile.empty()) {
        SaveIndexCache(cacheFile, baseDir, key);
        FSO::Files::InstallDirCache::Save(); // the listings of this walk (dirs.cache)
        IndexKey = key + " " + std::to_string(Slots.size()) + " " + std::to_string(LoosePaths.size());
    }
}

// avatars.cache: "<key>\n", "F <far path relative to the install>" lines, "L <loose file path>" lines, then one line
// per frozen slot (sorted by name): "<name>\t<far, -1 = loose>\t<offset>\t<size>". Paths relative to the install so a
// cache made on a PC (tools/simcache) works on the SD card.
bool TS1Provider::LoadIndexCache(const std::string& file, const std::string& baseDir, const std::string& key) {
    std::vector<uint8_t> bytes;
    if (!FSO::Files::ReadWholeFile(file, bytes)) return false;
    const char* p = (const char*)bytes.data();
    const char* end = p + bytes.size();
    auto line = [&](const char*& b, const char*& e) -> bool {
        if (p >= end) return false;
        b = p;
        const char* nl = (const char*)memchr(p, '\n', (size_t)(end - p));
        e = nl ? nl : end;
        p = nl ? nl + 1 : end;
        return true;
    };
    const char *b, *e;
    if (!line(b, e) || std::string(b, e) != key) return false;
    std::vector<std::string> fars, loose;
    std::string pool;
    std::vector<Slot> slots;
    pool.reserve(bytes.size() / 2);
    while (line(b, e)) {
        if (e - b >= 2 && b[1] == ' ' && (b[0] == 'F' || b[0] == 'L')) {
            (b[0] == 'F' ? fars : loose).push_back(baseDir + "/" + std::string(b + 2, e));
            continue;
        }
        const char* t1 = (const char*)memchr(b, '\t', (size_t)(e - b));
        if (t1 == nullptr) return false;
        Slot s;
        s.Name = (uint32_t)pool.size();
        pool.append(b, t1);
        pool.push_back('\0');
        char* q = nullptr;
        s.E.Far = (int32_t)strtol(t1 + 1, &q, 10);
        s.E.Offset = (uint32_t)strtoul(q + 1, &q, 10);
        s.E.Size = (uint32_t)strtoul(q + 1, &q, 10);
        if (s.E.Far >= 0 ? s.E.Far >= (int32_t)fars.size() : s.E.Offset >= loose.size()) return false;
        slots.push_back(s);
    }
    if (slots.empty()) return false;
    FarPaths = std::move(fars);
    LoosePaths = std::move(loose);
    NamePool = std::move(pool);
    NamePool.shrink_to_fit();
    Slots = std::move(slots);
    std::unordered_map<std::string, Entry>().swap(Entries);
    return true;
}

void TS1Provider::SaveIndexCache(const std::string& file, const std::string& baseDir, const std::string& key) const {
    FILE* f = FSO::Files::SafeFile::BeginWrite(file); // FIX(sims) SAVE: the crash-safe writer (was remove + rename)
    if (f == nullptr) return;
    auto rel = [&](const std::string& p) { return p.compare(0, baseDir.size() + 1, baseDir + "/") == 0 ? p.substr(baseDir.size() + 1) : p; };
    fprintf(f, "%s\n", key.c_str());
    for (auto& fp : FarPaths) fprintf(f, "F %s\n", rel(fp).c_str());
    for (auto& lp : LoosePaths) fprintf(f, "L %s\n", rel(lp).c_str());
    const char* pool = NamePool.c_str();
    for (auto& s : Slots) fprintf(f, "%s\t%d\t%u\t%u\n", pool + s.Name, (int)s.E.Far, (unsigned)s.E.Offset, (unsigned)s.E.Size);
    FSO::Files::SafeFile::EndWrite(file, f);
}

void TS1Provider::Freeze() // FIX(sims) memory: see TS1AvatarProvider.h
{
    std::vector<const std::pair<const std::string, Entry>*> all;
    all.reserve(Entries.size());
    size_t bytes = 0;
    for (auto& kv : Entries) { all.push_back(&kv); bytes += kv.first.size() + 1; }
    std::sort(all.begin(), all.end(), [](const std::pair<const std::string, Entry>* a, const std::pair<const std::string, Entry>* b) { return a->first < b->first; });
    NamePool.clear(); NamePool.reserve(bytes);
    Slots.clear(); Slots.reserve(all.size());
    for (auto* kv : all) {
        Slots.push_back({ (uint32_t)NamePool.size(), kv->second });
        NamePool.append(kv->first);
        NamePool.push_back('\0');
    }
    std::unordered_map<std::string, Entry>().swap(Entries);
    LoosePaths.shrink_to_fit();
}

const TS1Provider::Entry* TS1Provider::Find(const std::string& lowerName) const
{
    if (!Entries.empty()) { auto it = Entries.find(lowerName); return it == Entries.end() ? nullptr : &it->second; }
    const char* pool = NamePool.c_str();
    auto it = std::lower_bound(Slots.begin(), Slots.end(), lowerName, [pool](const Slot& s, const std::string& k) { return strcmp(pool + s.Name, k.c_str()) < 0; });
    if (it == Slots.end() || lowerName != pool + it->Name) return nullptr;
    return &it->E;
}

bool TS1Provider::Has(const std::string& lowerName) const {
    if (Find(lowerName) != nullptr) return true;
    for (auto& dir : ProbeFolders) {
        if (FILE* t = fopen((dir + "/" + lowerName).c_str(), "rb")) { fclose(t); return true; }
    }
    return false;
}

bool TS1Provider::IsLoose(const std::string& lowerName) const {
    auto e = Find(lowerName);
    return e != nullptr && e->Far < 0;
}

bool TS1Provider::GetBytes(const std::string& name, std::vector<uint8_t>& out) {
    auto found = Find(Lower(name));
    if (found == nullptr) {
        for (auto& dir : ProbeFolders) if (ReadFile(dir + "/" + Lower(name), out)) return true;
        return false;
    }
    auto& e = *found;
    if (e.Far < 0) return e.Offset < LoosePaths.size() && ReadFile(LoosePaths[e.Offset], out);
    // (FAR1Archive.GetEntry searches the manifest by name; we index once and seek directly.)
    if (e.Far >= (int32_t)FarPaths.size()) return false;
    return FSO::Files::FilePool::ReadAt(FarPaths[(size_t)e.Far], e.Offset, e.Size, out) && out.size() == e.Size;
}

std::vector<std::string> TS1Provider::ListNdx() const {
    std::vector<std::string> result;
    for (auto& kv : Entries) if (EndsWith(kv.first, ".ndx") || kv.first.find(".ndx#") != std::string::npos) result.push_back(kv.first);
    for (auto& s : Slots) { std::string n = NamePool.c_str() + s.Name; if (EndsWith(n, ".ndx") || n.find(".ndx#") != std::string::npos) result.push_back(n); }
    std::sort(result.begin(), result.end());
    return result;
}

std::vector<std::string> TS1Provider::ListExtension(const std::string& ext) const {
    std::vector<std::string> result;
    for (auto& kv : Entries) if (EndsWith(kv.first, ext)) result.push_back(kv.first);
    for (auto& s : Slots) { const char* n = NamePool.c_str() + s.Name; size_t l = strlen(n); // FIX(sims): frozen index
        if (l >= ext.size() && memcmp(n + l - ext.size(), ext.data(), ext.size()) == 0) result.push_back(n); }
    std::sort(result.begin(), result.end());
    return result;
}

// ---------------------------------------------------------------- TS1BCFProvider

void TS1BCFProvider::ReadIndex(const std::vector<uint8_t>& ndx) {
    // Animation.ndx: 3 sections (skeletons, suits, skills), each int32 count x (pstr name, pstr file.cmx)
    try {
        st::Reader io(ndx, st::Endian::Little);
        std::unordered_map<std::string, std::string>* sections[3] = { &SkelHostBCF, &SkinHostBCF, &AnimHostBCF };
        for (int s = 0; s < 3 && io.has_more(); s++) {
            auto count = io.ReadInt32();
            for (int32_t i = 0; i < count; i++) {
                auto name = Lower(io.ReadPascalString());
                auto file = Lower(io.ReadPascalString()) + ".bcf"; // stored in the FAR as <file>.cmx.bcf
                if (!sections[s]->count(name)) (*sections[s])[name] = file;
            }
        }
    } catch (...) {
        // truncated index: keep what we have
    }
}

void TS1BCFProvider::Init() {
    // TS1(sims) hardware pass: the three name maps as the last boot built them (bcfnames.cache, keyed by the avatar
    // index): reading every AnimationOut.ndx opens ~75 archives (one per download), ~1 s on the 3DS.
    std::string namesCache, namesKey;
    if (!TS1AvatarContent::CacheDir.empty() && !BaseProvider->IndexKey.empty()) {
        namesCache = TS1AvatarContent::CacheDir + "/bcfnames.cache";
        namesKey = "STBCF1 " + BaseProvider->IndexKey;
        std::vector<uint8_t> bytes;
        if (FSO::Files::ReadWholeFile(namesCache, bytes)) {
            const char* p = (const char*)bytes.data();
            const char* end = p + bytes.size();
            const char* nl = (const char*)memchr(p, '\n', (size_t)(end - p));
            if (nl && std::string(p, nl) == namesKey) {
                p = nl + 1;
                bool ok = true;
                while (p < end) {
                    nl = (const char*)memchr(p, '\n', (size_t)(end - p));
                    const char* e = nl ? nl : end;
                    const char* t = e - p > 2 ? (const char*)memchr(p + 2, '\t', (size_t)(e - p - 2)) : nullptr;
                    if (t == nullptr || p[1] != '\t') { ok = false; break; }
                    auto& map = p[0] == 'A' ? AnimHostBCF : (p[0] == 'S' ? SkinHostBCF : SkelHostBCF);
                    map.emplace(std::string(p + 2, t), std::string(t + 1, e));
                    p = nl ? nl + 1 : end;
                }
                if (ok && !SkelHostBCF.empty()) return;
                SkelHostBCF.clear(); SkinHostBCF.clear(); AnimHostBCF.clear();
            }
        }
    }
    //scan system for named items
    for (auto& ndx : BaseProvider->ListNdx()) { // TS1(sims): every AnimationOut.ndx, see TS1Provider::AddFar
        std::vector<uint8_t> data;
        if (BaseProvider->GetBytes(ndx, data)) ReadIndex(data);
    }
    // BCF files not covered by an index: map by their file name ("<name>.cmx.bcf" holds skill/suit <name>).
    // (FreeSims parsed every BCF here; the index makes that unnecessary for the shipped data.)
    // Loose text CMX files (custom skins / downloads in Skins folders) are parsed - they are tiny.
    // TS1(sims): reading every loose CMX (~300 files with the expansion packs' SkinsBuy) costs ~300 SD card opens
    // on the first Sim of every boot: the names they define are cached in <cache>/cmxnames.cache, valid while the
    // list of loose CMX files is the same. Lines: A|S|K <tab> lower-case name <tab> cmx, in file order (the
    // "first wins" rule below is applied on load exactly as when parsing).
    std::vector<std::string> loose;
    for (auto& cmx : BaseProvider->ListExtension(".cmx")) if (BaseProvider->IsLoose(cmx)) loose.push_back(cmx);
    std::string key = "STCMX1 " + std::to_string(loose.size());
    for (auto& n : loose) key += "|" + n;
    std::string cachePath = TS1AvatarContent::CacheDir.empty() ? std::string() : TS1AvatarContent::CacheDir + "/cmxnames.cache";
    struct Name { char Kind; std::string Name, Cmx; };
    std::vector<Name> names;
    bool cached = false;
    if (!cachePath.empty()) {
        std::vector<uint8_t> bytes;
        if (FSO::Files::ReadWholeFile(cachePath, bytes)) {
            std::string text(bytes.begin(), bytes.end());
            size_t nl = text.find('\n');
            if (nl != std::string::npos && text.compare(0, nl, key) == 0) {
                cached = true;
                size_t pos = nl + 1;
                while (pos < text.size()) {
                    size_t e = text.find('\n', pos);
                    if (e == std::string::npos) e = text.size();
                    std::string line = text.substr(pos, e - pos);
                    pos = e + 1;
                    size_t t1 = line.find('\t'), t2 = t1 == std::string::npos ? t1 : line.find('\t', t1 + 1);
                    if (t2 == std::string::npos || t1 != 1) continue;
                    names.push_back({ line[0], line.substr(2, t2 - 2), line.substr(t2 + 1) });
                }
            }
        }
    }
    if (!cached) {
        for (auto& cmx : loose) {
            std::vector<uint8_t> data;
            if (!BaseProvider->GetBytes(cmx, data)) continue;
            try {
                BCF file(data, true);
                for (auto& anim : file.Animations) names.push_back({ 'A', Lower(anim->Name), cmx });
                for (auto& skin : file.Appearances) names.push_back({ 'S', Lower(skin->Name), cmx });
                for (auto& skel : file.Skeletons) names.push_back({ 'K', Lower(skel->Name), cmx });
            } catch (...) {
                fprintf(stderr, "[vitaboy] bad CMX %s\n", cmx.c_str());
            }
        }
        if (!cachePath.empty()) {
            if (FILE* f = FSO::Files::SafeFile::BeginWrite(cachePath)) { // FIX(sims) SAVE
                fprintf(f, "%s\n", key.c_str());
                for (auto& n : names) fprintf(f, "%c\t%s\t%s\n", n.Kind, n.Name.c_str(), n.Cmx.c_str());
                FSO::Files::SafeFile::EndWrite(cachePath, f);
            }
        }
    }
    for (auto& n : names) {
        auto& map = n.Kind == 'A' ? AnimHostBCF : (n.Kind == 'S' ? SkinHostBCF : SkelHostBCF);
        if (!map.count(n.Name)) map[n.Name] = n.Cmx;
    }
    if (!namesCache.empty() && !SkelHostBCF.empty()) {
        if (FILE* f = FSO::Files::SafeFile::BeginWrite(namesCache)) { // FIX(sims) SAVE
            fprintf(f, "%s\n", namesKey.c_str());
            for (auto& kv : SkelHostBCF) fprintf(f, "K\t%s\t%s\n", kv.first.c_str(), kv.second.c_str());
            for (auto& kv : SkinHostBCF) fprintf(f, "S\t%s\t%s\n", kv.first.c_str(), kv.second.c_str());
            for (auto& kv : AnimHostBCF) fprintf(f, "A\t%s\t%s\n", kv.first.c_str(), kv.second.c_str());
            FSO::Files::SafeFile::EndWrite(namesCache, f);
        }
    }
}

BCF* TS1BCFProvider::GetBCF(const std::string& host) {
    auto it = BCFCache.find(host);
    if (it != BCFCache.end()) { it->second.Gen = CacheGen; return it->second.B.get(); }
    std::vector<uint8_t> data;
    std::string file = host;
    bool ok = BaseProvider->GetBytes(file, data);
    if (!ok && EndsWith(host, ".cmx.bcf")) { // fall back to a text .cmx of the same name
        file = host.substr(0, host.size() - 4);
        ok = BaseProvider->GetBytes(file, data);
    }
    BCF* result = nullptr;
    if (ok) {
        try {
            std::unique_ptr<BCF> bcf(new BCF(data, EndsWith(file, ".cmx")));
            result = bcf.get();
            auto& slot = BCFCache[host];
            slot.B = std::move(bcf);
            slot.Gen = CacheGen;
        } catch (...) {
            fprintf(stderr, "[vitaboy] failed to parse %s\n", file.c_str());
        }
    }
    if (result == nullptr) { auto& slot = BCFCache[host]; slot.B.reset(); slot.Gen = CacheGen; }
    return result;
}

size_t TS1BCFProvider::TrimCache() // FIX(sims): see TS1AvatarProvider.h
{
    size_t n = 0;
    for (auto it = BCFCache.begin(); it != BCFCache.end();) {
        if (it->second.Gen != CacheGen) { it = BCFCache.erase(it); n++; } else ++it;
    }
    CacheGen++;
    return n;
}

static std::string StripExt(const std::string& name, const char* ext) {
    auto l = Lower(name);
    if (EndsWith(l, ext)) l.resize(l.size() - strlen(ext));
    return l;
}

Animation* TS1BCFProvider::GetAnimation(const std::string& name) {
    auto key = StripExt(name, ".anim"); //remove .anim
    auto it = AnimHostBCF.find(key);
    if (it == AnimHostBCF.end()) {
        // not indexed: try "<name>.cmx.bcf"
        if (!BaseProvider->Has(key + ".cmx.bcf") && !BaseProvider->Has(key + ".cmx")) return nullptr;
        AnimHostBCF[key] = key + ".cmx.bcf";
        it = AnimHostBCF.find(key);
    }
    auto bcf = GetBCF(it->second);
    if (bcf == nullptr) return nullptr;
    for (auto& anim : bcf->Animations) if (Lower(anim->Name) == key) return anim.get();
    return nullptr;
}

bool TS1BCFProvider::EnrichAnimation(Animation* anim) {
    if (anim->KeyframesLoaded()) return true;
    //enrich animation with CFP
    std::vector<uint8_t> data;
    if (!BaseProvider->GetBytes(Lower(anim->XSkillName + ".cfp"), data)) return false;
    try {
        CFP cfp;
        cfp.Read(std::move(data));
        cfp.EnrichAnim(anim);
    } catch (...) {
        anim->UnloadKeyframes();
        return false;
    }
    return anim->KeyframesLoaded();
}

Appearance* TS1BCFProvider::GetAppearance(const std::string& name) {
    auto key = StripExt(name, ".apr"); //remove .apr
    auto it = SkinHostBCF.find(key);
    if (it == SkinHostBCF.end()) {
        if (!BaseProvider->Has(key + ".cmx.bcf") && !BaseProvider->Has(key + ".cmx")) return nullptr;
        SkinHostBCF[key] = BaseProvider->Has(key + ".cmx.bcf") ? key + ".cmx.bcf" : key + ".cmx";
        it = SkinHostBCF.find(key);
    }
    auto bcf = GetBCF(it->second);
    if (bcf == nullptr) return nullptr;
    for (auto& skin : bcf->Appearances) if (Lower(skin->Name) == key) return skin.get();
    return nullptr;
}

Skeleton* TS1BCFProvider::GetSkeleton(const std::string& name) {
    auto key = StripExt(name, ".skel"); //remove .skel
    auto it = SkelHostBCF.find(key);
    if (it == SkelHostBCF.end()) return nullptr;
    auto bcf = GetBCF(it->second);
    if (bcf == nullptr) return nullptr;
    for (auto& skel : bcf->Skeletons) if (Lower(skel->Name) == key) return skel.get();
    return nullptr;
}

// ---------------------------------------------------------------- meshes / textures

Mesh* TS1BMFProvider::Get(const std::string& name) {
    auto key = StripExt(name, ".mesh");
    auto it = Cache.find(key);
    if (it != Cache.end()) return it->second.get();
    std::unique_ptr<Mesh> mesh;
    std::vector<uint8_t> data;
    try {
        if (BaseProvider->GetBytes(key + ".bmf", data)) {
            mesh.reset(new Mesh());
            mesh->Read(data, true);
        } else if (BaseProvider->GetBytes(key + ".skn", data)) {
            CMXReadProxy io(data, false);
            mesh.reset(new Mesh());
            mesh->Read(io, true);
        }
    } catch (...) {
        fprintf(stderr, "[vitaboy] failed to read mesh %s\n", key.c_str());
        mesh.reset();
    }
    auto result = mesh.get();
    Cache[key] = std::move(mesh);
    return result;
}

Texture* TS1AvatarTextureProvider::Get(const std::string& name) {
    if (name.empty()) return nullptr;
    auto key = Lower(name);
    auto it = Cache.find(key);
    if (it != Cache.end()) return it->second.get();
    std::unique_ptr<Texture> tex;
    std::vector<uint8_t> data;
    if (BaseProvider->GetBytes(key + ".bmp", data)) {
        tex.reset(new Texture());
        tex->Name = name;
        if (!tex->LoadBMP(data)) tex.reset();
    }
    // STUB(3ds): ".tga" textures (TS1AvatarTextureProvider fallback) - no TGA decoder; none ship in the base game.
    auto result = tex.get();
    Cache[key] = std::move(tex);
    return result;
}

size_t TS1AvatarTextureProvider::MemoryBytes() const {
    size_t n = 0;
    for (auto& kv : Cache) if (kv.second) n += kv.second->MemoryBytes();
    return n;
}

// ---------------------------------------------------------------- TS1AvatarContent

size_t TS1AvatarContent::ReleaseCaches() // FIX(sims): see TS1AvatarProvider.h
{
    size_t bytes = AvatarTextures.MemoryBytes();
    AvatarMeshes.Clear();
    AvatarTextures.Clear();
    bytes += FSO::Vitaboy::Animation::UnloadAllKeyframes();
    BCF.TrimCache(); // after the keyframe list no longer points at its animations
    return bytes;
}

std::string TS1AvatarContent::s_BasePath;
std::string TS1AvatarContent::CacheDir;
std::string TS1AvatarContent::InstallSignature;

static bool LoadKeyframesHook(Animation* anim) {
    auto c = TS1AvatarContent::Get();
    if (!c->EnsureInit()) return false;
    return c->BCF.EnrichAnimation(anim);
}

TS1AvatarContent::TS1AvatarContent()
    : BCF(&TS1Global), AvatarMeshes(&TS1Global), AvatarTextures(&TS1Global) {}

TS1AvatarContent* TS1AvatarContent::Get() {
    static TS1AvatarContent instance;
    return &instance;
}

void TS1AvatarContent::SetBasePath(const std::string& baseDir) {
    auto c = Get();
    if (c->m_Initialized && s_BasePath == baseDir) return;
    s_BasePath = baseDir;
    c->m_Initialized = false;
}

bool TS1AvatarContent::EnsureInit() {
    if (m_Initialized) return m_HaveData;
    m_Initialized = true;
    // TS1(sims) hardware pass: its own step in log.txt, then back to the step that needed it
    std::string outer = FSO::Files::LoadProgress::Current;
    FSO::Files::LoadProgress::Step("avatars: file index + animation names");
    struct Back { std::string S; ~Back() { if (!S.empty()) FSO::Files::LoadProgress::Step(S + " (continued)"); } } back{ outer };
    m_HaveData = false;
    if (s_BasePath.empty()) return false;
    std::string base = s_BasePath;
    while (base.size() > 1 && (base.back() == '/' || base.back() == '\\')) base.pop_back();
    TS1Global.Init(base);
    BCF.Init();
    m_HaveData = !BCF.SkelHostBCF.empty() || !BCF.SkinHostBCF.empty();
    Animation::KeyframeLoader = &LoadKeyframesHook;
    return m_HaveData;
}

static uint64_t Fnv1a(const std::string& s) {
    uint64_t h = 1469598103934665603ULL;
    for (unsigned char c : s) { h ^= c; h *= 1099511628211ULL; }
    return h;
}

uint64_t TS1AvatarContent::RegisterOutfit(const std::string& aprName, const std::string& texName, bool withHands,
                                          const std::string handStrings[6]) {
    std::string key = Lower(aprName) + "|" + Lower(texName);
    if (withHands) for (int i = 0; i < 6; i++) key += "|" + Lower(handStrings[i]);
    uint64_t id = (0x7531ULL << 48) | (Fnv1a(key) & 0xFFFFFFFFFFFFULL);
    while (true) {
        auto it = Outfits.find(id);
        if (it == Outfits.end()) break;
        if (it->second->Name == key) return id; // already registered
        id = (0x7531ULL << 48) | ((id + 1) & 0xFFFFFFFFFFFFULL); // collision: probe
    }
    std::unique_ptr<Outfit> oft(new Outfit());
    oft->TS1AppearanceID = aprName;
    oft->TS1TextureID = texName;
    if (withHands) {
        // reuse Outfit.Read for the hand group layout
        Outfit tmp;
        tmp.Read(std::string(), handStrings);
        oft->LiteralHandgroup = tmp.LiteralHandgroup;
    }
    oft->ID = id;
    oft->Name = key;
    Outfits[id] = std::move(oft);
    return id;
}

FSO::Vitaboy::Outfit* TS1AvatarContent::GetOutfit(uint64_t id) {
    auto it = Outfits.find(id);
    return it == Outfits.end() ? nullptr : it->second.get();
}

uint64_t TS1AvatarContent::RegisterOutfitFromBodyStrings(FSO::Files::Formats::IFF::Chunks::STR* bodyStrings, bool head) {
    if (bodyStrings == nullptr) return 0;
    Outfit oft;
    std::string hands[6];
    if (head) {
        oft.ReadHead(bodyStrings);
        if (bodyStrings->GetString(2).empty()) return 0;
    } else {
        oft.Read(bodyStrings);
        if (bodyStrings->GetString(1).empty()) return 0;
        for (int i = 0; i < 6; i++) hands[i] = bodyStrings->GetString(17 + i);
    }
    return RegisterOutfit(oft.TS1AppearanceID, oft.TS1TextureID, !head, hands);
}

}}}
