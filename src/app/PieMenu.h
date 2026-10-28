/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims app — the pie menu (FreeSims UI/Panels/UIPieMenu.cs): interactions whose names contain '/'
// become categories ("Ask/About Work"), items are laid out around a centre, picking a category descends,
// the centre goes back up (or closes at the root). Drawn modal on the 320x240 touch screen.
#pragma once
#include <string>
#include <vector>
#include <memory>
#include "app/Canvas.h"
#include "app/LotSession.h"

namespace simapp {

class PieMenu {
public:
    enum class Result { None, Closed, Chosen, Navigated };

    void Open(const std::vector<PieChoice>& choices, const std::string& title, const Rect& area);
    void Close() { IsOpen = false; Root.reset(); Current = nullptr; }
    bool IsOpen = false;

    /// Handles a tap at screen (x, y). On Result::Chosen, `out` is the chosen interaction.
    Result Tap(int x, int y, PieChoice& out);
    /// Button navigation: d-pad moves the highlight, A picks, B goes back/closes.
    Result Button(int dx, int dy, bool a, bool b, PieChoice& out);

    void Draw(Canvas& c, const Canvas* icon);

    /// Centre of the visible item whose label contains `text` (case-insensitive), for the host script.
    bool FindItem(const std::string& text, int& x, int& y) const;
    std::vector<std::string> VisibleLabels() const;
    std::string Title;

private:
    struct Node {
        std::string Name;
        bool Category = false;
        PieChoice Choice;
        Node* Parent = nullptr;
        std::vector<std::unique_ptr<Node>> Children;
        Node* Find(const std::string& n) { for (auto& c : Children) if (c->Name == n) return c.get(); return nullptr; }
    };
    std::unique_ptr<Node> Root;
    Node* Current = nullptr;
    Rect Area;
    int CX = 0, CY = 0;
    std::vector<Rect> ItemRects;
    int Highlight = -1;
    void Layout();
    Result Activate(int index, PieChoice& out);
    Result Back();
};

}
