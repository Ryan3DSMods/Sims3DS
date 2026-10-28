/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// TS1(sims): saves the running lot back into its The Sims 1 house file (UserData*/Houses/HouseNN.iff) and the
// lot's Sims into the neighbourhood (Neighborhood.iff NBRS), in the original game's formats.
//
// FreeSO has no TS1 house writer: Simitone stores its own VM snapshot (an "FSOV" chunk) inside the house file and the
// original game cannot read that. FreeSO does have the exact *reader* of the original state:
// VMTS1ActivatorNew.LoadFromIff converts every OBJM record into a VM entity, thread, stack, queue, multitile group and
// avatar (ConvertThread, ConvertAction, ConvertAvatar, ConvertArchitecture). This class is that conversion run
// backwards, member by member (comments name the FreeSO line each field comes from), on top of FreeSO's OBJM record
// layout (files/formats/iff/chunks/OBJMWriter.h) and the ARRY run-length encoding (ARRYWriter.h).
//
// The file is rewritten from the file the lot was loaded from (IffRawFile): every chunk the engine does not own
// (HOUS, altitude, grass, flags, pool, water, unknown chunks, the second thumbnail ...) is copied byte for byte in its
// original order; the rsmp resource map is regenerated. What is written:
//   objt     the object type table (new types appended, ids kept)
//   ObjM     one record per VM entity: position, footprint, attributes, temps, object data, stack (BHAV frames),
//            relationships, tile list / container slots, dynamic sprite flags, multitile and portal data, and for
//            persons motives, person data, interaction queue, object uses and motive deltas
//   Arry 3/103   the per-tile object lists (head of each tile's linked list)
//   Arry 1/101/2/102 (or 11/111/12/112)  floors and walls: tiles the VM changed are encoded, others keep their bytes
//   FLRm/WALm    entries for custom floors/walls that were not in the house before
//   SIMI     the simulation globals (time of day, date, selected Sim, family, budget, ...)
//   BMP_ 512 / 513 the house pictures for the neighbourhood (+ 1024 / 1025 at half size when the file has them;
//   when the caller supplies them: SetPicture, FIX(sims) FB1)
// See src/simantics/STUBS_save.md for what is approximated.
#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include "simantics/_files_contract.h"

namespace FSO { namespace SimAntics { class VM; class VMEntity; class VMAvatar; } }

class VMTS1LotSaver {
public:
    explicit VMTS1LotSaver(FSO::SimAntics::VM* vm) : VM(vm) {}

    struct Stats {
        int Objects = 0, Persons = 0, Kept = 0, Added = 0, Removed = 0, NewTypes = 0;
        int FloorTiles = 0, WallTiles = 0, Contained = 0, StackFrames = 0, Neighbours = 0;
    } LastStats;
    std::vector<std::string> Log;
    std::string Error;

    /// House number (global 10) for the NBRS/save paths; 0 = take it from the VM.
    int16_t HouseNumber = 0;

    /// Optional picture of the lot (24-bit, top-down rows of R,G,B), written as the BMP_ #512 "Thumbnail" the
    /// neighbourhood screen shows (and #513 when the house has one). Width/height should be the original's (264x144).
    /// Size of the house picture already in `housePath` (BMP_ #512; it varies per house), else 264x144.
    /// FIX(sims) FB1: also the THMB 512 anchor (the lot's centre in the picture) when ax / ay are given.
    static void ThumbnailSizeOf(const std::string& housePath, int32_t& width, int32_t& height, int32_t* ax = nullptr, int32_t* ay = nullptr);
    void SetThumbnail(int32_t width, int32_t height, const std::vector<uint8_t>& rgb) { SetPicture(512, width, height, rgb); }
    /// FIX(sims) FB1: BMP_ 512 (no roof) / 513 (roof); the half-size copy (+512: 1024 / 1025) is written too when the
    /// house file has one (Unleashed and later templates).
    void SetPicture(int id, int32_t width, int32_t height, const std::vector<uint8_t>& rgb) { Picture& p = Pictures[id]; p.W = width; p.H = height; p.RGB = rgb; }

    /// Builds the new house file from `sourcePath` (the file the lot was loaded from) and the VM, and writes it to
    /// `destPath` (may be the same file; written atomically). Returns false with Error set.
    bool SaveHouse(const std::string& sourcePath, const std::string& destPath);
    /// Same, into memory.
    bool BuildHouse(const std::string& sourcePath, std::vector<uint8_t>& out);

    /// Into the neighbourhood in memory (TS1NeighborhoodProvider): TS1 "save this Sim's persistent data" for every
    /// person on the lot with a neighbour record (person data 0..79 into its NBRS record; on-lot relationships already
    /// live there) and the lot family's budget into its FAMI. The front end then writes the file (NhoodModel::Save ->
    /// TS1NeighbourhoodWriter, the single writer of Neighborhood.iff).
    bool UpdateNeighbourhood();
    /// UpdateNeighbourhood + TS1NeighbourhoodWriter::Save(neighborhoodPath) (tools; the app goes through NhoodModel).
    bool SaveNeighbours(const std::string& neighborhoodPath);

    /// The house file path of this lot inside the user data folder the neighbourhood provider uses (never a template
    /// or the install): <UserPath>/Houses/HouseNN.iff. Empty without a neighbourhood.
    static std::string UserHousePath(int16_t houseNumber);
    /// TS1(sims): SIMI lot values (what the neighbourhood shows as the house price): the build / buy mode
    /// spending of the session is added to the saved ArchitectureValue / ObjectsValue; Bulldoze sets them.
    int32_t ArchitectureDelta = 0, ObjectsDelta = 0;
    int32_t SetArchitectureValue = -1, SetObjectsValue = -1;

private:
    FSO::SimAntics::VM* VM;
    struct Picture { int32_t W = 0, H = 0; std::vector<uint8_t> RGB; };
    std::map<int, Picture> Pictures; // FIX(sims) FB1
};
