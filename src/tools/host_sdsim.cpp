/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims host tools — the 3DS SD card on the host (linked into sim3ds_host).
//
// Interposes the C library's file functions (fopen/fread/fseek/fwrite/fclose/setvbuf, stat, opendir/readdir) and
// counts what they would cost on the New 3DS into FSO::Files::Fs, the same counters the console fills from its sdmc
// device hook (src/platform/3ds/main.cpp), so the step / heartbeat lines of log.txt (SIM3DS_LOGSTEPS=1) have the same
// columns on both.
//
// The device requests are modelled like devkitARM's newlib + libctru's archive device do them: a FILE has a 1 KB
// buffer unless setvbuf gave it another (FilePool: 32+ KB, ReadWholeFile: none), a buffer refill or a read larger than
// the buffer is one FSFILE_Read, fseek only moves the offset (SEEK_END asks for the size), stat opens the file (or
// the directory) and closes it again, readdir fetches 32 entries per FSDIR_Read.
//
// SIM3DS_SDSIM=<scale> sleeps the calibrated cost of every operation (FSO::Files::SdCostModel, src/app/README.md
// "Calibrated SD cost model") times <scale> (1 = like the console, 0.1 = ten times faster), so loading screens and
// heartbeat lines can be watched at console speed. Without it nothing sleeps; the estimate is printed anyway.
#include "files/FilePool.h"
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <unordered_map>

namespace {
using FSO::Files::Fs;
using FSO::Files::SdCostModel;

struct FileState {
    size_t BufSize = 1024; // newlib's BUFSIZ on the 3DS
    long Pos = 0;          // logical position
    long BufStart = 0, BufEnd = 0; // file range held in the (simulated) stdio buffer
    bool Write = false;
};
struct DirState { long Entries = 0; };

std::unordered_map<FILE*, FileState>* g_Files;
std::unordered_map<DIR*, DirState>* g_Dirs;
double g_Scale = -1; // < 0: no sleeping
bool g_Busy = false;
bool g_On = false;
// FIX(sims) SAVE: the 3DS SD card's FAT rules and power cuts on the host.
// SIM3DS_FATLOCK=1 reports, =2 refuses what the 3DS FS service refuses (FIX(sims) FB1 had remove / rename / open for
// writing of an open file; now also: opening a file that is open for writing, renaming onto or from an open file);
// =2 renames onto an existing file like libctru's archive_rename (FSUSER_RenameFile fails with "already exists", then
// the target is DELETED and the rename retried: two separate steps), =3 refuses them (the bare FS service: EEXIST).
// SIM3DS_POWERCUT=N ends the process (_exit, nothing flushed) at the N-th write step on a path containing
// SIM3DS_POWERCUT_MATCH (default "/Saves/"): opening a file for writing, each fwrite (the N-th writes half its bytes),
// closing a written file (its stdio buffer is lost), remove, rename (2 steps for an overwrite under =2). SIM3DS_POWERCUT=0
// counts the steps and prints "[powercut] steps N" at exit; SIM3DS_POWERCUT_TRACE=1 prints each step.
struct OpenInfo { std::string Path; bool Write = false; };
std::unordered_map<FILE*, OpenInfo>* g_Locks; // open FILE -> normalised path (SIM3DS_FATLOCK / SIM3DS_POWERCUT)
int g_FatLock = 0; // FIX(sims) FB1: SIM3DS_FATLOCK=1 reports, =2 also fails (as the 3DS SD FAT does), =3 + no overwrite
long g_Cut = -1;   // FIX(sims) SAVE: SIM3DS_POWERCUT
long g_Steps = 0;
bool g_CutTrace = false;
std::string g_CutMatch = "/Saves/";

std::string NormPath(const char* path) {
    char buf[4096];
    if (realpath(path, buf)) return buf;
    std::string p = path; // not there yet: normalise the directory part
    size_t sl = p.find_last_of('/');
    if (sl != std::string::npos && realpath(p.substr(0, sl).c_str(), buf)) return std::string(buf) + p.substr(sl);
    return p;
}
// what: the operation; forWrite: the new handle writes (opens only). A conflict = the 3DS would refuse it.
bool FatLocked(const char* path, const char* what, bool newHandleWrites = true) {
    if (g_FatLock <= 0 || !g_Locks || path == nullptr) return false;
    std::string n = NormPath(path);
    for (auto& kv : *g_Locks) if (kv.second.Path == n) {
        if (!newHandleWrites && !kv.second.Write) continue; // two readers are fine
        fprintf(stderr, "[fatlock] %s of an open file: %s\n", what, n.c_str());
        return g_FatLock >= 2;
    }
    return false;
}
bool CutWatched(const std::string& path) { return g_Cut >= 0 && path.find(g_CutMatch) != std::string::npos; }
/// One write step; true = the power goes now (the caller may finish a partial write, then calls PowerCut()).
bool CutStep(const char* what, const std::string& path) {
    if (!CutWatched(path)) return false;
    g_Steps++;
    if (g_CutTrace) fprintf(stderr, "[powercut] step %ld: %s %s\n", g_Steps, what, path.c_str());
    return g_Cut > 0 && g_Steps == g_Cut;
}
[[noreturn]] void PowerCut(const char* what, const std::string& path) {
    fprintf(stderr, "[powercut] POWER CUT at step %ld: %s %s\n", g_Steps, what, path.c_str());
    _exit(86);
}
void ReportSteps() { if (g_Cut == 0) fprintf(stderr, "[powercut] steps %ld\n", g_Steps); }
std::string PathOf(FILE* f) { if (!g_Locks) return ""; auto it = g_Locks->find(f); return it == g_Locks->end() ? "" : it->second.Path; }
bool IsWrite(FILE* f) { if (!g_Locks) return false; auto it = g_Locks->find(f); return it != g_Locks->end() && it->second.Write; }

void Init() {
    static bool done = false;
    if (done) return;
    done = true;
    g_Files = new std::unordered_map<FILE*, FileState>();
    g_Dirs = new std::unordered_map<DIR*, DirState>();
    if (const char* s = getenv("SIM3DS_SDSIM")) g_Scale = atof(s);
    if (const char* s = getenv("SIM3DS_FATLOCK")) g_FatLock = atoi(s);
    if (const char* s = getenv("SIM3DS_POWERCUT")) { g_Cut = atol(s); atexit(ReportSteps); }
    if (const char* s = getenv("SIM3DS_POWERCUT_MATCH")) g_CutMatch = s;
    g_CutTrace = getenv("SIM3DS_POWERCUT_TRACE") != nullptr;
}

bool Watched(const char* path) {
    if (!g_On || path == nullptr || g_Busy) return false;
    static const char* skip[] = { "/proc", "/dev", "/sys", "/etc", "/usr", "/lib", "/tmp/.", nullptr };
    for (int i = 0; skip[i]; i++) if (strncmp(path, skip[i], strlen(skip[i])) == 0) return false;
    size_t n = strlen(path);
    if (n > 4 && strcmp(path + n - 4, ".png") == 0) return false; // host screenshots
    return true;
}

void Cost(double ms) {
    if (g_Scale > 0 && ms > 0) std::this_thread::sleep_for(std::chrono::microseconds((long long)(ms * g_Scale * 1000.0)));
}
void After() {
    if (FSO::Files::FsOps::OnOp && !g_Busy) { g_Busy = true; FSO::Files::FsOps::OnOp(); g_Busy = false; }
}

template <typename F> F Real(const char* name) {
    static_assert(sizeof(F) == sizeof(void*), "function pointer");
    void* p = dlsym(RTLD_NEXT, name);
    F f;
    memcpy(&f, &p, sizeof f);
    return f;
}

void CountOpen() { Fs.Opens++; Fs.OpenMs += SdCostModel::OpenMs; Cost(SdCostModel::OpenMs); }
void CountRead(size_t bytes) {
    double ms = SdCostModel::ReadMs + bytes / 1048576.0 * SdCostModel::MsPerMB;
    Fs.Reads++; Fs.BytesRead += bytes; Fs.ReadMs += ms; Cost(ms);
}
} // namespace

