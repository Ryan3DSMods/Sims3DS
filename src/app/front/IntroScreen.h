/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims front end — start-up: the EA logo (optional, see tools/convert_intro.py) and the loading screen.
//
// LoadingScreen is translated from FreeSims SimsVille/UI/Screens/LoadingScreen.cs: the "setup" picture
// (UIGraphics Other/setup.bmp, kSimsLogo in Res_Other.RT) scaled to the screen height and centred on a
// (0x09,0x18,0x2F) background, the copyright line (STR#154 #5) at y=508 and the progress labels (STR#155
// 'splashprogress') at y=550 of the 800x600 layout, each new label sliding in from the right over one second
// while the previous one slides out to the left (UITween, 1.0 s), advancing when the load progress passes the
// label's share. TS1(sims): on the 3DS the picture fills the top screen (0.4 scale) and the touch screen
// shows the Maxis line, the load progress and the list of steps.
//
// The EA intro (Music/eag_us.avi, MS-MPEG4v1 + PCM) cannot be decoded on the console; tools/convert_intro.py
// turns it into <cache>/intro.stv (400x240 grey frames, RLE), which is played here when present.
// ADDED(sims): and <cache>/intro.wav (its sound, 32728 Hz s16 stereo, the video's length), which FrontEnd starts with
// the first frame (FrontEnd::IntroSound -> GameAudio::PlayFile) and stops when the intro ends or is skipped.
#pragma once
#include <string>
#include <vector>
#include "app/Canvas.h"
#include "app/Input.h"

namespace simapp { namespace front {

/// <cache>/intro.stv: "STV1", u16 w, u16 h, u16 fps*100, u16 frames, then per frame u32 size + RLE (u8 count, u8 grey).
class IntroVideo {
public:
    bool Open(const std::string& path);
    bool IsOpen() const { return _F != nullptr; }
    ~IntroVideo();
    /// Advances by dt; false when the video is over. Decodes the current frame into `frame` (grey -> RGB).
    bool Update(double dtMs);
    void Draw(Canvas& top) const;
private:
    void* _F = nullptr;
    int _W = 0, _H = 0, _Frames = 0, _Index = -1;
    double _Fps = 30, _T = 0;
    std::vector<uint8_t> _Grey;
    bool ReadFrame();
};

class LoadingScreen {
public:
    void Init(); // loads the picture and the labels
    /// FIX(sims) FB1: the same screen for loading a lot / a trip: `art` (Res_*.RT name or path: kSimsLogo = setup.bmp,
    /// the expansion packs' kDTGo / kVacationGo / ... vehicle pictures) and the lot loading labels (STR# 155).
    void InitLot(const std::string& art, const std::string& title);
    /// FIX(sims) FB1: the lot loader's step ("lot 5: objects + architecture") moves the progress / label.
    void SetLotStep(const std::string& step);
    std::string Title;         // touch screen heading (default: the game's name)
    void SetProgress(float p) { _Progress = p; }
    /// Advances the label tweens. Returns true once every label has been shown and the progress is 1.
    bool Update(double dtMs);
    void Draw(Canvas& top, Canvas& bottom);
    std::string CurrentStep;   // the file / step being worked on (touch screen)
    bool Done() const { return _Done; }

private:
    Canvas _Background;        // setup.bmp at 0.4 (320x240)
    std::vector<std::string> _PreloadLabels;
    int _CurrentPreloadLabel = 0;
    bool _InTween = false, _Done = false;
    double _TweenT = 0;
    std::string _Label1, _Label2;
    float _Progress = 0;
    std::string _Copyright, _Brand;
    void AnimateLabel(const std::string& previous, const std::string& next);
    void CheckPreloadLabel();
};

}}
