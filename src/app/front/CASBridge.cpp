/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims front end — see CASBridge.h. The only place that touches src/app/cas (adapt here if its API moves).
#include "app/front/CASBridge.h"
#include "app/front/FrontStrings.h"
#include "app/front/FrontUI.h"
#include "app/front/TS1Font.h"
#include "app/front/ResGraphics.h"
#include "content/ContentManager.h"
#include "files/LoadProgress.h"
#if defined(__has_include)
#include "app/front/FrontAudio.h"
#if __has_include("app/cas/CASScreen.h")
#include "app/cas/CASScreen.h"
#include "simantics/VM.h"
#include "vitaboy/content/TS1AvatarProvider.h"
#define SIMS_HAVE_CAS 1
#endif
#endif

namespace simapp { namespace front {

#ifdef SIMS_HAVE_CAS
/// simapp::cas::CASScreen (src/app/cas/CASScreen.h) run as a sub screen: Update + Draw each frame until Done()
/// (the family is saved by CAS itself: character files + Neighborhood.iff) or Cancelled().
class CASSubScreen : public SubScreen {
public:
    // TS1(sims) hardware pass: nothing heavy here. The first frame shows a loading screen (the Create A Family
    // background, both screens) and only the next frame builds the CAS screen (avatar file index, UI art, skin
    // catalogue: up to a minute on the first hardware run, the console looked frozen), redrawing that loading screen
    // with each step / progress report (App::LoadProgressFrame -> DrawProgress) and logging every step.
    CASSubScreen(const std::string& dataRoot, const std::string& userDir) : _DataRoot(dataRoot), _UserDir(userDir) {
        FrontMusic("Family"); // bkground_family (GameAudio keeps the neighbourhood's tune playing)
    }
    ~CASSubScreen() override {
        // FIX(sims) memory: the line-up's avatar meshes/textures/keyframes are not needed in the neighbourhood
        _Cas.reset();
        if (FSO::SimAntics::VM::ReleaseAssembled()) // no lot VM alive: no avatar uses them
            if (auto av = FSO::Content::TS1::TS1AvatarContent::Get()) av->ReleaseCaches();
    }
    bool Frame(const AppInput& in, double dtMs, Canvas& top, Canvas& bottom) override {
        if (!_Cas) {
            DrawLoading(top, bottom);
            if (_Frames++ == 0) return true; // shown before the blocking part
            FSO::Files::LoadProgress::Step("cas: opening Create-A-Family");
            auto cfg = simapp::cas::CASScreen::DefaultConfig(_DataRoot);
            cfg.UserDataDir = _UserDir;
#ifdef __3DS__
            cfg.LineupFrameSkip = 2;
#endif
            _Cas.reset(new simapp::cas::CASScreen(cfg));
            _Cas->GUIDInUse = [](uint32_t g) { auto c = FSO::Content::Content::Get(); return c && c->WorldObjects->Entries.count(g) != 0; };
            FSO::Files::LoadProgress::Step("cas: first frame");
            _Cas->Update(AppInput(), 0);
            _Cas->Draw(top, bottom);
            FSO::Files::LoadProgress::Step("cas: running");
            return true;
        }
        _Cas->Update(in, dtMs);
        _Cas->Draw(top, bottom);
        return !(_Cas->Done() || _Cas->Cancelled());
    }
    bool DrawProgress(const std::string& text, float fraction, Canvas& top, Canvas& bottom) override {
        if (_Cas) return false;
        if (!text.empty()) _Text = text;
        _Fraction = fraction;
        DrawLoading(top, bottom);
        return true;
    }
    bool ScriptButtonPos(const std::string& id, int& x, int& y) override { return _Cas && _Cas->ScriptButtonPos(id, x, y); }
    std::string Describe() const override { return _Cas ? "cas " + _Cas->Describe() : std::string("cas loading"); }
private:
    void DrawLoading(Canvas& top, Canvas& bottom) {
        if (_Bkg.Width == 0 && !_BkgTried) { _BkgTried = true; ResGraphics::Get().LoadScaled("kDesignFamilyBkg", 0.4f, _Bkg); }
        top.Clear(RGBA(0, 10, 60));
        if (_Bkg.Width > 0) { Canvas big; Resample(_Bkg, big, 400, 300); top.Blit(big.Px.data(), big.Width, big.Height, 0, -30, false); }
        top.Dim(0, 0, top.Width, top.Height, 150);
        auto& S = FrontStrings::Get();
        TextCentered(top, TS1Size::S20, Rect(0, 92, top.Width, 26), S.Get(128, 2, "Create New Family"), RGBA(255, 216, 96), RGBA(0, 0, 30, 220));
        TextCentered(top, TS1Size::S12, Rect(0, 126, top.Width, 18), S.Get(155, 15, "Loading Characters"), RGBA(255, 255, 255), RGBA(0, 0, 30, 220));
        bottom.GradientV(0, 0, bottom.Width, bottom.Height, RGBA(10, 30, 110), RGBA(0, 8, 50));
        if (_Bkg.Width > 0) { bottom.Blit(_Bkg.Px.data(), _Bkg.Width, _Bkg.Height, (bottom.Width - _Bkg.Width) / 2, (bottom.Height - _Bkg.Height) / 2, false); bottom.Dim(0, 0, bottom.Width, bottom.Height, 170); }
        // a progress bar: the fraction when known, else a sweep that moves with each redraw
        Rect bar(40, 150, bottom.Width - 80, 8);
        bottom.FillRect(bar.X, bar.Y, bar.W, bar.H, RGBA(0, 0, 30, 255));
        if (_Fraction >= 0) bottom.FillRect(bar.X, bar.Y, (int)(bar.W * (_Fraction > 1 ? 1.0f : _Fraction)), bar.H, RGBA(120, 200, 255, 255));
        else { int w = bar.W / 4, x = bar.X + (int)((_Sweep++ * 23) % (unsigned)(bar.W - w)); bottom.FillRect(x, bar.Y, w, bar.H, RGBA(120, 200, 255, 255)); }
        TextCentered(bottom, TS1Size::S9, Rect(0, 164, bottom.Width, 13), _Text.empty() ? std::string("Please wait...") : _Text, RGBA(210, 220, 240));
    }
    std::string _DataRoot, _UserDir;
    std::unique_ptr<simapp::cas::CASScreen> _Cas;
    Canvas _Bkg;
    bool _BkgTried = false;
    int _Frames = 0;
    unsigned _Sweep = 0;
    std::string _Text;
    float _Fraction = -1.0f;
};
bool HaveCASScreen() { return true; }
std::unique_ptr<SubScreen> MakeCASScreen(const std::string& dataRoot, const std::string& userDir, int) {
    return std::unique_ptr<SubScreen>(new CASSubScreen(dataRoot, userDir));
}
#else
/// Placeholder: the design-a-family background with a note (TS1(sims)).
class NoCASScreen : public SubScreen {
public:
    NoCASScreen() {
        ResGraphics::Get().LoadScaled("kDesignFamilyBkg", 0.4f, _Bkg);
        _Ok.Label = FrontStrings::Get().Get(152, 0, "OK");
        _Ok.R = Rect(110, 196, 100, 24);
    }
    bool Frame(const AppInput& in, double, Canvas& top, Canvas& bottom) override {
        top.Clear(RGBA(0, 10, 60));
        if (_Bkg.Width > 0) { Canvas big; Resample(_Bkg, big, 400, 300); top.Blit(big.Px.data(), big.Width, big.Height, 0, -30, false); }
        top.Dim(0, 0, top.Width, top.Height, 140);
        TextCentered(top, TS1Size::S20, Rect(0, 90, top.Width, 26), FrontStrings::Get().Get(128, 2, "Create New Family"), RGBA(255, 216, 96), RGBA(0, 0, 30, 220));
        TextCentered(top, TS1Size::S12, Rect(0, 124, top.Width, 18), "Create-A-Sim is not part of this build yet.", RGBA(255, 255, 255), RGBA(0, 0, 30, 220));
        bottom.GradientV(0, 0, bottom.Width, bottom.Height, RGBA(10, 30, 110), RGBA(0, 8, 50));
        TextCentered(bottom, TS1Size::S12, Rect(0, 90, bottom.Width, 18), "Create-A-Sim (src/app/cas)", RGBA(255, 255, 255));
        _Ok.Draw(bottom);
        return !(_Ok.Update(in) || (in.Down & (BTN_A | BTN_B)));
    }
private:
    Canvas _Bkg;
    TextButton _Ok;
};
bool HaveCASScreen() { return false; }
std::unique_ptr<SubScreen> MakeCASScreen(const std::string&, const std::string&, int) { return std::unique_ptr<SubScreen>(new NoCASScreen()); }
#endif

// ---------------------------------------------------------------- credits (TS1(sims))

class CreditsScreen : public SubScreen {
public:
    CreditsScreen() {
        auto& S = FrontStrings::Get();
        for (int i = 0; i < 200; i++) {
            std::string s = S.Get(163, i, "\x01");
            if (s == "\x01") break;
            _Lines.push_back(s == "[F]" ? std::string() : s);
        }
        ResGraphics::Get().LoadScaled("kSimsLogo", 0.25f, _Logo);
    }
    bool Frame(const AppInput& in, double dtMs, Canvas& top, Canvas& bottom) override {
        _T += dtMs;
        float speed = (in.Held & BTN_DOWN) ? 120.0f : 28.0f; // pixels per second
        _Y += speed * (float)dtMs / 1000.0f;
        top.GradientV(0, 0, top.Width, top.Height, RGBA(0, 12, 70), RGBA(0, 4, 30));
        int lh = 18;
        int total = (int)_Lines.size() * lh + 260;
        if (_Y > total) _Y = 0;
        int y = top.Height - (int)_Y;
        for (size_t i = 0; i < _Lines.size(); i++, y += lh) {
            if (y < -lh || y > top.Height) continue;
            bool head = i == 0 || (i > 0 && _Lines[i - 1].empty() && !_Lines[i].empty() && _Lines[i].find('"') == std::string::npos);
            TextCentered(top, head ? TS1Size::S14 : TS1Size::S12, Rect(0, y, top.Width, lh), _Lines[i], head ? RGBA(255, 216, 96) : RGBA(255, 255, 255), RGBA(0, 0, 0, 200));
        }
        // fade the edges
        for (int k = 0; k < 24; k++) { top.Dim(0, k, top.Width, 1, 256 - (24 - k) * 10); top.Dim(0, top.Height - 1 - k, top.Width, 1, 256 - (24 - k) * 10); }
        bottom.GradientV(0, 0, bottom.Width, bottom.Height, RGBA(10, 30, 110), RGBA(0, 8, 50));
        if (_Logo.Width > 0) bottom.Blit(_Logo.Px.data(), _Logo.Width, _Logo.Height, (bottom.Width - _Logo.Width) / 2, 30, false);
        TextCentered(bottom, TS1Size::S10, Rect(0, 200, bottom.Width, 14), FrontStrings::Get().Get(154, 4, "Maxis is an Electronic Arts brand."), RGBA(170, 190, 230));
        TextCentered(bottom, TS1Size::S9, Rect(0, 218, bottom.Width, 14), "Tap to return  -  hold DOWN to scroll faster", RGBA(120, 140, 190));
        return !(in.TouchDown || (in.Down & (BTN_A | BTN_B | BTN_START)));
    }
private:
    std::vector<std::string> _Lines;
    Canvas _Logo;
    double _T = 0;
    float _Y = 0;
};

std::unique_ptr<SubScreen> MakeCreditsScreen() { return std::unique_ptr<SubScreen>(new CreditsScreen()); }

}}
