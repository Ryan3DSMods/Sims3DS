/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims front end — the sub screens the neighbourhood opens: Create-A-Sim (owned by src/app/cas/) and the
// credits.
//
// Create-A-Sim: simapp::cas::CASScreen (src/app/cas/CASScreen.h, written separately) is run as a sub screen when
// its header is present: CASScreen(Config{DataRoot, UserDataDir = the neighbourhood's save folder}), then
// Update(in, dt) + Draw(top, bottom) every frame until Done() (the family is saved by CAS itself into
// Neighborhood.iff + Characters/) or Cancelled(). The front end then re-opens the neighbourhood
// (NhoodModel::Switch: provider InitSpecific + LoadCharacters) and shows the family bin with the new family.
// Without the header a placeholder says Create-A-Sim is not in this build. CASBridge.cpp is the only file that
// touches src/app/cas.
#pragma once
#include <memory>
#include <string>
#include "app/front/FrontEnd.h"

namespace simapp { namespace front {

std::unique_ptr<SubScreen> MakeCASScreen(const std::string& dataRoot, const std::string& userDir, int neighbourhood);
/// True when this build has the Create-A-Sim screen.
bool HaveCASScreen();
/// The scrolling credits (STR#163 'credits'), from the logo button of the neighbourhood screen.
std::unique_ptr<SubScreen> MakeCreditsScreen();

}}
