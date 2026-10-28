/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims front end — The Sims 1 UI art: UIGraphics/UIGraphics.far + the Res_*.h / Res_*.RT resource tables.
//
// TS1(sims): FreeSims has no TS1 UI loader (its UI is The Sims Online's .uis scripts + TSO packages), so this
// is new code. How the original organises its art:
//   * Res_Nbhd.h / Res_CPanel.h / Res_Other.h ...  "const Sint32 kName = 5000;" resource ids
//   * Res_Nbhd.RT ...                               "{RLEBMP,kName,"Nbhd/NScreen.BMP"}" id -> file in the FAR
//                                                   (BITMAP / RLEBMP / TGA, optional language column)
//   * UIGraphics.far (FAR v1a)                      the files ("Nbhd\NScreen.BMP": 8-bit RLE8 BMPs, 24-bit BMPs,
//                                                   TGAs), magenta (255,0,255) = transparent
// Positions of the controls were compiled into the executable; a few panels keep theirs in UIText.iff STR#
// tables ("(88;6)" in STR#145 'optionstrs'). Buttons are horizontal strips of 4 states (see UIButton below).
//
// Memory: the archive stays on disk (53 MB); only the manifest is read. Images are decoded on request into a
// 32-bit Canvas and are meant to be scaled down at once and dropped (an 800x600 screen is 1.9 MB decoded).
#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "app/Canvas.h"
#include "app/Input.h"

namespace FSO { namespace Files { namespace FAR1 { class FAR1Archive; } } }

namespace simapp { namespace front {

/// Decodes a Windows BMP (1/4/8/24/32 bpp, BI_RGB / RLE8 / RLE4) or a TGA (types 1,2,3,9,10,11) into a canvas.
/// keyMagenta: (255,0,255) (and the 16-bit-converted (248,0,248)) becomes fully transparent.
bool DecodeImage(const uint8_t* data, size_t size, Canvas& out, bool keyMagenta = true);
inline bool DecodeImage(const std::vector<uint8_t>& d, Canvas& out, bool keyMagenta = true) {
    return !d.empty() && DecodeImage(d.data(), d.size(), out, keyMagenta);
}

/// Area-averaging resample (premultiplied alpha, so keyed edges do not bleed magenta). Any scale factor.
void Resample(const Canvas& src, Canvas& dst, int dw, int dh);
/// Copies a sub-rectangle.
void Crop(const Canvas& src, Canvas& dst, int x, int y, int w, int h);
/// Blends the window of `src` starting at (sx,sy) over the whole of `dst`.
void BlitWindow(Canvas& dst, const Canvas& src, int sx, int sy);
/// Blits `src` with its top-left at (x,y), alpha blended.
inline void BlitImage(Canvas& dst, const Canvas& src, int x, int y) { if (src.Width > 0) dst.Blit(src.Px.data(), src.Width, src.Height, x, y, true); }

class ResGraphics {
public:
    ResGraphics();
    ~ResGraphics();
    /// Opens <dir>/UIGraphics.far and parses the Res_* tables (loose files in <dir>, else inside the FAR).
    bool Open(const std::string& dir);
    bool IsOpen() const { return _Far != nullptr; }

    /// Resource id of a "kName" constant (-1 when unknown).
    int Const(const std::string& name) const;
    /// FAR path of a resource id (the default-language entry), "" when unknown.
    std::string PathFor(int id) const;
    /// Raw bytes of a FAR member ("Nbhd/NScreen.BMP", any case, '/' or '\').
    std::vector<uint8_t> Raw(const std::string& path) const;
    bool Exists(const std::string& path) const;
    /// Loads "kName" (resource constant) or a FAR path.
    bool Load(const std::string& nameOrPath, Canvas& out, bool keyMagenta = true) const;
    /// Loads and scales by `scale` (0 < scale <= 1; dimensions rounded).
    bool LoadScaled(const std::string& nameOrPath, float scale, Canvas& out) const;

    static ResGraphics& Get(); // the app-wide instance (opened by the front end at boot)

private:
    std::unique_ptr<FSO::Files::FAR1::FAR1Archive> _Far;
    std::map<std::string, size_t> _Index;      // normalized path -> index into the far's entries
    std::map<std::string, int> _Consts;        // kName -> id
    std::map<int, std::string> _Paths;         // id -> path
    void ParseHeader(const std::string& text);
    void ParseRT(const std::string& text);
    static std::string Normalize(const std::string& p);
};

/// FreeSims UIButton (SimsVille/UI/Controls/UIButton.cs): a texture strip of ImageStates frames side by side,
/// frame 0 normal, 1 pressed / selected, 2 hover, 3 disabled. Touch has no hover: frame 2 shows while the stylus
/// is down on the button (like a mouse over + down in the original, the "lit" look), 1 for Selected.
class UIButton {
public:
    Canvas Texture;          // the (already scaled) strip
    int ImageStates = 4;
    int X = 0, Y = 0;
    bool Disabled = false;
    bool Selected = false;
    bool Visible = true;
    std::string Id;          // for scripting / dispatch
    std::string Tooltip;

    bool Load(const ResGraphics& ui, const std::string& nameOrPath, float scale, int states = 4);
    int Width() const { return ImageStates > 0 ? Texture.Width / ImageStates : Texture.Width; }
    int Height() const { return Texture.Height; }
    int PadX = 0, PadY = 0;  // FIX(sims) FB1: extra touch area around the art (small buttons on the touch screen)
    Rect Bounds() const { return Rect(X - PadX, Y - PadY, Width() + 2 * PadX, Height() + 2 * PadY); }
    /// Feeds touch input. Returns true on a click (stylus released over the button).
    bool Update(const AppInput& in);
    void Draw(Canvas& c) const;
    bool Pressed() const { return _Down && _Over; }

private:
    bool _Down = false, _Over = false;
};

}}
