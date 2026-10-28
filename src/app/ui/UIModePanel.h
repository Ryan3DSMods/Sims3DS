/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims app — ADDED(sims): the common shape of the four control-panel modes (FreeSims UIUCP switches
// between UILiveMode, UIBuyMode, UIBuildMode and UIOptions panels; on the 3DS each also owns how the stylus /
// cursor acts on the lot while it is active).
#pragma once
#include <string>
#include "app/Canvas.h"
#include "app/Input.h"

namespace simapp {

class LotScreen;

class UIModePanel {
public:
    explicit UIModePanel(LotScreen* owner) : L(owner) {}
    virtual ~UIModePanel() {}
    virtual void Activate() {}
    virtual void Deactivate() {}
    /// Draws the panel into `area` of the bottom screen and registers its hit boxes (LotScreen::Hit).
    virtual void Draw(Canvas& c, const Rect& area) = 0;
    /// A tap on one of the panel's hit boxes. Returns true if handled.
    virtual bool Tap(const std::string& id) { return false; }
    /// Pointer (cursor or stylus in the touch view) acting on the lot, in top-screen pixels.
    virtual void PointerUpdate() {}
    /// Overlays on the top screen (ghost info, tooltips).
    virtual void DrawTop(Canvas& top) {}
    /// Mode-specific buttons (after the global ones). Returns true if the B button was consumed.
    virtual bool Buttons(const AppInput& in) { return false; }
    /// Whether a drag in the touch view scrolls the lot (live mode) or is a pointer drag (tools).
    virtual bool ViewDragScrolls() const { return true; }
    /// The TS1 cursor (UIGraphics Shared\cursors\*.cur) to show over the lot.
    virtual std::string CursorArt() const { return "Shared/cursors/LiveNothing.cur"; }
protected:
    LotScreen* L;
};

}
