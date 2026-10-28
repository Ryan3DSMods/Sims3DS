/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims app — everything on screen while a lot is loaded: FreeSims UILotControl (the lot, its cursor, the pie
// menu, dialogs) + UIUCP (the control panel: mode buttons, clock, speed, funds) + the mode panels UILiveMode,
// UIBuyMode, UIBuildMode and the options panel.
//
// ADDED(sims) — the dual screen:
//   top (400x240)    the lot, with TS1's overlays: action queue (top left), plumbob, thought/speech balloons
//                    (UIHeadlineRenderer), money pop-ups, the hand cursor, buy/build tooltips (cost, placement error)
//   bottom (320x240) the control panel: the gadget column on the left (mode buttons, speed, clock, funds, mood)
//                    and the active mode's panel on the right. Where the stylus has to touch the lot (live mode
//                    picking, placing objects, drawing walls) the panel shows a scaled "touch view" of the top screen.
// Buttons: circle pad = hand cursor on the top screen (edge scroll), A = click at the cursor (hold + move = drag),
// C-stick = scroll, L/R rotate, d-pad up/down zoom, ZL/ZR floors, SELECT walls, X pause, Y centre / rotate the held
// object, START = options, B = cancel.
#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include "app/Canvas.h"
#include "app/Input.h"
#include "app/LotSession.h"
#include "app/PieMenu.h"
#include "app/UIStrings.h"

namespace FSO { namespace SimAntics { class VMEntity; class VMAvatar; } }

namespace simapp {

class UILiveMode;
class UIBuyMode;
class UIBuildMode;
class UIOptionsPanel;
class UIObjectHolder;

class LotScreen {
public:
    static const int TOP_W = 400, TOP_H = 240, BOT_W = 320, BOT_H = 240;
    enum class Mode { Live = 0, Buy = 1, Build = 2, Options = 3 };

    LotScreen(LotSession* session, UIStrings* str, Canvas* top, Canvas* bottom);
    ~LotScreen();

    /// Input for one frame (before the app runs the simulation at EffectiveSpeed()).
    void Input(const AppInput& in, double dtMs, double timeMs);
    /// Both screens (after the simulation and World::Draw).
    void Draw();

    LotSession* S;
    UIStrings* Str;
    Canvas* Top;
    Canvas* Bottom;
    std::string* Log = nullptr;
    double Time = 0;          // ms (animations)
    int Speed = 1;            // 0..3 (TS1)
    int LastSpeed = 1;
    bool LeaveRequested = false, QuitRequested = false;
    bool CheatRequested = false; // TS1(sims): the Options panel's "Cheat" button (the App opens the cheat console)
    /// TS1(sims): the funds changed without a "+§" balloon (cheats): the next TrackBudget sees no difference.
    void QuietBudget() { _LastBudget = S->Budget(); }
    Mode CurrentMode = Mode::Live;
    void SetMode(Mode m);
    /// TS1: time stands still in buy, build and options mode, and while a blocking dialog waits.
    int EffectiveSpeed() const;

    // ---- shared widgets for the mode panels
    struct HitBox { Rect R; std::string Id; };
    std::vector<HitBox> Hits;           // filled while drawing the bottom screen, read by the next frame's taps
    void Hit(const Rect& r, const std::string& id) { Hits.push_back({ r, id }); }
    std::string HitAt(int x, int y) const;
    /// Original-art button (UIGraphics strip) with a Theme fallback (label or icon drawer).
    void DrawArtButton(Canvas& c, const Rect& r, const std::string& art, int frames, bool selected, bool disabled,
                       const std::string& fallbackLabel, const std::string& hitId);
    void DrawPanelFrame(Canvas& c, const Rect& r);

    // the touch view: the top screen scaled into a rect of the bottom screen
    Rect ViewRect;
    float ViewScale = 0.6f;
    Canvas ViewThumb;
    uint32_t ViewFrameNo = 0;
    int ThumbInterval = 1;
    void DrawTouchView(Canvas& c, const Rect& r);
    bool ViewToTop(int bx, int by, int& tx, int& ty) const;
    void TopToView(float tx, float ty, int& bx, int& by) const;

    // pointer = top-screen "mouse" fed by the stylus in the touch view or by the cursor + A
    int CursorX = TOP_W / 2, CursorY = TOP_H / 2;
    bool CursorShown = true;
    // ADDED(sims): the pointer is only on screen while it is useful (console style): any touch on the bottom screen
    // hides it, the circle pad / C-stick / A show it again; in live mode it fades out after POINTER_HIDE_MS without
    // pad movement. CursorX/Y keep following the stylus while hidden (buy/build placement feedback, A clicks there).
    static constexpr double POINTER_HIDE_MS = 3000, POINTER_FADE_MS = 350;
    double PointerMovedAt = -1;         // Time of the last pad movement (live-mode auto hide)
    void ShowPointer();
    void HidePointer() { CursorShown = false; }
    int PointerAlpha() const;           // 0..255 (fading out at the end of the idle time in live mode)
    // ADDED(sims): where the walls are cut around the "mouse" (LotSession::UpdateMouseCut): the pointer while it is
    // shown, the stylus position while it touches the lot in the touch view; CutFollow false = keep the last cut.
    bool CutFollow = true; int CutX = TOP_W / 2, CutY = TOP_H / 2;
    struct Pointer { bool Active = false, Down = false, Held = false, Up = false, Click = false, FromTouch = false; int X = 0, Y = 0; };
    Pointer P;

    // money pop-ups ("+§50" floating up, TS1)
    void MoneyPopup(int amount, FSO::SimAntics::VMEntity* at);
    void MoneyPopupAt(int amount, int topX, int topY); // FIX(sims) QA2: build costs float where they were spent
    // tooltip on the top screen (cost of a wall, placement error)
    std::string TopTip; uint32_t TopTipColor = 0; int TopTipX = 0, TopTipY = 0; double TopTipUntil = 0;
    void ShowTopTip(const std::string& s, int x, int y, uint32_t color, double ms = 1500);

    // icons (catalog BMP of objects, FreeSims VMGameObject.GetIcon; Sims: the face from their character file)
    const Canvas* IconFor(FSO::SimAntics::VMEntity* e);
    /// TS1(sims): catalogue icon / name of an object by GUID (inventory tokens are not on the lot)
    const Canvas* IconForGUID(uint32_t guid);
    std::string NameForGUID(uint32_t guid);
    const Canvas* FaceFor(FSO::SimAntics::VMAvatar* a, int frame = 0); // 0 normal, 1 hover, 2 selected (faces BMP)

    // ---- pie menu / live interactions
    PieMenu Pie;
    FSO::SimAntics::VMEntity* PieTarget = nullptr;
    bool PieGoto = false;
    float PieMarkX = -1, PieMarkY = -1;
    void OpenPieAt(int tx, int ty);
    std::set<uint16_t> Cancelled;
    double PickMsLast = 0;
    std::function<double()> NowMs;

    std::unique_ptr<UIObjectHolder> Holder;
    std::unique_ptr<UILiveMode> Live;
    std::unique_ptr<UIBuyMode> Buy;
    std::unique_ptr<UIBuildMode> Build;
    std::unique_ptr<UIOptionsPanel> Options;

    // ---- scripting (host simulator)
    bool ScriptButtonPos(const std::string& id, int& x, int& y) const;
    bool ScriptObjectPos(const std::string& name, int& x, int& y, std::string* found);
    std::string DescribeQueue();
    bool ShowStats = false;
    std::string StatsLine;

    static const Rect GadgetRect;   // left column of the control panel
    static const Rect PanelRect;    // right part (mode panel)

private:
    struct Popup { int Amount; float X, Y; double Start; float Lift = 1.6f; bool Head = false; float Z = 0; int SX = -1, SY = -1; };
    std::vector<Popup> _Popups;
    std::map<int16_t, uint32_t> _MoneyHeadlineSerial; // VMAvatar::MoneyHeadlineSerial last seen, per Sim
    void PollMoneyHeadlines();
    int _LastBudget = -1;
    bool _BudgetExplained = false;
    std::map<uint64_t, std::unique_ptr<Canvas>> _Icons;
    std::map<uint64_t, std::unique_ptr<Canvas>> _Faces;
    bool _ViewTouch = false, _ViewDragging = false, _TouchInView = false;
    int _TouchStartX = 0, _TouchStartY = 0, _TouchLastX = 0, _TouchLastY = 0;
    bool _ABtnHeld = false;
    std::vector<std::pair<Rect, int>> _DialogButtons;

    void HandleInput(const AppInput& in, double dtMs);
    void HandleButtons(const AppInput& in);
    void HandleTap(const std::string& id);
    void DrawTop();
    void DrawBottom();
    void DrawGadget(Canvas& c);
    void DrawDialog();
    void DrawQueueTop(FSO::SimAntics::VMAvatar* sim);
    void DrawHeadlines();
    void DrawPopups();
    void DrawCursor();
    void TrackBudget();
    void DrawCheatOverlays(); // TS1(sims): tile_info, route_balloons, draw_routes
};

}
