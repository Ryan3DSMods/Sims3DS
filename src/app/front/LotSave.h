/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// TS1(sims): "Save" of a lot from the app — the house file (VMTS1LotSaver) with a fresh neighbourhood picture,
// and the lot's Sims / money into the in-memory neighbourhood (the front end's NhoodModel::Save then writes
// Neighborhood.iff through content/TS1NeighbourhoodWriter). Called by AppFlow for Options "Save" and for "Yes" in the
// "Save?" / "Quit?" dialogs when leaving a lot. See src/simantics/STUBS_save.md.
#pragma once
#include <string>

namespace simapp { class LotSession; }

namespace simapp { namespace front {

/// Writes <UserData>/Houses/HouseNN.iff (never the install or a template) and updates the neighbourhood in memory.
/// Returns false with `error` set; `log` gets the saver's notes.
bool SaveLot(LotSession& session, std::string& error, std::string& log);

/// TS1(sims): "Bulldoze" of an unoccupied house in the neighbourhood: loads the lot, removes every object on the
/// buildable area except the mailbox and the outdoor trash can (what TS1 leaves on an empty lot), all walls, floors
/// and pools of the buildable area (the terrain, the street and the sidewalk stay), and saves it as an empty lot
/// (SIMI ArchitectureValue = 0: the neighbourhood then offers it as "empty lot"). Writes the user house file only.
bool BulldozeLot(int houseNumber, std::string& error, std::string& log);

}}
