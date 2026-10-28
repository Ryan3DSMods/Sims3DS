/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims app — the options mode panel (FreeSims UI/Panels/UIOptions.cs, TS1's "Options" mode) together with
// TS1's view controls (rotate, zoom, walls up / cutaway / down, first / second story — cpanel\Buttons\rotleft.bmp,
// zoomin.bmp, nowall.bmp, nocut.bmp, dynacut.bmp, Lev1.BMP, Lev2.BMP) and the game buttons (neighbourhood, quit;
// cpanel\Buttons\OptNbhd.bmp, OptExit.bmp). Saving is not ported (the Save button is shown disabled).
#pragma once
#include "app/ui/UIModePanel.h"

namespace simapp {

class UIOptionsPanel : public UIModePanel {
public:
    explicit UIOptionsPanel(LotScreen* owner) : UIModePanel(owner) {}
    void Draw(Canvas& c, const Rect& area) override;
    bool Tap(const std::string& id) override;
    void PointerUpdate() override;
    std::string CursorArt() const override { return "Shared/cursors/Arrow.cur"; }
};

}
