/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims app — the original game's UI art.
//
// TS1(sims): The Sims keeps its control panel bitmaps in UIGraphics/UIGraphics.far (cpanel\Buttons\Mood.bmp,
// cpanel\Catalog\BuyFSeating.bmp, Other\QueueCancel.bmp, ...; the layout resources Res_CPanel.RT/.h name them).
// FreeSims reads TSO's uigraphics/*.dat FAR3 files through UIGraphicsProvider (MASK_COLORS = magenta keys); this
// is the TS1 equivalent: FAR1 entries looked up by path (case-insensitive, '/' or '\'), decoded once (BMP via the
// Vitaboy BMP reader, TGA here), magenta (255,0,255)/(254,2,254)/(255,1,255) made transparent like
// UIGraphicsProvider.MASK_COLORS.
// Buttons are horizontal strips of 4 states (normal, highlighted, pressed, disabled — FreeSims UIButton
// ImageStates = 4). The panel is drawn at 3DS resolution, so images are resampled once (box filter) into a small
// cache and blitted from there afterwards (no per-frame scaling).
// Every lookup has a fallback path: when UIGraphics.far is not on the SD card the UI draws Theme.h shapes.
#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "app/Canvas.h"

namespace FSO { namespace Files { namespace FAR1 { class FAR1Archive; } } }

namespace simapp {

enum class ButtonState { Normal = 0, Highlight = 1, Pressed = 2, Disabled = 3 };

class UIGraphics {
public:
    static UIGraphics& Get();
    /// Opens <dataRoot>/UIGraphics/UIGraphics.far (or <dataRoot>/GameData/UIGraphics/...). Safe to call again.
    void Init(const std::string& dataRoot);
    bool Available() const { return _Far != nullptr; }

    /// A decoded image ("cpanel/Buttons/Mood.bmp"), or nullptr. Results (also misses) are cached.
    const Canvas* Image(const std::string& path);
    /// `img` (sub rect `src`, whole image if W == 0) resampled to w x h, cached by key.
    const Canvas* Scaled(const std::string& path, const Rect& src, int w, int h);
    /// Frame `frame` of an `frames`-wide horizontal strip, resampled to w x h (cached).
    const Canvas* Frame(const std::string& path, int frames, int frame, int w, int h);

    /// Draws a 4-state button strip scaled into r. Returns false (draws nothing) if the image is missing.
    bool DrawButton(Canvas& c, const std::string& path, ButtonState st, const Rect& r, int frames = 4);
    /// Like DrawButton but keeps the frame's aspect ratio, centred in r.
    bool DrawButtonFit(Canvas& c, const std::string& path, ButtonState st, const Rect& r, int frames = 4);
    /// Draws an image (or a sub-rect of it) scaled into r. Returns false if missing.
    bool DrawImage(Canvas& c, const std::string& path, const Rect& r, const Rect& src = Rect());

    void ClearScaled() { _Scaled.clear(); }
    /// FIX(sims) memory: frees the decoded UI art (the lot screens' panels, buttons, catalogue frames); it is only
    /// used by the lot UI and decodes again on next use. Called when a lot is left.
    void ReleaseImages() { _Images.clear(); _Scaled.clear(); }

    /// A Windows cursor (Shared\cursors\*.cur: 32x32 monochrome XOR/AND masks) with its hot spot.
    const Canvas* Cursor(const std::string& path, int& hotX, int& hotY);

    /// Decoders shared with the rest of the app (BMP_ chunks of objects/characters use the same rules).
    static bool DecodeBMP(const std::vector<uint8_t>& data, Canvas& out);
    static bool DecodeTGA(const std::vector<uint8_t>& data, Canvas& out);
    /// Box-filter resample (premultiplied alpha) of src (sub rect) into a w x h canvas.
    static void Resample(const Canvas& src, const Rect& srcRect, Canvas& out, int w, int h);

    ~UIGraphics();

private:
    std::unique_ptr<FSO::Files::FAR1::FAR1Archive> _Far;
    std::map<std::string, int> _Index;                    // lower-case path with '/' -> entry index
    std::map<std::string, std::unique_ptr<Canvas>> _Images;
    std::map<std::string, std::unique_ptr<Canvas>> _Scaled;
    struct Cur { std::unique_ptr<Canvas> Img; int HX = 0, HY = 0; };
    std::map<std::string, Cur> _Cursors;
    static std::string Key(const std::string& path);
};

}
