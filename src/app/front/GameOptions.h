/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims front end — the TS1 Options panel content (control panel "Options" mode): the game buttons
// Neighborhood / Save / Quit and the Graphics / Sound / Play option pages, with the original art
// (CPanel/Buttons/OptNbhd, OptSave, OptExit, OptGraphics, OptSound, OptPlay, NewRadio, OptSlider*,
// Shared/Sys/CheckBtnNew) and texts (STR#145 'optionstrs': "Graphics Options", "Anti-alias", "Free Will",
// the "About ..." help texts shown as the top-screen tooltip).
//
// TS1(sims): the panel is drawn into a rectangle of the control panel by the lot UI's options mode
// (src/app/ui/UIOptionsPanel.cpp hosts it; ids "gopt_*" go through Hit()/Tap()). The original put the game
// buttons at (29;6) (88;6) (159;6) and the page buttons at (29;58) (89;58) (151;58) of its panel (STR#145);
// here both rows share one line and the page below uses the rest of the panel.
// Settings persist in <SaveRoot>/sims_options.txt (FrontSettings) and are pushed to the sound engine
// (volumes) and to FrontEnd::SettingsChanged (free will / autonomy hook for the simulation).
#pragma once
#include <functional>
#include <string>
#include "app/Canvas.h"
#include "app/front/FrontEnd.h"

namespace simapp { namespace front {

class GameOptions {
public:
    static GameOptions& Get();
    FrontEnd* FE = nullptr;              // set by the App (settings, saving)
    enum class Page { Graphics, Sound, Play };
    Page Current = Page::Play;
    enum class Action { None, Neighbourhood, Save, Quit };
    /// Set by a tap on Neighborhood / Save / Quit; the App takes it (TakeAction) and runs the TS1 dialogs.
    Action Pending = Action::None;
    Action TakeAction() { Action a = Pending; Pending = Action::None; return a; }
    /// The "About ..." text of the last option touched (STR#145), for a tooltip on the top screen.
    std::string Help;
    double HelpTime = 0;

    /// Draws into r; hit(rect, id) registers the touch areas ("gopt_*").
    void Draw(Canvas& c, const Rect& r, const std::function<void(const Rect&, const std::string&)>& hit);
    /// A tap on a "gopt_*" id. Returns false for ids that are not ours.
    bool Tap(const std::string& id, int touchX = -1);
    /// Slider drags: the touch x within the last drawn slider of `id`.
    void DrawHelp(Canvas& top, double dtMs);

private:
    struct Slider { Rect R; std::string Id; };
    std::vector<Slider> _Sliders;
    bool _ArtLoaded = false;
    void LoadArt();
    void Changed();
};

}}
