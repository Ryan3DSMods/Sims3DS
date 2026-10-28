/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// TS1(sims): writes the in-memory neighbourhood (TS1NeighborhoodProvider) back into Neighborhood.iff, complete
// and in the original layout. The single writer of that file: NhoodModel::Save (front end "Save", leaving a lot with
// "Yes") and VMTS1LotSaver / simsave call it.
//
// Chunks written from memory (every other chunk - XXXX filler, FAMs names, unknown ones - byte for byte):
//   NBRS  every neighbour record: person data (the provider keeps 88 of the 80/256 shorts; the rest stay as saved),
//         relationships; records added at run time (Generic Sims Call 7, ...) are appended as new slots. Records
//         that did not change keep their bytes (incl. the 0xA3 pad bytes FreeSims' Neighbour::Save replaces by 0).
//   NGBH  the 16 neighbourhood data shorts (tutorial state, the expansion "help shown" counters ...) and, for Hot Date
//         and later files, the per-neighbour inventories (Hot Date gifts, Magic Town ingredients ...).
//   TATT  type attributes (field encoded, as the game writes them; FreeSims wrote them uncompressed).
//   FAMI  house, family number, budget, net worth, friends, flags and members patched into the saved record, which
//         keeps its version (7 base game / 9 later); deleted families' FAMI and FAMs are removed.
//   FSPL  FIX(sims): private chunk, the Makin' Magic spells each family has made (GameState.FamilySpells).
// The rsmp resource map is regenerated (IffRawFile). TS1GameState (travel bookkeeping) has no place in the TS1
// file format; it is only valid while the game runs.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace FSO { namespace Content { namespace TS1 { class TS1NeighborhoodProvider; } } }

namespace FSO { namespace Content { namespace TS1 {

class TS1NeighbourhoodWriter {
public:
    /// Writes `path` (normally <prov->UserPath>/Neighborhood.iff). Returns false with *error set.
    static bool Save(TS1NeighborhoodProvider* prov, const std::string& path, const std::vector<int16_t>& deletedFamilies,
                     std::string* error = nullptr, std::vector<std::string>* log = nullptr);
    /// FIX(sims): reads the private "FSPL" chunks (Makin' Magic family spells) into prov->GameState.FamilySpells.
    static void LoadFamilySpells(TS1NeighborhoodProvider* prov, const std::string& path);
};

}}}
