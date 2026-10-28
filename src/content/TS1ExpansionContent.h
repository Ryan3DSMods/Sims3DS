/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// TS1(sims): expansion pack content that lives inside the expansion .far archives rather than loose in GameData.
//
// The Complete Collection ships the expansion walls and floors (ExpansionPack*/ExpansionPack*.far, ExpansionShared,
// Deluxe: "WallsAndFloors\*.wll|.flr", 457 files, ~29 MB). FreeSO's TS1Provider reads every .far of the install; this
// port indexes them through WorldObjectProvider::Archives (already open) and, to keep the 3DS memory/SD budget:
//   * the catalog strings (STR# 0: name, price, description) are read once and cached in <CacheDir>/wallfloor.cache,
//   * an archive entry is only read (and kept) when a lot or the catalog draws that wall/floor.
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace FSO { namespace Files { namespace Formats { namespace IFF { class IffFile; } } } }

namespace FSO { namespace Content {
class Content;
namespace TS1 {

class TS1ExpansionContent {
public:
    struct ArchiveItem {
        int32_t Far = -1;          // WorldObjectProvider::Archives index
        std::string Entry;         // entry name in the far ("WallsAndFloors\floor_castle1.flr")
        std::string Key;           // lowercase file name without folder/extension ("floor_castle1")
        std::string Name, Description;
        int32_t Price = 0;
    };
    /// Every expansion archive entry with the given lowercase extension (".wll" / ".flr"), in archive order
    /// (Objects.far first, then ExpansionPack .. 7, ExpansionShared, Deluxe, Downloads), catalog strings filled.
    static std::vector<ArchiveItem> ListArchiveItems(Content* content, const std::string& ext);
    /// "far:<index>:<entry>" -> the iff (null if the reference is not a far one or cannot be read).
    static std::unique_ptr<FSO::Files::Formats::IFF::IffFile> LoadArchiveIff(Content* content, const std::string& reference);
    static std::string MakeReference(const ArchiveItem& item);
    static bool IsArchiveReference(const std::string& reference) { return reference.compare(0, 4, "far:") == 0; }
};

}}}
