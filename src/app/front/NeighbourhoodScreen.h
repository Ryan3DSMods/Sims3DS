/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims front end — the neighbourhood screen (and Downtown / Vacation Island / Studio Town / Magic Town).
//
// TS1(sims) — FreeSims has no TS1 neighbourhood view (SimsVille/UI/Model/Neighborhood.cs is a TSO-style
// city renderer with its own content). This is the original screen rebuilt from its art and data:
//   * the map (Community/NScreen_unleashed.bmp or Nbhd/NScreen.BMP, the EP area maps) with its animation
//     layers (waves: NScreen_unleashed_waves001-006, dscreen00-04, visland_waves, Magicland waves),
//   * the lots: the house pictures saved in each HouseNN.iff (BMP_ 1024, or 512 at half size), placed at the
//     LotLocations.iff coordinates (top-left of the picture on the 800x600 map),
//   * the banner (NghUI/banner_neighborhood.bmp) with the NghUI buttons and their STR#151 tooltips,
//   * the lot popup (Nbhd/HotSpotPopupBkg.bmp + its alpha) with the STR#134 rollover texts,
//   * move in / evict / bulldoze modes and their STR#131/#132 dialogs, the family bin (FamilyPicker).
//
// Dual screen: touch screen = the whole 800x600 screen at 0.4 (map, banner and buttons where the original has
// them; the top screen's view is outlined on it); top screen = the map at its native 1:1 scale around the
// selected lot, with the popup and tooltips (a close-up "monitor" of the neighbourhood).
#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "app/Canvas.h"
#include "app/Input.h"
#include "app/front/FamilyPicker.h"
#include "app/front/FrontUI.h"
#include "app/front/NhoodModel.h"
#include "app/front/ResGraphics.h"

namespace simapp { namespace front {

class FrontEnd;

class NeighbourhoodScreen {
public:
    explicit NeighbourhoodScreen(FrontEnd* fe);
    /// Loads an area (art + lots). Neighbourhood switching goes through FrontEnd/NhoodModel first.
    void Enter(Area a);
    Area CurrentArea() const { return _Area; }
    void Frame(const AppInput& in, double dtMs, Canvas& top, Canvas& bottom);
    /// Rebuilds the lot layer after the families changed.
    void RefreshLots();
    void OpenImport(); // TS1(sims): Import Family (.FAM)
    void SelectHouse(int house);
    /// TS1 lot selection dialogs (types 5/7/8/11/13, "Where would you like to go?"): the area's map in pick mode;
    /// a community lot tapped twice (or A) calls done(house), Return / B calls done(-1).
    void BeginPick(Area a, const std::string& title, std::function<void(int)> done);
    bool Picking() const { return (bool)_PickDone; }
    /// The touch screen without overlays (the lot loading screen dims it).
    void RedrawBottom(Canvas& bottom) { DrawBottom(bottom); }
    /// TS1(sims): frees the map art (~8 MB) while a lot is played; Enter() rebuilds it.
    void ReleaseArt() {
        _Base = Canvas(); _MapTop = Canvas(); _MapBottom = Canvas(); _OpenPic = Canvas(); _OpenHouse = -1;
        std::vector<Canvas>().swap(_Over); std::vector<Spans>().swap(_AnimTop); std::vector<Spans>().swap(_AnimBottom);
        _HasMap = false;
    }
    int SelectedHouse() const;
    /// FIX(sims) SAVE: neighbourhood `index` has no usable Neighborhood.iff: explain it and offer to copy that one
    /// neighbourhood again from the install (UserDataSetup::RestoreFromInstall); never an empty neighbourhood.
    void OfferRestore(int index);

    // ---- scripting ----
    bool LotPos(int house, int& x, int& y) const;
    bool ButtonPos(const std::string& id, int& x, int& y);
    std::string Describe() const;

    FrontDialog Dialog;
    FamilyPicker Picker;

private:
    FrontEnd* _FE;
    Area _Area = Area::Neighbourhood;
    struct Spans {
        int W = 0;
        std::vector<uint32_t> Start;
        std::vector<uint16_t> Len;
        std::vector<uint32_t> Px;
        void Build(const Canvas& c, int ox, int oy, int w, int h); // runs of non-transparent pixels of c placed at (ox,oy)
        void Draw(Canvas& dst, int camX, int camY) const;          // dst window starts at (camX, camY) in map space
    };
    Canvas _Base;                    // 800x600 map with the under layers
    std::vector<Canvas> _Over;       // layers above the lots
    Canvas _MapTop, _MapBottom;      // + lots (1:1 and 0.4)
    Canvas _OpenPic;                 // FIX(sims) FB1: the selected house without its roof (BMP_ 512)
    int _OpenHouse = -1;
    std::vector<Spans> _AnimTop, _AnimBottom;
    int _AnimMs = 250;
    Canvas _Banner, _Popup, _CurrentBtn;
    std::vector<UIButton> _Buttons;
    bool _HasMap = false;

    int _Sel = -1;                   // index into Model().Lots
    float _CamX = 200, _CamY = 180;  // top-left of the top screen's window into the map
    bool _ManualCam = false;
    enum class Mode { Normal, MoveIn, Evict } _Mode = Mode::Normal;
    int16_t _MovingFamily = -1;
    std::string _Tip;
    double _TipTime = 0, _Time = 0;
    double _PressT = 0;

    void BuildArt();
    void BuildButtons();
    void Composite();
    int LotAtBottom(int x, int y) const;
    void Activate(int lotIndex);
    void TryMoveIn(int16_t family, int house);
    void TryEvict(int house);
    void EnterLot(int house);
    void OpenPicker(int targetHouse);
    void OnButton(const std::string& id);
    void SwitchNeighbourhood(int dir);
    void GoToNeighbourhood(int n); // FIX(sims) SAVE: Switch + the broken-file offer
    void DrawTop(Canvas& top);
    void DrawBottom(Canvas& bottom);
    void DrawPopup(Canvas& top, const LotInfo& lot, int sx, int sy);
    void Tip(const std::string& t) { _Tip = t; _TipTime = 2500; }
    void Sound(const char* name);
    bool _SuppressTap = false;
    std::function<void(int)> _PickDone;
    std::string _PickTitle;
    void FinishPick(int house);
};

/// Scrolling credits (STR#163 'credits'), from the logo button.
class CreditsScreen;

}}
