/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims — Create-A-Family / Create-A-Sim for the dual screen (API: src/app/cas/README.md).
//
//   Top screen (400x240):    the live 3D Sim on the CAS backdrop (idle animation, turntable / circle pad / drag),
//                            with name, astrological sign and the personality descriptions; in the family view
//                            the whole family standing in a row.
//   Bottom screen (320x240): the original CAS controls (UIGraphics art): first/last name, the personality LEDs and
//                            points remaining, male/female/adult/child/skin cluster, head/body arrows, biography,
//                            the family member strip with Add/Delete/Edit, Done/Cancel.
//
// The screen saves the family itself (CASFamilyWriter: character files + Neighborhood.iff FAMI/FAMs/NBRS) and
// reports Done() with the new family's id; the family then sits in the family bin until it is moved into a lot.
#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <random>
#include <string>
#include <vector>
#include "app/Canvas.h"
#include "app/Input.h"
#include "app/cas/CASArt.h"
#include "app/cas/CASCatalog.h"
#include "app/cas/CASFamilyWriter.h"
#include "app/cas/CASSimDesc.h"
#include "app/cas/CASSimPreview.h"
#include "app/cas/CASTextInput.h"

namespace simapp { class App; }

namespace simapp { namespace cas {

class CASScreen {
public:
    struct Config {
        std::string DataRoot;     // install root: GameData/, UIGraphics/, ExpansionPack*/...
        std::string UserDataDir;  // neighbourhood folder the family is saved into (…/UserData or …/UserData2)
        bool Bilinear = true;     // bilinear texture filtering for the large preview
        int LineupFrameSkip = 1;  // 3DS: redraw the family line-up every N frames (2 halves its cost)
    };

    explicit CASScreen(const Config& cfg);
    /// Convenience for the app: DataRoot from the app config, UserDataDir from the engine's neighbourhood
    /// (FSO::Content::Content::Get()->Neighborhood->UserPath) or <DataRoot>/UserData.
    static Config DefaultConfig(const std::string& dataRoot);
    ~CASScreen();

    /// One frame of input + animation (dtMs = real time since the last frame).
    void Update(const AppInput& in, double dtMs);
    /// Draws both screens (400x240 / 320x240 canvases, e.g. App::Top / App::Bottom).
    void Draw(Canvas& top, Canvas& bottom);

    bool Ready() const { return _Ready; }          // catalogue + avatar data available (else Error says why)
    std::string Error;
    bool Done() const { return _State == State::Finished; }
    bool Cancelled() const { return _State == State::Cancelled; }
    /// The new family's FAMI chunk id (the TS1 family number) once Done(), else -1.
    int16_t FamilyID() const { return _SaveResult.FamilyID; }
    const CASSaveResult& SaveResult() const { return _SaveResult; }
    const CASFamilyDesc& Family() const { return _Family; }

    /// Text entry (names, bio). Defaults to the 3DS system keyboard; the host driver replaces it.
    std::function<bool(const CASTextRequest&, std::string&)> TextInput;
    /// Optional: GUIDs already used by objects (avoids collisions when generating Sim GUIDs).
    std::function<bool(uint32_t)> GUIDInUse;

    /// After Done(): re-reads Neighborhood.iff into the running engine (TS1NeighborhoodProvider::InitSpecific +
    /// LoadCharacters) so the new family is in the family bin and its Sims can be created on a lot.
    static bool ReloadEngineNeighbourhood(const std::string& userDataDir);

    std::string Log; // notable events, cleared by the caller

    // ---- host scripting ----
    bool ScriptButtonPos(const std::string& id, int& x, int& y);
    void Seed(unsigned s) { _Rng.seed(s); }
    std::string Describe() const;

private:
    enum class Page { Family, Sim };
    enum class State { Editing, Saving, Finished, Cancelled };
    struct Control { Rect R; std::string Id; bool Enabled = true; };
    struct Dialog {
        bool Open = false;
        std::string Title, Text;
        std::vector<std::string> Buttons;
        std::function<void(int)> OnClose;
    };

    Config _Cfg;
    bool _Ready = false;
    CASArt _Art;
    CASCatalog _Catalog;
    std::mt19937 _Rng;
    Page _Page = Page::Family;
    State _State = State::Editing;
    Dialog _Dialog;
    CASSaveResult _SaveResult;

    // family
    CASFamilyDesc _Family;
    int _Selected = -1;
    std::vector<std::unique_ptr<CASSimPreview>> _Lineup; // one per member
    std::vector<float> _LineupPhase;
    int _FrameNo = 0;
    Canvas _TopCache; // family line-up, reused on skipped frames (Config::LineupFrameSkip)

    // sim being edited
    CASSimDesc _Edit;
    int _EditIndex = -1; // member index, -1 = new
    int _HeadIdx = 0, _BodyIdx = 0;
    int _TraitSel = -1;  // d-pad row
    int _InfoTrait = -1; // trait shown on the top screen
    CASSimPreview _Preview;
    float _Rot = 0;
    double _SinceManualRot = 1e9, _Time = 0;

    // touch
    std::vector<Control> _Controls;
    std::string _Pressed;
    bool _Touching = false, _Dragging = false;
    int _TouchX0 = 0, _TouchLastX = 0;

    // ---- logic ----
    void Layout();
    const Control* Hit(int x, int y) const;
    void OnTap(const std::string& id, int x, int y);
    void OnButtons(const AppInput& in);
    void BeginEdit(int memberIndex);
    void FinishEdit();
    void CancelEdit();
    void ApplyLook(bool resetIdle);
    void SetAge(CASAge a);
    void SetGender(CASGender g);
    void SetSkin(CASSkin s);
    void StepHead(int d);
    void StepBody(int d);
    void RandomLook();
    void RandomPersonality();
    void SetTrait(int t, int value);
    void EditText(const std::string& what);
    void RequestDone();
    void RequestCancel();
    void SaveFamily();
    void SyncLineup();
    void OpenDialog(const std::string& title, const std::string& text, std::vector<std::string> buttons, std::function<void(int)> onClose);
    const std::vector<CASOutfit>& Heads() const { return _Catalog.Heads(_Edit.Age, _Edit.Gender, _Edit.Skin); }
    const std::vector<CASOutfit>& Bodies() const { return _Catalog.Bodies(_Edit.Age, _Edit.Gender, _Edit.Skin); }
    int Rand(int n) { return n <= 0 ? 0 : (int)(_Rng() % (unsigned)n); }

    // ---- drawing ----
    void DrawSimTop(Canvas& c);
    void DrawSimBottom(Canvas& c);
    void DrawFamilyTop(Canvas& c);
    void DrawFamilyBottom(Canvas& c);
    void DrawDialog(Canvas& c);
    void DrawPanel(Canvas& c, const Rect& r) const;
    void DrawSprite(Canvas& c, const CASSprite& s, int frame, int x, int y) const;
    void DrawArtButton(Canvas& c, const CASSprite& s, const Rect& r, const std::string& id, bool selected, bool enabled,
                       const std::string& fallbackLabel, int frameCount = 4) const;
    void DrawZodiac(Canvas& c, int cx, int cy, int r, int sign) const;
    void DrawDice(Canvas& c, const Rect& r, bool pressed) const;
    std::string TraitName(int t) const;
    std::string FullName(const CASSimDesc& s) const;
};

}}
