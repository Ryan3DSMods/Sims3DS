/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims — the one crash-safe file writer of the port, and the recovery of interrupted saves at load.
//
// FIX(sims) SAVE: the user's New 3DS lost UserData/Neighborhood.iff after a save ("Could not open
// sdmc:/3ds/sims/Saves/UserData/Neighborhood.iff", neighbourhoods empty on the next start). The port had three
// different "atomic" writers (IffRawFile::Save: tmp + rename old to .bak; RawIff::WriteFileAtomic: tmp + REMOVE the
// target + rename; the caches: remove + rename) and none survived an interruption at every point on the console's
// FAT: the 3DS FS service refuses to delete / rename / open-for-write a file another handle has open, FSUSER_RenameFile
// refuses an existing target (libctru's rename() then deletes the target and renames: two separate steps), and a
// crash (HOME menu ErrDisp) or a power cut between "old file gone" and "new file in place" leaves only <path>.tmp /
// <path>.bak on the card. Nothing read those back.
//
// SafeFile::Write(path) does, in this order (every failure logged with errno, "[save] ..." in log.txt):
//   1. write <path>.new completely, fflush + fsync (FSFILE_Flush) + fclose, check its size;
//   2. close every handle the game itself holds on <path> (IffFile::DetachSource: deferred chunks loaded first, then
//      the FilePool handle);
//   3. remove an old <path>.bak, rename <path> -> <path>.bak (no rename ever targets an existing file);
//   4. rename <path>.new -> <path>; on failure rename <path>.bak back;
//   5. remove <path>.bak.
// At any interruption the card holds a complete file under one of <path>, <path>.new (complete: <path> is only moved
// after it is closed), <path>.bak. SafeFile::Recover(path) at load puts the newest complete one back (also the old
// writers' <path>.tmp / <path>.bak), so the next start finds the old or the new save, never none.
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace FSO { namespace Files {

class SafeFile {
public:
    /// Writes `path` crash-safely (see above). False with *error (and a log line) when the old file is kept.
    static bool Write(const std::string& path, const void* data, size_t size, std::string* error = nullptr);
    static bool Write(const std::string& path, const std::vector<uint8_t>& data, std::string* error = nullptr) {
        return Write(path, data.data(), data.size(), error);
    }
    /// Streaming form (fprintf writers): BeginWrite opens <path>.new; EndWrite flushes, closes and puts it in place
    /// (steps 1-5). writeOk = false (a failed fwrite) drops the new file and keeps the old one.
    static FILE* BeginWrite(const std::string& path, std::string* error = nullptr);
    static bool EndWrite(const std::string& path, FILE* f, std::string* error = nullptr, bool writeOk = true);
    static bool WriteText(const std::string& path, const std::string& text, std::string* error = nullptr) {
        return Write(path, text.data(), text.size(), error);
    }

    /// Whether `data` is a complete IFF file: the header, then chunks that tile the file (a partial chunk at the end
    /// = truncated), every rsmp entry pointing at a chunk inside the file; requiredType (e.g. "NBRS") must be there.
    static bool IffComplete(const std::vector<uint8_t>& data, const char* requiredType = nullptr);
    using Validator = std::function<bool(const std::vector<uint8_t>&)>;
    static Validator IffValidator(const char* requiredType = nullptr);

    enum class Result { Ok, Restored, Missing, Damaged };
    /// Makes sure `path` is a complete file: kept when it validates (leftover .new/.tmp/.bak removed); otherwise the
    /// newest complete <path>.new / <path>.tmp / <path>.bak is put in its place (a damaged <path> is kept as
    /// <path>.damaged). Missing / Damaged: nothing usable (nothing is deleted then). log gets one line per action.
    static Result Recover(const std::string& path, const Validator& valid, std::string* log = nullptr);
    /// Recover() for every IFF in `dir` that has a .new / .tmp / .bak leftover (one directory listing). Returns the
    /// number of files restored.
    static int RecoverDir(const std::string& dir, std::string* log = nullptr);
    /// Cheap probe (no reads): <path>.new / .tmp / .bak exists.
    static bool HasLeftover(const std::string& path);

    static bool Exists(const std::string& path);
    /// Closes every handle the game holds on `path` (IffFile deferred readers, FilePool). Called before a remove/rename.
    static void ReleaseHandles(const std::string& path);
    /// Log sink (default: printf to stdout = log.txt on the 3DS).
    static std::function<void(const std::string&)> Log;
    /// Writes since start (tests).
    static uint64_t Writes, Failures, Recovered;
};

}} // namespace FSO::Files
