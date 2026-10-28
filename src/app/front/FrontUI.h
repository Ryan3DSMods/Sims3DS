/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims front end — shared widgets drawn with the original art: 9-slice panels (NScreenDialogTiler /
// HotspotPopupTiler), the generic TS1 text button (Shared/Sys/WinBtn.BMP, 3-sliced like FreeSims UIButton with
// Width set), and the modal message box of the neighbourhood screens (EvictModeStrs / MoveInModeStrs dialogs).
#pragma once
#include <functional>
#include <string>
#include <vector>
#include "app/Canvas.h"
#include "app/Input.h"
#include "app/front/ResGraphics.h"
#include "app/front/TS1Font.h"

namespace simapp { namespace front {

/// TS1 UI colours (text colours of the original neighbourhood/option panels).
namespace col {
    static const uint32_t Text      = 0xFFFFFFFFu;            // white
    static const uint32_t TextBlue  = 0xFFF0E0A0u;            // light blue (R A0 G E0 B F0)
    static const uint32_t TextGold  = 0xFF60D8FFu;            // gold/yellow (FF D8 60)
    static const uint32_t TextDim   = 0xFFC8A890u;            // (90 A8 C8)
    static const uint32_t Shadow    = 0xC0200800u;            // dark navy shadow
    static const uint32_t Navy      = 0xFF500A00u;            // (00 0A 50) panel blue
    static const uint32_t Highlight = 0xFFFFFF00u;            // cyan (00 FF FF)
}

/// Stretches a tile image over r keeping a `border`-pixel frame (corners unscaled).
void NineSlice(Canvas& dst, const Canvas& tile, const Rect& r, int border);

/// Shared art, loaded once (scaled for the touch screen where noted).
struct FrontArt {
    Canvas DialogTiler;   // Nbhd/NScreenDialogTiler.TGA (42x42)
    Canvas PopupTiler;    // Nbhd/HotspotPopupTiler.TGA (111x111)
    Canvas WinBtn;        // Shared/Sys/WinBtn.BMP 4 frames (65x33 each), scaled 0.6
    Canvas CheckBtn;      // Shared/Sys/CheckBtnNew.BMP 6 frames (28x28)
    bool Loaded = false;
    void Load(const ResGraphics& ui);
    static FrontArt& Get();
};

/// A text button on the WinBtn art. Frame 0 normal, 1 pressed, 2 lit, 3 disabled (FreeSims UIButton).
struct TextButton {
    Rect R;
    std::string Label, Id;
    bool Disabled = false, Selected = false;
    bool Update(const AppInput& in); // true on click
    void Draw(Canvas& c) const;
private:
    bool _Down = false, _Over = false;
};

/// Modal message box (title, message, 1-3 buttons) drawn on the touch screen.
class FrontDialog {
public:
    bool Open = false;
    std::string Title, Message;
    std::vector<TextButton> Buttons; // ids "0","1","2"
    std::function<void(int)> OnResult;

    void Show(const std::string& title, const std::string& message, const std::vector<std::string>& buttons, std::function<void(int)> onResult);
    /// Feeds input; returns true when the dialog consumed it.
    bool Update(const AppInput& in);
    void Draw(Canvas& c);
    bool ButtonPos(int index, int& x, int& y) const;
};

}}