namespace FSO { namespace Files {
/// sim3ds_host: start counting (after its own start-up) — see SIM3DS_SDSIM above.
void HostSdSimEnable(bool on) { Init(); g_On = on; }
}}

extern "C" {

static bool WriteMode(const char* mode) { return strchr(mode, 'w') || strchr(mode, 'a') || strchr(mode, '+'); }
static FILE* OpenCommon(FILE* f, const char* path, const char* mode) {
    if (f != nullptr && (g_FatLock > 0 || g_Cut >= 0) && path != nullptr) {
        if (!g_Locks) g_Locks = new std::unordered_map<FILE*, OpenInfo>();
        OpenInfo oi; oi.Path = NormPath(path); oi.Write = WriteMode(mode);
        (*g_Locks)[f] = oi;
    }
    if (f == nullptr || !Watched(path)) return f;
    FileState st;
    st.Write = strchr(mode, 'w') || strchr(mode, 'a') || strchr(mode, '+');
    (*g_Files)[f] = st;
    CountOpen();
    After();
    return f;
}

FILE* fopen(const char* path, const char* mode) {
    Init();
    static auto real = Real<FILE* (*)(const char*, const char*)>("fopen");
    if (WriteMode(mode) ? FatLocked(path, "open for writing") : FatLocked(path, "open for reading", false)) { errno = EACCES; return nullptr; }
    if (WriteMode(mode) && path && CutStep("open for writing", NormPath(path))) PowerCut("open for writing", NormPath(path));
    FILE* f = real(path, mode);
    if (f == nullptr && Watched(path)) { CountOpen(); After(); return f; } // a failed open costs the same
    return OpenCommon(f, path, mode);
}
FILE* fopen64(const char* path, const char* mode) {
    Init();
    static auto real = Real<FILE* (*)(const char*, const char*)>("fopen64");
    if (WriteMode(mode) ? FatLocked(path, "open for writing") : FatLocked(path, "open for reading", false)) { errno = EACCES; return nullptr; }
    if (WriteMode(mode) && path && CutStep("open for writing", NormPath(path))) PowerCut("open for writing", NormPath(path));
    FILE* f = real(path, mode);
    if (f == nullptr && Watched(path)) { CountOpen(); After(); return f; }
    return OpenCommon(f, path, mode);
}
FILE* freopen(const char* path, const char* mode, FILE* old) {
    Init();
    static auto real = Real<FILE* (*)(const char*, const char*, FILE*)>("freopen");
    if (g_Files) g_Files->erase(old);
    return OpenCommon(real(path, mode, old), path, mode);
}

int setvbuf(FILE* f, char* buf, int mode, size_t size) {
    Init();
    static auto real = Real<int (*)(FILE*, char*, int, size_t)>("setvbuf");
    if (g_Files) {
        auto it = g_Files->find(f);
        if (it != g_Files->end()) it->second.BufSize = mode == _IONBF ? 0 : size;
    }
    return real(f, buf, mode, size);
}

size_t fread(void* ptr, size_t size, size_t n, FILE* f) {
    static auto real = Real<size_t (*)(void*, size_t, size_t, FILE*)>("fread");
    size_t got = real(ptr, size, n, f);
    if (!g_Files) return got;
    auto it = g_Files->find(f);
    if (it == g_Files->end()) return got;
    FileState& s = it->second;
    long want = (long)(size * n);
    long avail = (long)(got * size);
    long pos = s.Pos, end = pos + want;
    // served from the buffer as far as it goes
    if (pos >= s.BufStart && pos < s.BufEnd) pos = s.BufEnd < end ? s.BufEnd : end;
    bool any = false;
    while (pos < end) {
        long rest = end - pos;
        if (s.BufSize == 0 || rest >= (long)s.BufSize) { // straight into the caller's memory
            CountRead((size_t)rest);
            pos = end;
            s.BufStart = s.BufEnd = 0;
        } else { // refill the buffer
            CountRead(s.BufSize);
            s.BufStart = pos;
            s.BufEnd = pos + (long)s.BufSize;
            pos = s.BufEnd < end ? s.BufEnd : end;
        }
        any = true;
    }
    s.Pos += avail;
    if (avail < want) { s.BufStart = s.BufEnd = 0; } // EOF / error: position unknown, next read refills
    if (any) After();
    return got;
}

size_t fwrite(const void* ptr, size_t size, size_t n, FILE* f) {
    static auto real = Real<size_t (*)(const void*, size_t, size_t, FILE*)>("fwrite");
    if (g_Cut >= 0 && IsWrite(f) && size * n > 0 && CutStep("write", PathOf(f))) { // half of it reaches the card
        static auto realFlush = Real<int (*)(FILE*)>("fflush");
        real(ptr, 1, size * n / 2, f); realFlush(f);
        PowerCut("write", PathOf(f));
    }
    size_t r = real(ptr, size, n, f);
    if (!g_Files) return r;
    auto it = g_Files->find(f);
    if (it == g_Files->end()) return r;
    size_t bytes = r * size;
    it->second.Pos += (long)bytes;
    // modelled as one write request per call (buffered writes are flushed in buffer-sized pieces; close enough)
    double ms = SdCostModel::ReadMs + bytes / 1048576.0 * SdCostModel::MsPerMB;
    Fs.Writes++; Fs.BytesWritten += bytes; Fs.WriteMs += ms; Cost(ms);
    After();
    return r;
}

static int SeekCommon(FILE* f, long off, int whence, int r) {
    if (!g_Files) return r;
    auto it = g_Files->find(f);
    if (it == g_Files->end() || r != 0) return r;
    FileState& s = it->second;
    long target = whence == SEEK_SET ? off : (whence == SEEK_CUR ? s.Pos + off : -1);
    if (whence == SEEK_END) { // FSFILE_GetSize
        Fs.Seeks++; Fs.SeekMs += SdCostModel::SeekMs; Cost(SdCostModel::SeekMs);
        s.Pos = ftell(f);
        s.BufStart = s.BufEnd = 0;
        After();
        return r;
    }
    if (target != s.Pos) {
        // newlib keeps the buffer when the target is inside it (read-only stream), else it is dropped
        if (!(target >= s.BufStart && target < s.BufEnd)) s.BufStart = s.BufEnd = 0;
        Fs.Seeks++; // only moves the offset on the 3DS: counted, costs ~0
        s.Pos = target;
    }
    return r;
}
int fseek(FILE* f, long off, int whence) {
    static auto real = Real<int (*)(FILE*, long, int)>("fseek");
    return SeekCommon(f, off, whence, real(f, off, whence));
}
int fseeko(FILE* f, off_t off, int whence) {
    static auto real = Real<int (*)(FILE*, off_t, int)>("fseeko");
    return SeekCommon(f, (long)off, whence, real(f, off, whence));
}
int fseeko64(FILE* f, off_t off, int whence) {
    static auto real = Real<int (*)(FILE*, off_t, int)>("fseeko64");
    return SeekCommon(f, (long)off, whence, real(f, off, whence));
}

int fclose(FILE* f) {
    static auto real = Real<int (*)(FILE*)>("fclose");
    if (g_Cut >= 0 && IsWrite(f) && CutStep("close", PathOf(f))) PowerCut("close (buffer lost)", PathOf(f));
    if (g_Locks) g_Locks->erase(f);
    if (g_Files) {
        auto it = g_Files->find(f);
        if (it != g_Files->end()) {
            g_Files->erase(it);
            Fs.Closes++; Fs.CloseMs += SdCostModel::CloseMs; Cost(SdCostModel::CloseMs);
        }
    }
    return real(f);
}

int remove(const char* path) {
    Init();
    static auto real = Real<int (*)(const char*)>("remove");
    if (FatLocked(path, "remove")) { errno = EACCES; return -1; }
    if (path && CutStep("remove", NormPath(path))) PowerCut("remove", NormPath(path));
    return real(path);
}
int rename(const char* from, const char* to) {
    Init();
    static auto real = Real<int (*)(const char*, const char*)>("rename");
    static auto realRemove = Real<int (*)(const char*)>("remove");
    if (FatLocked(from, "rename") || FatLocked(to, "rename onto")) { errno = EACCES; return -1; }
    struct stat st;
    static auto realStat = Real<int (*)(const char*, struct stat*)>("stat");
    bool exists = to && realStat(to, &st) == 0 && !S_ISDIR(st.st_mode);
    std::string nf = from ? NormPath(from) : "", nt = to ? NormPath(to) : "";
    if (exists && g_FatLock >= 3) { fprintf(stderr, "[fatlock] rename onto an existing file refused: %s\n", nt.c_str()); errno = EEXIST; return -1; }
    if (exists && (g_FatLock == 2 || g_Cut >= 0)) { // libctru archive_rename: delete the target, then rename
        if (g_FatLock >= 1) fprintf(stderr, "[fatlock] rename onto an existing file (libctru deletes it first): %s\n", nt.c_str());
        if (CutStep("rename: delete target", nt)) PowerCut("rename: delete target", nt);
        realRemove(to);
    }
    if (CutStep("rename", nt.empty() ? nf : nt)) PowerCut("rename", nf + " -> " + nt);
    return real(from, to);
}

int stat(const char* path, struct stat* st) {
    Init();
    static auto real = Real<int (*)(const char*, struct stat*)>("stat");
    int r = real(path, st);
    if (Watched(path)) {
        // archive_stat: FSUSER_OpenFile + close, or (directory) a failed OpenFile + OpenDirectory + close
        double ms = (r == 0 && S_ISDIR(st->st_mode)) ? SdCostModel::StatDirMs : SdCostModel::StatMs;
        Fs.Stats++; Fs.StatMs += ms; Cost(ms);
        After();
    }
    return r;
}

DIR* opendir(const char* path) {
    Init();
    static auto real = Real<DIR* (*)(const char*)>("opendir");
    DIR* d = real(path);
    if (Watched(path)) {
        Fs.DirOpens++; Fs.DirMs += SdCostModel::DirOpenMs; Cost(SdCostModel::DirOpenMs);
        if (d) (*g_Dirs)[d] = DirState();
        After();
    }
    return d;
}
struct dirent* readdir(DIR* d) {
    static auto real = Real<struct dirent* (*)(DIR*)>("readdir");
    struct dirent* e = real(d);
    if (!g_Dirs) return e;
    auto it = g_Dirs->find(d);
    if (it == g_Dirs->end()) return e;
    // "." and ".." are not returned by the 3DS; FSDIR_Read fetches 32 entries at a time
    if (e && (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)) return e;
    if (it->second.Entries++ % 32 == 0) { Fs.DirReads++; Fs.DirMs += SdCostModel::DirReadMs; Cost(SdCostModel::DirReadMs); After(); }
    return e;
}
int closedir(DIR* d) {
    static auto real = Real<int (*)(DIR*)>("closedir");
    if (g_Dirs) g_Dirs->erase(d);
    return real(d);
}

} // extern "C"
