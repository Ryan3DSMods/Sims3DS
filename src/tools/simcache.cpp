/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// simcache — prebuilds the sims cache folder on a PC, in the exact format the 3DS reads, so the console skips
// the slow first boot (object index of every archive) and the first Buy mode (catalogue scan).
//
//   simcache <installRoot> <cacheDir>
//
// installRoot is the same tree that goes to sdmc:/3ds/sims/ (GameData/, ExpansionPack*/, Downloads/...).
// Copy the resulting files to sdmc:/3ds/sims/cache/:
//   objects.cache        GUID -> archive/file index of every object (STOC3: archive paths relative to the install)
//   catalog.cache        Buy/Build catalogue (OBJD flags, prices, names)
//   wallfloorwll.cache   expansion-pack walls, wallfloorflr.cache  expansion-pack floors (catalogue strings)
//   cmxnames.cache       animations / suits / skeletons defined by the loose skins (Skins/, SkinsBuy/ *.cmx)
//   avatars.cache        the avatar file index (22k Sim files in the archives and Skins folders) - TS1(sims) hardware pass
//   bcfnames.cache       animation / suit / skeleton names of every AnimationOut.ndx (+ the loose skins)
//   dirs.cache           directory listings of the install (Downloads/...): no directory scans on the SD card
//   cache_version.txt    the version stamp the app checks (a stale stamp deletes the caches and rebuilds them)
// All are text files: nothing depends on endianness or pointer size. They are only valid for the same install
// (the archive list must match: the app compares it and rebuilds silently when it differs).
#include "app/App.h"
#include "app/buy/UICatalog.h"
#include "content/ContentManager.h"
#include "vitaboy/content/TS1AvatarProvider.h"
#include "files/FilePool.h"
#include "files/LoadProgress.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/stat.h>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::printf("usage: simcache <installRoot> <cacheDir>\n");
        return 2;
    }
    std::string root = argv[1], cache = argv[2];
    mkdir(cache.c_str(), 0777);
    for (const char* n : { "/objects.cache", "/catalog.cache", "/wallfloorwll.cache", "/wallfloorflr.cache", "/cmxnames.cache", "/avatars.cache", "/bcfnames.cache", "/dirs.cache" }) std::remove((cache + n).c_str());
    auto t0 = std::chrono::steady_clock::now();
    auto ms = [&]() { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count(); };
    FSO::Files::LoadProgress::StepSink = [&](const std::string& s) { std::printf("[%7.0f ms] %s\n", ms(), s.c_str()); };
    try {
        FSO::Content::Content::Init(root, cache); // objects.cache, wallfloor*.cache
        simapp::UICatalog::Get().Init(cache);     // catalog.cache
        FSO::Content::TS1::TS1AvatarContent::SetBasePath(root);
        FSO::Content::TS1::TS1AvatarContent::Get()->EnsureInit(); // cmxnames/avatars/bcfnames.cache
        FSO::Files::InstallDirCache::Save();                       // dirs.cache (incl. what the avatar index listed)
    } catch (std::exception& e) {
        std::printf("simcache: failed: %s\n", e.what());
        return 1;
    }
    if (FILE* f = std::fopen((cache + "/cache_version.txt").c_str(), "w")) { std::fprintf(f, "%d\n", simapp::App::CacheVersion()); std::fclose(f); }
    auto c = FSO::Content::Content::Get();
    auto& io = FSO::Files::FilePool::Stats;
    std::printf("simcache: %zu objects in %zu archives; %.1f MB read, %llu seeks, %llu opens (est. %.0f s on a 3DS SD card)\n",
                c->WorldObjects->Entries.size(), c->WorldObjects->Archives.size(), io.BytesRead / 1048576.0,
                (unsigned long long)io.Seeks, (unsigned long long)io.Opens, io.EstimatedSdMs() / 1000.0);
    std::printf("copy %s/{objects.cache,catalog.cache,wallfloorwll.cache,wallfloorflr.cache,cmxnames.cache,avatars.cache,bcfnames.cache,dirs.cache,cache_version.txt} to sdmc:/3ds/sims/cache/\n", cache.c_str());
    return 0;
}
