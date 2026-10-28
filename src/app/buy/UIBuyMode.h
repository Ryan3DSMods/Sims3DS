/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims app — FreeSims UI/Panels/UIBuyMode.cs: the buy mode panel — catalogue sorted by function (Seating,
// Surfaces, Decorative, Electronics, Appliances, Plumbing, Lighting, Miscellaneous) or by room (Living Room, Dining
// Room, Bedroom, Study, Kitchen, Bathroom, Outside, Miscellaneous) with TS1's sub-sort buttons, pages of
// catalogue thumbnails (UICatalog / UICatalogItem), and the object holder for placing, moving and selling.
// ADDED(sims): picking an item switches the panel to a touch view of the lot (drag the stylus to move the
// ghost, tap it again or press the check button to buy, rotate buttons), the query panel (name, price,
// description, motive ratings) goes to the top screen next to the ghost; the hand tool picks objects up from the
// touch view or with the cursor + A.
#pragma once
#include <string>
#include <vector>
#include "app/ui/UIModePanel.h"
#include "app/buy/UICatalog.h"

namespace simapp {

class UIBuyMode : public UIModePanel {
public:
    explicit UIBuyMode(LotScreen* owner);
    void Activate() override;
    void Deactivate() override;
    void Draw(Canvas& c, const Rect& area) override;
    bool Tap(const std::string& id) override;
    void PointerUpdate() override;
    void DrawTop(Canvas& top) override;
    bool Buttons(const AppInput& in) override;
    bool ViewDragScrolls() const override;
    std::string CursorArt() const override;

    bool RoomCategories = false;
    int Category = -1;   // 0..7 (UI order), -1 = none open
    int Subsort = -1;    // -1 all, 0..3, 4 other
    int Page = 0;
    bool HandView = false;

    /// Selects catalogue entry `index` of the current list (C# Catalog_OnSelectionChange): starts holding it.
    bool Select(int index);
    /// Draws a page of catalogue items (C# UICatalog.SetPage + UICatalogItem), hit ids "<prefix><index>".
    /// Returns the number of pages.
    static int DrawCatalogPage(LotScreen* L, Canvas& c, const Rect& r, const std::vector<const UICatalogElement*>& items,
                               int page, int selected, const std::string& prefix, int budget);
    static int PageSize(const Rect& r);
    /// The holding toolbar shared with build mode (rotate, place, sell/cancel) + item info.
    static void DrawHoldToolbar(LotScreen* L, Canvas& c, const Rect& r, bool canPlace);
    /// TS1-style query panel on the top screen (name, price, description, ratings).
    static void DrawQueryPanel(LotScreen* L, Canvas& top, const UICatalogElement* e, int price, bool sell);

    std::vector<const UICatalogElement*> Current;
    int Selection = -1;

private:
    Rect _GridRect;
    int _Pages = 1;
    // touch placement: second tap on the ghost's tile buys
    int _LastTileX = -9999, _LastTileY = -9999;
    bool _TapPlaceArmed = false;
    void Refresh();
};

}
