/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// ADDED(sims): the "Saving..." overlay shown while a lot is saved (Options "Save", "Yes" in the "Save?" / "Quit?"
// dialogs when leaving, the save before a trip). TS1 has no saving screen of its own (no string for it in UIText.iff;
// the game just blocked until the files were written), but on the 3DS a save takes several seconds, so the app shows
// one, drawn like the front end's dialogs (Nbhd/NScreenDialogTiler, the variablesans fonts): the top screen keeps the
// lot visible, dimmed, with a "Saving..." panel; the touch screen shows the progress bar, the percentage and the
// current step. Driven by the save steps' progress reports (files/LoadProgress.h) from App (front/AppFlow.cpp).
#pragma once
#include <string>
#include "app/Canvas.h"

namespace simapp { namespace front {

class SaveOverlay {
public:
    /// Takes the screens as they are now (the lot and its panel) and composes everything that does not change:
    /// both screens dimmed, the top screen's panel with "Saving..." and `lotName` (may be empty), the touch screen's
    /// panel. Redraws then only copy these and draw the percentage, the step and the bar (cheap on the 3DS).
    void Begin(const Canvas& top, const Canvas& bottom, const std::string& lotName);
    /// Both screens: "Saving house: 45%", `step` ("Drawing house picture"), the bar at `fraction`.
    void Draw(Canvas& top, Canvas& bottom, const std::string& step, float fraction) const;
    /// Frees the composed screens.
    void End();
private:
    Canvas _Top, _Bottom;
};

}}
