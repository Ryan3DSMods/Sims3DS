/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
#include "app/PieMenu.h"
#include "app/Font.h"
#include "app/Theme.h"
#include <cmath>
#include <algorithm>
#include <cctype>

namespace simapp {

static const int CENTER_R = 17;

void PieMenu::Open(const std::vector<PieChoice>& pie, const std::string& title, const Rect& area) {
    Root.reset(new Node());
    Root->Category = true;
    // UIPieMenu constructor: split names on '/', build the category tree (C# Dictionary keeps insertion order)
    for (auto& item : pie) {
        std::vector<std::string> depth;
        std::string name = ToLatin1(item.Name);
        size_t p = 0;
        while (true) {
            size_t e = name.find('/', p);
            if (e == std::string::npos) { depth.push_back(name.substr(p)); break; }
            depth.push_back(name.substr(p, e - p));
            p = e + 1;
        }
        Node* category = Root.get();
        for (size_t j = 0; j + 1 < depth.size(); j++) {
            Node* next = category->Find(depth[j]);
            if (next == nullptr) {
                std::unique_ptr<Node> n(new Node());
                n->Category = true; n->Name = depth[j]; n->Parent = category;
                next = n.get();
                category->Children.push_back(std::move(n));
            }
            category = next;
        }
        if (category->Find(depth.back()) == nullptr) {
            std::unique_ptr<Node> n(new Node());
            n->Name = depth.back(); n->Choice = item; n->Parent = category;
            category->Children.push_back(std::move(n));
        }
    }
    Current = Root.get();
    Title = ToLatin1(title);
    Area = area;
    IsOpen = true;
    Highlight = -1;
    Layout();
}

void PieMenu::Layout() {
    ItemRects.clear();
    CX = Area.X + Area.W / 2;
    CY = Area.Y + Area.H / 2 + 6;
    int n = (int)Current->Children.size();
    if (n == 0) return;
    const int h = 19;
    if (n <= 10) {
        // radial: items on an ellipse around the centre, first item at the top (TS1 / UIPieMenu.RenderMenu)
        float rx = (n <= 4) ? 70.0f : (n <= 6 ? 92.0f : 100.0f), ry = (n <= 4) ? 50.0f : 74.0f;
        if (n <= 2) ry = 40.0f;
        for (int i = 0; i < n; i++) {
            float ang = -1.5707963f + 6.2831853f * i / n;
            int px = CX + (int)std::lround(std::cos(ang) * rx);
            int py = CY + (int)std::lround(std::sin(ang) * ry);
            auto& node = Current->Children[(size_t)i];
            std::string label = node->Name + (node->Category ? " >" : "");
            int w = std::min(150, TextWidth(FontId::Bold, label) + 14);
            int x = px - w / 2;
            // items on the left/right side hang away from the centre
            float c = std::cos(ang);
            if (c > 0.2f) x = px - 12;
            else if (c < -0.2f) x = px - w + 12;
            x = std::max(Area.X + 2, std::min(Area.Right() - w - 2, x));
            int y = std::max(Area.Y + 18, std::min(Area.Bottom() - h - 2, py - h / 2));
            ItemRects.push_back(Rect(x, y, w, h));
        }
    } else {
        // many items: two columns
        int cols = 2, rows = (n + 1) / 2;
        int colW = (Area.W - 12) / cols;
        int top = Area.Y + 20;
        int rowH = std::min(h + 2, (Area.H - 24) / std::max(1, rows));
        for (int i = 0; i < n; i++) {
            int col = i / rows, row = i % rows;
            ItemRects.push_back(Rect(Area.X + 4 + col * (colW + 4), top + row * rowH, colW, rowH - 2));
        }
        CX = -100; // no centre button in list mode (B / tap outside goes back)
    }
}

PieMenu::Result PieMenu::Back() {
    if (Current != nullptr && Current->Parent != nullptr) {
        Current = Current->Parent;
        Highlight = -1;
        Layout();
        return Result::Navigated;
    }
    Close();
    return Result::Closed;
}

PieMenu::Result PieMenu::Activate(int index, PieChoice& out) {
    if (index < 0 || index >= (int)Current->Children.size()) return Result::None;
    Node* node = Current->Children[(size_t)index].get();
    if (node->Category) {
        Current = node;
        Highlight = -1;
        Layout();
        return Result::Navigated;
    }
    out = node->Choice;
    Close();
    return Result::Chosen;
}

PieMenu::Result PieMenu::Tap(int x, int y, PieChoice& out) {
    if (!IsOpen) return Result::None;
    for (size_t i = 0; i < ItemRects.size(); i++) {
        Rect r = ItemRects[i];
        r.X -= 2; r.Y -= 2; r.W += 4; r.H += 4;
        if (r.Contains(x, y)) return Activate((int)i, out);
    }
    int dx = x - CX, dy = y - CY;
    if (dx * dx + dy * dy <= (CENTER_R + 4) * (CENTER_R + 4)) return Back();
    // tap anywhere else closes (UILotControl: a click while the pie is open removes it)
    Close();
    return Result::Closed;
}

PieMenu::Result PieMenu::Button(int dx, int dy, bool a, bool b, PieChoice& out) {
    if (!IsOpen) return Result::None;
    int n = (int)ItemRects.size();
    if (b) return Back();
    if (a) return (Highlight >= 0) ? Activate(Highlight, out) : Result::None;
    if ((dx != 0 || dy != 0) && n > 0) {
        if (Highlight < 0) Highlight = 0;
        else Highlight = (Highlight + (dx + dy > 0 ? 1 : n - 1)) % n;
    }
    return Result::None;
}

void PieMenu::Draw(Canvas& c, const Canvas* icon) {
    if (!IsOpen) return;
    c.Dim(Area.X, Area.Y, Area.W, Area.H, 110);
    // title
    if (!Title.empty()) {
        std::string t = Ellipsize(FontId::Bold, Title, Area.W - 8);
        int w = TextWidth(FontId::Bold, t);
        c.FillRoundRect(Area.X + (Area.W - w) / 2 - 6, Area.Y + 1, w + 12, 16, 6, theme::PanelDark);
        DrawText(c, FontId::Bold, Area.X + (Area.W - w) / 2, Area.Y + 1, t, theme::TextBright);
    }
    // spokes + centre (the C# pie draws the Sim's head in the middle)
    if (CX > 0) {
        for (auto& r : ItemRects) c.Line(CX, CY, r.X + r.W / 2, r.Y + r.H / 2, RGBA(150, 190, 220, 110));
        c.FillCircle(CX, CY, CENTER_R, theme::PieCentre);
        c.StrokeCircle(CX, CY, CENTER_R, theme::PieEdge);
        if (icon != nullptr && icon->Width > 0) {
            Rect ir(CX - 13, CY - 13, 26, 26);
            c.BlitScaled(icon->Px.data(), icon->Width, icon->Height, ir, true);
        } else if (Current != nullptr && Current->Parent != nullptr) {
            DrawTextCentered(c, FontId::Bold, Rect(CX - CENTER_R, CY - CENTER_R, CENTER_R * 2, CENTER_R * 2), "<", theme::TextBright);
        } else { // a Sim or the floor: the plumbob
            c.FillTriangle(CX - 6, CY, CX + 6, CY, CX, CY - 11, theme::Plumbob);
            c.FillTriangle(CX - 6, CY, CX + 6, CY, CX, CY + 11, theme::PlumbobDark);
        }
    }
    for (size_t i = 0; i < ItemRects.size(); i++) {
        auto& r = ItemRects[i];
        auto& node = Current->Children[i];
        bool hl = (int)i == Highlight;
        c.FillRoundRect(r, 7, hl ? theme::PieItemHi : theme::PieItem);
        c.StrokeRoundRect(r.X, r.Y, r.W, r.H, 7, theme::PieEdge);
        std::string label = node->Name + (node->Category ? " >" : "");
        label = Ellipsize(FontId::Bold, label, r.W - 8);
        DrawTextCentered(c, FontId::Bold, r, label, hl ? theme::PieTextHi : theme::PieText);
    }
}

static std::string Lower(const std::string& s) {
    std::string r = s;
    for (auto& ch : r) ch = (char)std::tolower((unsigned char)ch);
    return r;
}

bool PieMenu::FindItem(const std::string& text, int& x, int& y) const {
    if (!IsOpen || Current == nullptr) return false;
    std::string t = Lower(text);
    for (size_t i = 0; i < Current->Children.size() && i < ItemRects.size(); i++) {
        if (Lower(Current->Children[i]->Name).find(t) != std::string::npos) {
            x = ItemRects[i].X + ItemRects[i].W / 2; y = ItemRects[i].Y + ItemRects[i].H / 2;
            return true;
        }
    }
    return false;
}

std::vector<std::string> PieMenu::VisibleLabels() const {
    std::vector<std::string> r;
    if (!IsOpen || Current == nullptr) return r;
    for (auto& n : Current->Children) r.push_back(n->Name + (n->Category ? "/..." : ""));
    return r;
}

}
