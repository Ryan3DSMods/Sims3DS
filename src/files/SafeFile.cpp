/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims — see SafeFile.h. FIX(sims) SAVE: new code.
#include "files/SafeFile.h"
#include "files/FilePool.h"
#include "files/formats/iff/IffFile.h"
#include "core/io.h"
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>

namespace FSO { namespace Files {

uint64_t SafeFile::Writes = 0, SafeFile::Failures = 0, SafeFile::Recovered = 0;
std::function<void(const std::string&)> SafeFile::Log;

static void Say(const std::string& line) {
    if (SafeFile::Log) { SafeFile::Log(line); return; }
    std::printf("[save] %s\n", line.c_str());
    std::fflush(stdout); // log.txt on the 3DS: the line must reach the card even if the console dies right after
}
static std::string Errno(int e) { return std::to_string(e) + " " + (e ? std::strerror(e) : "unknown"); }
static std::string Base(const std::string& p) { size_t s = p.find_last_of('/'); return s == std::string::npos ? p : p.substr(s + 1); }

bool SafeFile::Exists(const std::string& path) {
    struct stat st;
    return ::stat(path.c_str(), &st) == 0 && !(st.st_mode & S_IFDIR);
}

void SafeFile::ReleaseHandles(const std::string& path) {
    // deferred chunk readers first (they load what is still on disk, then drop the path), then the pooled handle
    try { FSO::Files::Formats::IFF::IffFile::DetachSource(path); } catch (std::exception& e) { Say("detach " + path + ": " + e.what()); }
    FilePool::Close(path);
}

static bool RemoveIfThere(const std::string& p, std::string* why) {
    if (!SafeFile::Exists(p)) return true;
    errno = 0;
    if (std::remove(p.c_str()) == 0) return true;
    int e = errno;
    FilePool::CloseAll(); // a pooled reader we did not know about: give everything back and retry once
    errno = 0;
    if (std::remove(p.c_str()) == 0) return true;
    if (errno == 0) errno = e;
    if (why) *why = "remove " + Base(p) + ": " + Errno(errno);
    return false;
}

static bool RenameRetry(const std::string& from, const std::string& to, std::string* why) {
    errno = 0;
    if (std::rename(from.c_str(), to.c_str()) == 0) return true;
    int e = errno;
    FilePool::CloseAll();
    errno = 0;
    if (std::rename(from.c_str(), to.c_str()) == 0) return true;
    if (errno == 0) errno = e;
    if (why) *why = "rename " + Base(from) + " -> " + Base(to) + ": " + Errno(errno);
    return false;
}

static int64_t FileSize(const std::string& p) {
    FILE* f = std::fopen(p.c_str(), "rb");
    if (!f) return -1;
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fclose(f);
    return n;
}

static bool Fail(const std::string& path, const std::string& w, bool removeTmp, std::string* error) {
    SafeFile::Failures++;
    if (removeTmp) std::remove((path + ".new").c_str());
    Say("FAILED writing " + path + ": " + w + (removeTmp ? " (the previous file is kept)" : ""));
    if (error) *error = "Could not write " + path + " (" + w + ")";
    return false;
}

FILE* SafeFile::BeginWrite(const std::string& path, std::string* error) {
    Writes++;
    const std::string tmp = path + ".new";
    std::string why;
    // 1. the new contents go to <path>.new
    ReleaseHandles(tmp);
    if (!RemoveIfThere(tmp, &why)) { Fail(path, why, false, error); return nullptr; }
    errno = 0;
    FILE* f = std::fopen(tmp.c_str(), "wb");
    if (!f) { FilePool::CloseAll(); errno = 0; f = std::fopen(tmp.c_str(), "wb"); }
    if (!f) { Fail(path, "open " + Base(tmp) + ": " + Errno(errno), false, error); return nullptr; }
    return f;
}

bool SafeFile::EndWrite(const std::string& path, FILE* f, std::string* error, bool writeOk) {
    if (f == nullptr) return false;
    const std::string tmp = path + ".new", bak = path + ".bak";
    std::string why;
    // ... complete and flushed to the card before anything else is touched
    bool ok = writeOk && !std::ferror(f);
    int e = ok ? 0 : (errno ? errno : EIO);
    if (ok && std::fflush(f) != 0) { ok = false; e = errno; }
    long size = ok ? std::ftell(f) : -1;
#ifndef __3DS__
    if (ok && fsync(fileno(f)) != 0 && errno != EINVAL && errno != ENOSYS) { ok = false; e = errno; } // 3DS: FSFILE_Flush
#endif // 3DS: fclose() below commits the file through the FS service (no fsync link dependency)
    if (std::fclose(f) != 0 && ok) { ok = false; e = errno; }
    if (!ok) return Fail(path, "write " + Base(tmp) + ": " + Errno(e) + (e == ENOSPC ? " (SD card full?)" : ""), true, error);
    int64_t got = FileSize(tmp);
    if (got != (int64_t)size) return Fail(path, "size check " + Base(tmp) + ": " + std::to_string(got) + " of " + std::to_string(size) + " bytes", true, error);

    // 2. nothing of ours may hold the file open (the 3DS refuses to delete / rename an open file)
    ReleaseHandles(path);
    ReleaseHandles(bak);

    // 3. the current file becomes the backup (never a rename onto an existing file: FSUSER_RenameFile refuses it)
    bool hadOld = Exists(path);
    if (hadOld) {
        if (!RemoveIfThere(bak, &why)) return Fail(path, why, true, error);
        if (!RenameRetry(path, bak, &why)) return Fail(path, why, true, error);
    }
    // 4. the new file takes its place
    if (!RenameRetry(tmp, path, &why)) {
        std::string w2;
        if (hadOld && !RenameRetry(bak, path, &w2)) why += "; restoring the backup failed too (" + w2 + "), the next start restores it";
        return Fail(path, why, false, error);
    }
    // 5. done: the backup goes (a leftover is harmless: Recover() removes it once <path> validates)
    if (hadOld && std::remove(bak.c_str()) != 0) Say("note: could not remove " + bak + ": " + Errno(errno));
    return true;
}

bool SafeFile::Write(const std::string& path, const void* data, size_t size, std::string* error) {
    FILE* f = BeginWrite(path, error);
    if (f == nullptr) return false;
    bool ok = size == 0 || std::fwrite(data, 1, size, f) == size;
    return EndWrite(path, f, error, ok);
}

// ------------------------------------------------------------------------------------------------- recovery

static uint32_t BE32(const uint8_t* p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
static uint32_t LE32(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static bool TypeLike(const uint8_t* p) {
    for (int i = 0; i < 4; i++) if (p[i] < 0x20 || p[i] > 0x7E) return false;
    return true;
}

bool SafeFile::IffComplete(const std::vector<uint8_t>& d, const char* requiredType) {
    if (d.size() < 64 + 76 || std::memcmp(d.data(), "IFF FILE 2.", 11) != 0) return false;
    std::vector<uint32_t> starts;
    bool hasRequired = requiredType == nullptr;
    size_t off = 64, rsmpAt = 0, rsmpSize = 0;
    while (off + 76 <= d.size()) {
        const uint8_t* h = d.data() + off;
        if (!TypeLike(h)) break; // filler after the last chunk
        uint32_t size = BE32(h + 4);
        if (size < 76) break;
        if (off + size > d.size()) return false; // the last chunk is cut: truncated file
        if (requiredType && std::memcmp(h, requiredType, 4) == 0) hasRequired = true;
        if (std::memcmp(h, "rsmp", 4) == 0) { rsmpAt = off; rsmpSize = size; }
        starts.push_back((uint32_t)off);
        off += size;
    }
    if (starts.empty() || !hasRequired) return false;
    if (off + 8 <= d.size() && TypeLike(d.data() + off) && BE32(d.data() + off + 4) >= 76) return false; // a cut chunk header
    // the resource map lists every chunk: an entry past the end = the file lost its tail at a chunk boundary
    uint32_t mapOff = BE32(d.data() + 60);
    if (mapOff != 0) {
        // (some shipped houses have junk there and no map: House00 / House11 say 960 MB)
        if ((size_t)mapOff + 76 > d.size() && mapOff < (64u << 20)) return false;
        if (rsmpAt == mapOff && rsmpSize >= 76 + 20 && std::memcmp(d.data() + rsmpAt + 76 + 8, "pmsr", 4) == 0) {
            const uint8_t* r = d.data() + rsmpAt + 76;
            size_t n = rsmpSize - 76, p = 20;
            uint32_t version = LE32(r + 4), types = LE32(r + 16);
            for (uint32_t t = 0; t < types && p + 8 <= n; t++) {
                uint32_t count = LE32(r + p + 4);
                p += 8;
                for (uint32_t i = 0; i < count; i++) {
                    if (p + (version == 0 ? 8 : 10) > n) return true; // odd map: the chunk walk decides
                    uint32_t eo = LE32(r + p);
                    if ((size_t)eo + 76 > d.size()) return false;
                    p += version == 0 ? 8 : 10;
                    if (version == 0) { size_t z = p; while (z < n && r[z] != 0) z++; p += ((z - p + 1) + 1) & ~(size_t)1; }
                    else if (p < n) p += 1 + r[p];
                }
            }
        }
    }
    return true;
}

SafeFile::Validator SafeFile::IffValidator(const char* requiredType) {
    std::string req = requiredType ? requiredType : "";
    return [req](const std::vector<uint8_t>& d) { return IffComplete(d, req.empty() ? nullptr : req.c_str()); };
}

bool SafeFile::HasLeftover(const std::string& path) {
    for (const char* s : { ".new", ".tmp", ".bak" }) if (Exists(path + s)) return true;
    return false;
}

SafeFile::Result SafeFile::Recover(const std::string& path, const Validator& valid, std::string* log) {
    auto say = [&](const std::string& l) { Say(l); if (log) *log += l + "\n"; };
    static const char* kinds[] = { ".new", ".tmp", ".bak" }; // newest first: .new / .tmp are complete once <path> moved
    std::vector<uint8_t> data;
    bool present = Exists(path);
    if (present) {
        ReleaseHandles(path);
        if (st::ReadAllBytes(path, data) && valid(data)) {
            for (const char* k : kinds) {
                std::string p = path + k;
                if (Exists(p) && std::remove(p.c_str()) == 0) say("removed the leftover " + p + " (" + Base(path) + " is complete)");
            }
            return Result::Ok;
        }
    }
    for (const char* k : kinds) {
        std::string cand = path + k;
        if (!Exists(cand)) continue;
        std::vector<uint8_t> c;
        ReleaseHandles(cand);
        if (!st::ReadAllBytes(cand, c) || !valid(c)) { say("ignored " + cand + " (incomplete)"); continue; }
        std::string why;
        if (present) { // keep the damaged file for a manual rescue
            std::string dmg = path + ".damaged";
            RemoveIfThere(dmg, nullptr);
            if (!RenameRetry(path, dmg, &why)) { say("cannot move the damaged " + path + " aside: " + why); if (!RemoveIfThere(path, &why)) { say(why); return Result::Damaged; } }
        }
        if (!RenameRetry(cand, path, &why)) {
            say("restore by rename failed (" + why + "): copying");
            if (!Write(path, c)) return present ? Result::Damaged : Result::Missing;
            std::remove(cand.c_str());
        }
        Recovered++;
        say(std::string("RECOVERED ") + path + " from " + Base(cand) + (present ? " (the damaged file is kept as .damaged)" : " (it was missing)") +
            ": an interrupted save");
        for (const char* k2 : kinds) { // older leftovers are superseded now
            std::string p = path + k2;
            if (p != cand && Exists(p) && std::remove(p.c_str()) == 0) say("removed the leftover " + p);
        }
        return Result::Restored;
    }
    if (present) say("DAMAGED: " + path + " is not a complete file and there is no backup");
    return present ? Result::Damaged : Result::Missing;
}

int SafeFile::RecoverDir(const std::string& dir, std::string* log) {
    bool opened = false;
    auto entries = ListDirectory(dir, &opened);
    std::vector<std::string> bases;
    for (auto& e : entries) {
        if (e.IsDir || e.Name.size() < 8) continue;
        std::string ext = e.Name.substr(e.Name.size() - 4);
        if (ext != ".new" && ext != ".tmp" && ext != ".bak") continue;
        std::string base = e.Name.substr(0, e.Name.size() - 4);
        std::string be = base.size() >= 4 ? base.substr(base.size() - 4) : "";
        for (auto& ch : be) ch = (char)tolower((unsigned char)ch);
        if (be != ".iff" && be != ".fam") continue;
        if (std::find(bases.begin(), bases.end(), base) == bases.end()) bases.push_back(base);
    }
    int restored = 0;
    for (auto& b : bases) {
        const char* req = b == "Neighborhood.iff" ? "NBRS" : nullptr;
        if (Recover(dir + "/" + b, IffValidator(req), log) == Result::Restored) restored++;
    }
    return restored;
}

}} // namespace FSO::Files
