/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims — see FilePool.h
#include "files/SafeFile.h" // FIX(sims) SAVE
#include "files/FilePool.h"
#include "core/io.h"
#include "files/LoadProgress.h"
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <unordered_set>
#include <algorithm>
#include <map>
#include <sys/stat.h>
#if !defined(__3DS__) || defined(ST_HAVE_DIRENT)
#include <dirent.h>
#define ST_POOL_DIRENT 1
#endif

namespace FSO { namespace Files {

int FilePool::MaxOpen = 16; // TS1(sims) hardware pass: was 6 (lot loads re-opened the same 10 archives ~200 times); 83 failed
#ifdef __3DS__
size_t FilePool::BufferSize = 32 * 1024;
#else
size_t FilePool::BufferSize = 32 * 1024;
#endif
IOStats FilePool::Stats;
FsOps Fs;
void (*FsOps::OnOp)() = nullptr;
// TS1(sims): fitted to the first New 3DS log (26 Oct; src/app/README.md "Calibrated SD cost model"): the boot steps'
// times against their file system operations counted on the host. Directory operations dominate: opening a directory,
// or stat'ing one (a failed FSUSER_OpenFile + FSUSER_OpenDirectory), ~36 ms; opening / stat'ing a file ~2 ms; reads
// ~0.3 ms + ~80 ms per MB; CPU ~22x the host's.
double SdCostModel::OpenMs = 2.0, SdCostModel::StatMs = 2.0, SdCostModel::StatDirMs = 36.0, SdCostModel::DirOpenMs = 36.0,
       SdCostModel::DirReadMs = 1.0, SdCostModel::ReadMs = 0.3, SdCostModel::SeekMs = 0.3, SdCostModel::MsPerMB = 80.0,
       SdCostModel::CloseMs = 0.5;
double SdCostModel::CpuFactor = 22.0;
std::function<void(const std::string&)> LoadProgress::StepSink;
std::string LoadProgress::Current;
std::function<void(const std::string&, size_t, size_t)> LoadProgress::ProgressSink;
// lets st::ReadAllBytes (src/core/io.h, header only) free the pool's handles when an fopen fails
static struct RegisterRelease { RegisterRelease() { st::ReleaseFileHandles = []() { FilePool::CloseAll(); }; } } g_RegisterRelease;

namespace {
struct Slot {
    std::string Path;
    FILE* F = nullptr;
    int64_t Pos = -1;   // stream position as far as we know (-1 = unknown)
    int64_t Size = -1;
    uint64_t LastUse = 0;
};
std::vector<Slot> g_Slots;
std::unordered_set<size_t> g_Seen; // hashes of the paths opened so far (IOStats::Files)
void NoteFile(const std::string& path) { if (g_Seen.insert(std::hash<std::string>()(path)).second) FilePool::Stats.Files++; }
uint64_t g_Clock = 0;

std::string ErrText(const std::string& path, int e) {
    return path + ": " + (e ? std::strerror(e) : "cannot open");
}

void CloseSlot(size_t i) {
    if (g_Slots[i].F) std::fclose(g_Slots[i].F);
    g_Slots.erase(g_Slots.begin() + (long)i);
}

Slot* Acquire(const std::string& path, std::string* err) {
    for (auto& s : g_Slots) if (s.Path == path) { s.LastUse = ++g_Clock; return &s; }
    while ((int)g_Slots.size() >= (FilePool::MaxOpen < 1 ? 1 : FilePool::MaxOpen)) {
        size_t lru = 0;
        for (size_t i = 1; i < g_Slots.size(); i++) if (g_Slots[i].LastUse < g_Slots[lru].LastUse) lru = i;
        CloseSlot(lru);
    }
    errno = 0;
    FILE* f = OpenBuffered(path.c_str(), "rb");
    if (f == nullptr && !g_Slots.empty()) {
        // out of handles (other readers hold files open): give back everything the pool holds and retry once
        int e = errno;
        while (!g_Slots.empty()) CloseSlot(g_Slots.size() - 1);
        errno = 0;
        f = OpenBuffered(path.c_str(), "rb");
        if (f == nullptr && errno == 0) errno = e;
    }
    if (f == nullptr) {
        FilePool::Stats.OpenFailures++;
        if (err) *err = ErrText(path, errno);
        return nullptr;
    }
    NoteFile(path);
    Slot s;
    s.Path = path;
    s.F = f;
    s.Pos = 0;
    s.LastUse = ++g_Clock;
    g_Slots.push_back(s);
    return &g_Slots.back();
}
} // namespace

FILE* OpenBuffered(const char* path, const char* mode) {
    FILE* f = std::fopen(path, mode);
    FilePool::Stats.Opens++;
    if (f != nullptr && FilePool::BufferSize > 0) std::setvbuf(f, nullptr, _IOFBF, FilePool::BufferSize);
    return f;
}

int64_t FilePool::ReadAt(const std::string& path, int64_t offset, void* dst, size_t len, std::string* err) {
    Slot* s = Acquire(path, err);
    if (s == nullptr) return -1;
    if (s->Pos != offset) {
        if (std::fseek(s->F, (long)offset, SEEK_SET) != 0) {
            if (err) *err = ErrText(path, errno);
            s->Pos = -1;
            return 0;
        }
        Stats.Seeks++;
        s->Pos = offset;
    }
    size_t got = len ? std::fread(dst, 1, len, s->F) : 0;
    Stats.Reads++;
    Stats.BytesRead += got;
    s->Pos = got == len ? offset + (int64_t)got : -1; // EOF / error: re-seek next time
    if (got != len) std::clearerr(s->F);
    return (int64_t)got;
}

bool FilePool::ReadAt(const std::string& path, int64_t offset, size_t len, std::vector<uint8_t>& out, std::string* err) {
    out.resize(len);
    int64_t got = ReadAt(path, offset, out.data(), len, err);
    if (got < 0) { out.clear(); return false; }
    out.resize((size_t)got);
    return true;
}

int64_t FilePool::Size(const std::string& path, std::string* err) {
    Slot* s = Acquire(path, err);
    if (s == nullptr) return -1;
    if (s->Size < 0) {
        std::fseek(s->F, 0, SEEK_END);
        s->Size = (int64_t)std::ftell(s->F);
        s->Pos = -1;
        Stats.Seeks++;
    }
    return s->Size;
}

void FilePool::Close(const std::string& path) {
    for (size_t i = 0; i < g_Slots.size(); i++) if (g_Slots[i].Path == path) { CloseSlot(i); return; }
}

void FilePool::CloseAll() {
    while (!g_Slots.empty()) CloseSlot(g_Slots.size() - 1);
}

int FilePool::OpenCount() { return (int)g_Slots.size(); }

bool ReadWholeFile(const std::string& path, std::vector<uint8_t>& out, std::string* err) {
    errno = 0;
    FILE* f = std::fopen(path.c_str(), "rb");
    FilePool::Stats.Opens++;
    if (f == nullptr && FilePool::OpenCount() > 0) {
        // out of handles: release the pool's and retry once
        FilePool::CloseAll();
        errno = 0;
        f = std::fopen(path.c_str(), "rb");
        FilePool::Stats.Opens++;
    }
    if (f == nullptr) {
        FilePool::Stats.OpenFailures++;
        if (err) *err = ErrText(path, errno);
        return false;
    }
    NoteFile(path);
    std::setvbuf(f, nullptr, _IONBF, 0); // one big fread: no stdio buffer copy
    long len = -1;
    if (std::fseek(f, 0, SEEK_END) == 0) len = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (len < 0) {
        if (err) *err = ErrText(path, errno) + " (size unknown)";
        std::fclose(f);
        return false;
    }
    try {
        out.resize((size_t)len);
    } catch (...) {
        std::fclose(f);
        throw std::runtime_error(path + ": out of memory reading " + std::to_string(len) + " bytes");
    }
    size_t rd = len > 0 ? std::fread(out.data(), 1, (size_t)len, f) : 0;
    FilePool::Stats.Reads++;
    FilePool::Stats.BytesRead += rd;
    int e = errno;
    std::fclose(f);
    if (rd != (size_t)len) {
        if (err) *err = path + ": short read (" + std::to_string(rd) + " of " + std::to_string(len) + " bytes" + (e ? std::string(", ") + std::strerror(e) : std::string()) + ")";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------- directory listings (TS1(sims) hardware pass)

std::vector<DirEntry> ListDirectory(const std::string& dir, bool* opened) {
    std::vector<DirEntry> r;
    if (opened) *opened = false;
#ifdef ST_POOL_DIRENT
    DIR* d = opendir(dir.c_str());
    if (!d) return r;
    if (opened) *opened = true;
    while (struct dirent* e = readdir(d)) {
        DirEntry de;
        de.Name = e->d_name;
        if (de.Name == "." || de.Name == "..") continue;
        bool known = false;
#if defined(DT_DIR) && defined(DT_REG)
        if (e->d_type == DT_DIR) { de.IsDir = true; known = true; }
        else if (e->d_type == DT_REG) known = true;
#endif
        if (!known) { struct stat st; de.IsDir = stat((dir + "/" + de.Name).c_str(), &st) == 0 && S_ISDIR(st.st_mode); }
        r.push_back(de);
    }
    closedir(d);
    std::sort(r.begin(), r.end(), [](const DirEntry& a, const DirEntry& b) { return a.Name < b.Name; });
#else
    (void)dir;
#endif
    return r;
}

std::string InstallDirCache::File;
std::string InstallDirCache::Root;
namespace {
struct Listing { std::vector<DirEntry> Entries; bool Exists = false; };
std::map<std::string, Listing> g_DirMemo;      // this run
std::map<std::string, Listing> g_DirPersisted; // dirs.cache
std::unordered_set<std::string> g_DirVerified; // listed live in this run and equal to dirs.cache
bool g_DirLoaded = false, g_DirDirty = false;

std::string Parent(const std::string& p) {
    size_t s = p.find_last_of('/');
    return s == std::string::npos || s == 0 ? std::string() : p.substr(0, s);
}
// dirs.cache: "STDIR1" then per directory "D <path>\t<0|1 exists>" followed by "  <d|f> <name>" lines
void LoadDirCache() {
    if (g_DirLoaded) return;
    g_DirLoaded = true;
    if (InstallDirCache::File.empty()) return;
    std::vector<uint8_t> bytes;
    if (!ReadWholeFile(InstallDirCache::File, bytes)) return;
    std::string text(bytes.begin(), bytes.end());
    size_t pos = text.find('\n');
    if (pos == std::string::npos || text.compare(0, pos, "STDIR1") != 0) return;
    pos++;
    Listing* cur = nullptr;
    while (pos < text.size()) {
        size_t nl = text.find('\n', pos);
        if (nl == std::string::npos) nl = text.size();
        if (nl > pos + 2 && text[pos] == 'D' && text[pos + 1] == ' ') {
            size_t tab = text.find('\t', pos);
            if (tab == std::string::npos || tab > nl) { cur = nullptr; }
            else {
                std::string d = text.substr(pos + 2, tab - pos - 2);
                if (!InstallDirCache::Root.empty() && (d.empty() || d[0] != '/') && d.find(':') == std::string::npos)
                    d = d.empty() ? InstallDirCache::Root : InstallDirCache::Root + "/" + d; // relative to the install
                cur = &g_DirPersisted[d];
                cur->Exists = text[tab + 1] == '1';
            }
        } else if (cur && nl > pos + 4 && text[pos] == ' ' && text[pos + 1] == ' ') {
            DirEntry e;
            e.IsDir = text[pos + 2] == 'd';
            e.Name = text.substr(pos + 4, nl - pos - 4);
            cur->Entries.push_back(e);
        }
        pos = nl + 1;
    }
}
} // namespace

std::vector<DirEntry> ListInstallDirectory(const std::string& dirIn, bool live) {
    std::string dir = dirIn;
    while (dir.size() > 1 && dir.back() == '/') dir.pop_back();
    auto m = g_DirMemo.find(dir);
    if (m != g_DirMemo.end()) return m->second.Entries;
    LoadDirCache();
    auto p = g_DirPersisted.find(dir);
    std::string parent = Parent(dir);
    if (!live && p != g_DirPersisted.end() && !parent.empty() && g_DirVerified.count(parent)) {
        g_DirMemo[dir] = p->second; // the parent did not change: trust the remembered listing
        return p->second.Entries;
    }
    Listing l;
    l.Entries = ListDirectory(dir, &l.Exists);
    if (p != g_DirPersisted.end() && p->second.Exists == l.Exists && p->second.Entries == l.Entries) g_DirVerified.insert(dir);
    else { g_DirPersisted[dir] = l; g_DirDirty = true; } // new / changed: its children are listed live too
    g_DirMemo[dir] = l;
    return l.Entries;
}

static bool InstallLookup(const std::string& path, bool wantDir) {
    std::string parent = Parent(path);
    if (parent.empty()) { struct stat st; return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode) == wantDir; }
    std::string name = path.substr(parent.size() + 1);
    for (auto& e : ListInstallDirectory(parent)) if (e.Name == name) return e.IsDir == wantDir;
    return false;
}
bool InstallFileExists(const std::string& path) { return InstallLookup(path, false); }
bool InstallDirExists(const std::string& path) { return InstallLookup(path, true); }

void InstallDirCache::Save() {
    if (!g_DirDirty || File.empty()) return;
    // a changed listing invalidates what was remembered below it: keep only the listings seen in this run plus the
    // remembered ones whose parent was verified unchanged
    std::map<std::string, Listing> keep;
    for (auto& kv : g_DirPersisted) {
        if (g_DirMemo.count(kv.first)) keep[kv.first] = g_DirMemo[kv.first];
        else if (g_DirVerified.count(Parent(kv.first))) keep[kv.first] = kv.second;
    }
    FILE* f = SafeFile::BeginWrite(File); // FIX(sims) SAVE: the crash-safe writer (was remove + rename)
    if (!f) return;
    std::fprintf(f, "STDIR1\n");
    const std::string& root = InstallDirCache::Root;
    for (auto& kv : keep) {
        std::string d = kv.first;
        if (!root.empty() && d == root) d.clear();
        else if (!root.empty() && d.compare(0, root.size() + 1, root + "/") == 0) d = d.substr(root.size() + 1);
        std::fprintf(f, "D %s\t%d\n", d.c_str(), kv.second.Exists ? 1 : 0);
        for (auto& e : kv.second.Entries) std::fprintf(f, "  %c %s\n", e.IsDir ? 'd' : 'f', e.Name.c_str());
    }
    SafeFile::EndWrite(File, f);
    g_DirDirty = false;
}

void InstallDirCache::Clear() {
    g_DirMemo.clear(); g_DirPersisted.clear(); g_DirVerified.clear();
    g_DirLoaded = false; g_DirDirty = false;
}

}} // namespace FSO::Files
