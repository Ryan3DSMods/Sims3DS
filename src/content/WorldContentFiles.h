/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims helpers for the wall/floor providers: case-insensitive lookup of TS1 files on disk
// (the original install mixes cases: GameData/floors.iff, GameData/Build.iff, Walls/WallFabric.wll)
// and directory scans for custom *.flr / *.wll content. Replaces FreeSims' FAR1Provider path regexes.
#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <sys/stat.h>
#include "files/FilePool.h"
#if !defined(__3DS__) || defined(ST_HAVE_DIRENT)
#include <dirent.h>
#define ST_WCF_DIRENT 1
#endif

namespace FSO { namespace Content { namespace WorldContentFiles {

inline std::string Lower(std::string s) { for (auto& c : s) c = (char)std::tolower((unsigned char)c); return s; }

inline bool IsDir(const std::string& p) { struct stat st; return stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode); }
inline bool IsFile(const std::string& p) { struct stat st; return stat(p.c_str(), &st) == 0 && !S_ISDIR(st.st_mode); }

/// Entries of a directory (names only), empty when the platform has no dirent.
inline std::vector<std::string> List(const std::string& dir) {
    std::vector<std::string> r;
#ifdef ST_WCF_DIRENT
    DIR* d = opendir(dir.c_str());
    if (!d) return r;
    struct dirent* e;
    while ((e = readdir(d)) != nullptr) {
        std::string n = e->d_name;
        if (n == "." || n == "..") continue;
        r.push_back(n);
    }
    closedir(d);
    std::sort(r.begin(), r.end());
#else
    (void)dir;
#endif
    return r;
}

/// Resolves a relative path ("GameData/floors.iff") under base, matching each component case-insensitively.
/// Returns "" when not found.
inline std::string Find(const std::string& base, const std::string& rel) {
    std::string cur = base;
    size_t start = 0;
    while (start <= rel.size()) {
        size_t slash = rel.find('/', start);
        std::string part = rel.substr(start, (slash == std::string::npos) ? std::string::npos : slash - start);
        std::string direct = cur + "/" + part;
        if (IsFile(direct) || IsDir(direct)) cur = direct;
        else {
            std::string want = Lower(part), hit;
            for (auto& n : List(cur)) if (Lower(n) == want) { hit = n; break; }
            if (hit.empty()) return "";
            cur = cur + "/" + hit;
        }
        if (slash == std::string::npos) break;
        start = slash + 1;
    }
    return cur;
}

/// Files with the given lowercase extension (".flr") directly in dir and one level of subfolders, sorted by name.
/// TS1(sims) hardware pass: typed listings (files/FilePool.h), no stat per entry (~36 ms per directory on the 3DS).
/// install = the read-only install tree (listings remembered in <cache>/dirs.cache); false for save folders.
inline std::vector<std::string> FilesWithExt(const std::string& dir, const std::string& ext, bool recurseOnce, bool install = false) {
    std::vector<std::string> r;
    if (dir.empty()) return r;
    auto entries = install ? FSO::Files::ListInstallDirectory(dir, recurseOnce) : FSO::Files::ListDirectory(dir);
    for (auto& e : entries) {
        std::string p = dir + "/" + e.Name;
        if (e.IsDir) {
            if (recurseOnce) for (auto& m : FilesWithExt(p, ext, false, install)) r.push_back(m);
        } else {
            std::string l = Lower(e.Name);
            if (l.size() > ext.size() && l.compare(l.size() - ext.size(), ext.size(), ext) == 0) r.push_back(p);
        }
    }
    return r;
}

/// Find for the install tree: each component looked up in the remembered listing of its parent (no stat).
inline std::string FindInstall(const std::string& base, const std::string& rel) {
    std::string cur = base;
    size_t start = 0;
    while (start <= rel.size()) {
        size_t slash = rel.find('/', start);
        std::string part = rel.substr(start, (slash == std::string::npos) ? std::string::npos : slash - start);
        std::string want = Lower(part), hit;
        bool exact = false;
        for (auto& e : FSO::Files::ListInstallDirectory(cur)) {
            if (e.Name == part) { hit = e.Name; exact = true; break; }
            if (hit.empty() && Lower(e.Name) == want) hit = e.Name;
        }
        (void)exact;
        if (hit.empty()) return "";
        cur = cur + "/" + hit;
        if (slash == std::string::npos) break;
        start = slash + 1;
    }
    return cur;
}

inline std::string BaseNameNoExt(const std::string& path) {
    size_t s = path.find_last_of("/\\");
    std::string n = (s == std::string::npos) ? path : path.substr(s + 1);
    size_t dot = n.find('.');
    return (dot == std::string::npos) ? n : n.substr(0, dot);
}

}}}
