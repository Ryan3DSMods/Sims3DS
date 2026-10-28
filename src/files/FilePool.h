/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims — shared pool of open read-only files (FAR archives, sound archives, avatar archives).
//
// The 3DS cannot keep a FILE* open per archive: with the Complete Collection there are ~85 FAR files (Objects.far,
// 7 expansion packs, ExpansionShared, Deluxe, ~70 Downloads/*/*.far, Global, UIGraphics, Animation, Textures,
// SoundData...), and the SD card's FS service / newlib handle table runs out long before that (first hardware boot:
// "Could not open file: sdmc:/3ds/sims/GameData/walls.iff" with 83 archives held open). Every archive reader
// now refers to its file by path and reads through this pool, which keeps at most MaxOpen files open (LRU), reopens
// on demand and gives each open file a large stdio buffer.
//
// Main thread only (the 3DS audio thread streams music through its own FILE*, src/audio/Mp3Stream).
#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace FSO { namespace Files {

/// I/O counters (all readers that go through FilePool / ReadWholeFile). Used for the loading-screen progress, the
/// boot log (log.txt) and the host's SD-card time estimate.
struct IOStats {
    uint64_t BytesRead = 0;   // bytes delivered by fread
    uint64_t Reads = 0;       // fread calls
    uint64_t Seeks = 0;       // fseek calls that moved the position
    uint64_t Opens = 0;       // fopen calls (pool misses + whole-file reads)
    uint64_t OpenFailures = 0;
    uint64_t Files = 0;       // distinct files opened so far (pool + whole-file reads)
    /// Estimated time on a 3DS SD card through stdio (~4 MB/s, ~1 ms per seek, ~5 ms per open).
    double EstimatedSdMs() const { return BytesRead / 4000.0 + Seeks * 1.0 + Opens * 5.0 + Reads * 0.05; }
};

/// Platform-level file system operations, counted and timed below the C library: on the 3DS by a hook on the sdmc
/// device (src/platform/3ds/main.cpp: every open/stat/directory read/read/seek of the main thread, with the real time
/// spent in the FS service); on the host by src/tools/host_sdsim.cpp (sim3ds_host), which can also make each operation
/// cost what it costs on the console (SIM3DS_SDSIM). Read by the step / heartbeat lines of log.txt.
struct FsOps {
    uint64_t Opens = 0, Stats = 0, DirOpens = 0, DirReads = 0, Reads = 0, Seeks = 0, Writes = 0, Closes = 0;
    uint64_t BytesRead = 0, BytesWritten = 0;
    double OpenMs = 0, StatMs = 0, DirMs = 0, ReadMs = 0, SeekMs = 0, WriteMs = 0, CloseMs = 0;
    double TotalMs() const { return OpenMs + StatMs + DirMs + ReadMs + SeekMs + WriteMs + CloseMs; }
    /// Called after every counted operation (main thread only; the app's heartbeat log line).
    static void (*OnOp)();
};
extern FsOps Fs;
/// Calibrated cost model of the New 3DS SD card through devkitARM stdio (hardware log of 26 Oct, src/app/README.md
/// "Calibrated SD cost model"): milliseconds for the counted operations.
struct SdCostModel {
    static double OpenMs, StatMs, StatDirMs, DirOpenMs, DirReadMs, ReadMs, SeekMs, MsPerMB, CloseMs;
    /// host CPU time -> 804 MHz ARM11 time (sim3ds_host's per-step estimate)
    static double CpuFactor;
};

class FilePool {
public:
    /// Maximum simultaneously open files held by the pool.
    static int MaxOpen;
    /// stdio buffer size of pooled files (setvbuf); large sequential reads bypass it anyway.
    static size_t BufferSize;

    /// Reads up to len bytes at offset. Returns the number of bytes read (short at EOF), or -1 when the file cannot be
    /// opened (err gets "<path>: <strerror>").
    static int64_t ReadAt(const std::string& path, int64_t offset, void* dst, size_t len, std::string* err = nullptr);
    /// Reads [offset, offset+len) into out (resized to what was read). False when the file cannot be opened.
    static bool ReadAt(const std::string& path, int64_t offset, size_t len, std::vector<uint8_t>& out, std::string* err = nullptr);
    /// File size (-1 when it cannot be opened).
    static int64_t Size(const std::string& path, std::string* err = nullptr);
    /// Closes the pooled handle of this file (if open).
    static void Close(const std::string& path);
    static void CloseAll();
    static int OpenCount();

    static IOStats Stats;
};

/// A directory entry with its type taken from the directory read itself (dirent d_type; a stat only when unknown).
struct DirEntry {
    std::string Name;
    bool IsDir = false;
    bool operator==(const DirEntry& o) const { return Name == o.Name && IsDir == o.IsDir; }
};
/// Entries of a directory ("." / ".." skipped), sorted by name; empty when it cannot be opened (or without dirent).
/// TS1(sims) hardware pass: on the 3DS opening a directory or stat'ing one costs ~36 ms (a file ~2 ms), so the
/// content scanners list with types instead of stat'ing every entry (src/app/README.md "Calibrated SD cost model").
std::vector<DirEntry> ListDirectory(const std::string& dir, bool* opened = nullptr);
/// ListDirectory for the read-only install tree, remembered for the whole run (the object index, walls, floors and the
/// avatar index all scan Downloads/) and persisted in <cache>/dirs.cache: the listing of a directory is taken from
/// that file when its parent was listed live in this run and has not changed. live = always list it from the card
/// (Downloads/: a new download is a new folder there, so its subfolders can come from the cache). Never use it for
/// save folders.
std::vector<DirEntry> ListInstallDirectory(const std::string& dir, bool live = false);
/// stat-free existence checks backed by ListInstallDirectory of the parent (case-sensitive name match).
bool InstallFileExists(const std::string& path);
bool InstallDirExists(const std::string& path);
struct InstallDirCache {
    /// <cache>/dirs.cache (empty: in-memory only). Loaded on first use.
    static std::string File;
    /// The install root: paths under it are stored relative (a dirs.cache made on a PC by tools/simcache works on the SD).
    static std::string Root;
    /// Writes dirs.cache when listings were added or changed in this run.
    static void Save();
    /// Forget everything remembered (tests).
    static void Clear();
};

/// Reads a whole file (fopen/fread/fclose, nothing kept open). False with err = "<path>: <strerror>" (or a short read
/// description) on failure. Counts into FilePool::Stats.
bool ReadWholeFile(const std::string& path, std::vector<uint8_t>& out, std::string* err = nullptr);
/// fopen + a larger stdio buffer (3DS: newlib's default is 1 KB, i.e. one SD request per KB). Counts the open.
FILE* OpenBuffered(const char* path, const char* mode);

}} // namespace FSO::Files
