/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims — Create-A-Family / Create-A-Sim screen (see CASScreen.h, README.md). TS1(sims).
//
// Flow of the original (UIText.iff STR# 129/130): the "CREATE A FAMILY" screen (last name, up to 8 Sims, Add a New
// Sim / Delete Selected Sim / Edit Selected Sim, Done -> "Finished?", Cancel -> "Cancel?") opens "CREATE A SIM" for
// each member (first name, adult/child, female/male, light/medium/dark skin, previous/next head and body, the five
// personality traits with 25 points, the astrological sign, the bio; Done with points left -> "More Personality!").
#include "app/cas/CASScreen.h"
#include "files/LoadProgress.h"
#include "app/Font.h"
#include "app/Theme.h"
#include "audio/GameAudio.h"
#include "content/ContentManager.h"
#include "content/TS1NeighbourProvider.h"
#include "vitaboy/content/TS1AvatarProvider.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>

namespace simapp { namespace cas {

using FSO::Content::TS1::TS1AvatarContent;

// ------------------------------------------------------------------------------------------- colours / layout

static const uint32_t PanelFill  = RGBA(0, 0, 57);
static const uint32_t PanelShade = RGBA(0, 4, 40);
static const uint32_t PanelLight = RGBA(52, 62, 128);
static const uint32_t TextHint   = RGBA(110, 125, 185);
static const uint32_t TextCyan   = RGBA(0x9C, 0xE6, 0xFF);

namespace L { // bottom screen, Create A Sim
static const Rect Name(4, 3, 204, 22);
static const Rect Points(212, 3, 104, 22);
static const Rect Personality(4, 28, 150, 129);
static const int TraitY0 = 34, TraitDY = 20, LedX = 64, LedW = 86, LedH = 13;
static const Rect PointsBar(9, 137, 140, 13);
static const Rect ClusterAt(156, 28, 84, 149);
static const Rect Arrows(244, 28, 72, 149);
static const Rect Bio(4, 160, 150, 46);
static const Rect Random(244, 181, 72, 25);
static const Rect Cancel(4, 211, 98, 24);
static const Rect Done(218, 211, 98, 24);
}
namespace F { // bottom screen, Create A Family (DsgnFamBkg at 0.4)
static const Rect Name(106, 18, 110, 14);
static const Rect Members(30, 40, 204, 124);
static const Rect Info(6, 168, 308, 38);
static const Rect Cancel(4, 211, 110, 26);
static const Rect Done(206, 211, 110, 26);
}

static std::string Money(int32_t v) {
    std::string s = std::to_string(v), o;
    int n = 0;
    for (int i = (int)s.size() - 1; i >= 0; i--) { o.insert(o.begin(), s[(size_t)i]); if (++n % 3 == 0 && i > 0) o.insert(o.begin(), ','); }
    return "\xA7" + o; // § (Latin-1), the Simoleon sign
}

// ------------------------------------------------------------------------------------------- setup

CASScreen::Config CASScreen::DefaultConfig(const std::string& dataRoot) {
    Config c;
    c.DataRoot = dataRoot;
    c.UserDataDir = dataRoot + "/UserData";
    auto content = FSO::Content::Content::Get();
    if (content && content->Neighborhood && !content->Neighborhood->UserPath.empty()) c.UserDataDir = content->Neighborhood->UserPath;
    return c;
}

CASScreen::CASScreen(const Config& cfg) : _Cfg(cfg), _Rng(0x5153u) {
    TextInput = CASSystemKeyboard;
    auto av = TS1AvatarContent::Get();
    if (!av->Initialized()) TS1AvatarContent::SetBasePath(cfg.DataRoot);
    FSO::Files::LoadProgress::Step("cas: UI art"); // TS1(sims) hardware pass: log.txt step lines
    _Art.Load(cfg.DataRoot);
    FSO::Files::LoadProgress::Step("cas: skin catalogue");
    if (!_Catalog.Build()) { Error = "No Sim skins found (GameData/Skins, Textures.far, Animation.far)."; return; }
    _Ready = true;
    Log += "[cas] catalogue: " + std::to_string(_Catalog.HeadCount()) + " heads, " + std::to_string(_Catalog.BodyCount()) +
           " bodies; UI art " + (_Art.HaveArt ? "from UIGraphics.far" : "not found (plain panels)") + "\n";
}

CASScreen::~CASScreen() {}

// ------------------------------------------------------------------------------------------- family members

void CASScreen::SyncLineup() {
    while (_Lineup.size() < _Family.Members.size()) {
        _Lineup.emplace_back(new CASSimPreview());
        _LineupPhase.push_back((float)Rand(4000));
    }
    _Lineup.resize(_Family.Members.size());
    _LineupPhase.resize(_Family.Members.size());
    for (size_t i = 0; i < _Family.Members.size(); i++) {
        _Lineup[i]->Set(_Family.Members[i]);
        _Lineup[i]->Update(_LineupPhase[i]);
        _LineupPhase[i] = 0;
    }
}

void CASScreen::BeginEdit(int memberIndex) {
    _EditIndex = memberIndex;
    if (memberIndex >= 0 && memberIndex < (int)_Family.Members.size()) {
        _Edit = _Family.Members[(size_t)memberIndex];
        _HeadIdx = std::max(0, CASCatalog::Find(Heads(), _Edit.Head));
        _BodyIdx = std::max(0, CASCatalog::Find(Bodies(), _Edit.Body));
        Log += "[cas] edit " + _Edit.FirstName + "\n";
    } else {
        // a new Sim: adult, the opposite sex of the last adult added, the family's skin tone, a random look;
        // all 25 personality points still to spend; interests from the template's 'init traits'
        _Edit = CASSimDesc();
        CASGender g = CASGender::Female;
        for (auto& m : _Family.Members) if (m.Age == CASAge::Adult) g = m.Gender == CASGender::Female ? CASGender::Male : CASGender::Female;
        _Edit.Gender = g;
        if (!_Family.Members.empty()) _Edit.Skin = _Family.Members.back().Skin;
        _Edit.InitTraits([this](int n) { return Rand(n); });
        _HeadIdx = Rand((int)Heads().size());
        _BodyIdx = Rand((int)Bodies().size());
        Log += "[cas] new Sim\n";
    }
    _Page = Page::Sim;
    _TraitSel = -1;
    _InfoTrait = -1;
    _Rot = 0;
    _SinceManualRot = 0;
    ApplyLook(true);
}

void CASScreen::FinishEdit() {
    if (_EditIndex >= 0 && _EditIndex < (int)_Family.Members.size()) _Family.Members[(size_t)_EditIndex] = _Edit;
    else { _Family.Members.push_back(_Edit); _EditIndex = (int)_Family.Members.size() - 1; }
    _Selected = _EditIndex;
    Log += "[cas] sim done: " + _Edit.FirstName + " (" + _Edit.Head.Texture + ", " + _Edit.Body.Texture + ", sign " +
           std::to_string(_Edit.Zodiac()) + ")\n";
    _Page = Page::Family;
    SyncLineup();
}

void CASScreen::CancelEdit() {
    _Page = Page::Family;
    Log += "[cas] sim cancelled\n";
}

void CASScreen::ApplyLook(bool resetIdle) {
    for (int attempt = 0; attempt < 64; attempt++) {
        auto& heads = Heads();
        auto& bodies = Bodies();
        if (heads.empty() || bodies.empty()) return;
        _HeadIdx = ((_HeadIdx % (int)heads.size()) + (int)heads.size()) % (int)heads.size();
        _BodyIdx = ((_BodyIdx % (int)bodies.size()) + (int)bodies.size()) % (int)bodies.size();
        _Edit.Head = heads[(size_t)_HeadIdx];
        _Edit.Body = bodies[(size_t)_BodyIdx];
        bool headOk = true, bodyOk = true;
        _Preview.Set(_Edit, &headOk, &bodyOk);
        if (headOk && bodyOk) break;
        // a skin whose mesh does not load (a texture without a usable mesh): drop it from the catalogue
        if (!headOk) { Log += "[cas] head mesh missing: " + _Edit.Head.Mesh + " (" + _Edit.Head.Texture + ")\n"; _Catalog.MarkBroken(_Edit.Head); }
        if (!bodyOk) { Log += "[cas] body mesh missing: " + _Edit.Body.Mesh + " (" + _Edit.Body.Texture + ")\n"; _Catalog.MarkBroken(_Edit.Body); }
    }
    if (resetIdle) _Preview.ResetIdle();
}

void CASScreen::SetAge(CASAge a) {
    if (_Edit.Age == a) return;
    _Edit.Age = a;
    _HeadIdx = Rand((int)Heads().size());
    _BodyIdx = Rand((int)Bodies().size());
    ApplyLook(true);
}

void CASScreen::SetGender(CASGender g) {
    if (_Edit.Gender == g) return;
    _Edit.Gender = g;
    _HeadIdx = Rand((int)Heads().size());
    _BodyIdx = Rand((int)Bodies().size());
    ApplyLook(true);
}

void CASScreen::SetSkin(CASSkin s) {
    if (_Edit.Skin == s) return;
    CASOutfit h = _Edit.Head, b = _Edit.Body;
    _Edit.Skin = s;
    int hi = CASCatalog::FindSameStyle(Heads(), h), bi = CASCatalog::FindSameStyle(Bodies(), b);
    _HeadIdx = hi >= 0 ? hi : Rand((int)Heads().size());
    _BodyIdx = bi >= 0 ? bi : Rand((int)Bodies().size());
    ApplyLook(false);
}

// sounds: TS1 Create-A-Sim (UISounds: ui_cac_cyclehead / ui_cac_cycleparts / ui_cac_personpts, ui_click)
static void CasSound(const char* evt) { simaudio::GameAudio::Get().PlayUI(evt); }

void CASScreen::StepHead(int d) { _HeadIdx += d; ApplyLook(false); CasSound("ui_cac_cyclehead"); }
void CASScreen::StepBody(int d) { _BodyIdx += d; ApplyLook(false); CasSound("ui_cac_cycleparts"); }

void CASScreen::RandomLook() {
    _HeadIdx = Rand((int)Heads().size());
    _BodyIdx = Rand((int)Bodies().size());
    ApplyLook(false);
}

void CASScreen::RandomPersonality() {
    for (auto& t : _Edit.Traits) t = 0;
    for (int i = 0; i < PersonalityPoints; i++) {
        int t;
        do { t = Rand(TraitCount); } while (_Edit.Traits[t] >= TraitMaxPoints);
        _Edit.Traits[t]++;
    }
}

void CASScreen::SetTrait(int t, int value) {
    if (t < 0 || t >= TraitCount) return;
    value = std::max(0, std::min(TraitMaxPoints, value));
    int maxAllowed = _Edit.Traits[t] + _Edit.PointsLeft();
    int before = _Edit.Traits[t];
    _Edit.Traits[t] = std::min(value, maxAllowed);
    _InfoTrait = t;
    if (_Edit.Traits[t] != before) CasSound("ui_cac_personpts");
    else if (value != before) CasSound("ui_error"); // no points left
}

void CASScreen::EditText(const std::string& what) {
    if (!TextInput) return;
    CASTextRequest req;
    std::string* target = nullptr;
    if (what == "familyname") { req.Hint = _Art.Str(129, 12, "Enter Last Name:"); req.Initial = _Family.LastName; req.MaxLength = 20; target = &_Family.LastName; }
    else if (what == "firstname") { req.Hint = _Art.Str(130, 26, "Enter First Name:"); req.Initial = _Edit.FirstName; req.MaxLength = 20; target = &_Edit.FirstName; }
    else if (what == "bio") {
        std::string h = _Art.Str(130, 25, "Bio for %s");
        auto p = h.find("%s");
        if (p != std::string::npos) h.replace(p, 2, _Edit.FirstName.empty() ? std::string("?") : _Edit.FirstName);
        req.Hint = h; req.Initial = _Edit.Bio; req.MaxLength = 200; req.Multiline = true; req.AllowEmpty = true; target = &_Edit.Bio;
    }
    if (!target) return;
    std::string out;
    if (TextInput(req, out)) {
        // trim; names are single words in TS1 (spaces allowed, leading/trailing removed)
        size_t a = out.find_first_not_of(" \t\r\n"), b = out.find_last_not_of(" \t\r\n");
        *target = a == std::string::npos ? std::string() : out.substr(a, b - a + 1);
        Log += "[cas] " + what + " = \"" + *target + "\"\n";
    }
}

void CASScreen::OpenDialog(const std::string& title, const std::string& text, std::vector<std::string> buttons, std::function<void(int)> onClose) {
    _Dialog.Open = true;
    _Dialog.Title = title;
    _Dialog.Text = text;
    _Dialog.Buttons = std::move(buttons);
    _Dialog.OnClose = std::move(onClose);
    Log += "[cas] dialog: " + title + "\n";
}

void CASScreen::RequestDone() {
    if (_Page == Page::Sim) {
        if (_Edit.FirstName.empty()) { EditText("firstname"); if (_Edit.FirstName.empty()) return; }
        if (_Edit.PointsLeft() > 0) {
            OpenDialog(_Art.Str(130, 13, "More Personality!"), _Art.Str(130, 14, "You haven't used all the personality points. Are you really done?"),
                       { _Art.Str(152, 2, "Yes"), _Art.Str(152, 3, "No") }, [this](int b) { if (b == 0) FinishEdit(); });
            return;
        }
        FinishEdit();
        return;
    }
    if (_Family.LastName.empty()) { EditText("familyname"); if (_Family.LastName.empty()) return; }
    if (!_Family.CanFinish()) {
        OpenDialog(_Art.Str(129, 9, "CREATE A FAMILY"), _Family.Members.empty() ? _Art.Str(129, 0, "Add a New Sim") :
                   "A family needs at least one adult.", { _Art.Str(152, 0, "OK") }, nullptr);
        return;
    }
    OpenDialog(_Art.Str(129, 13, "Finished?"),
               _Art.Str(129, 14, "Are you sure you are finished?  You cannot return to the Create A Family screen to edit this family again."),
               { _Art.Str(152, 2, "Yes"), _Art.Str(152, 3, "No") }, [this](int b) { if (b == 0) _State = State::Saving; });
}

void CASScreen::RequestCancel() {
    if (_Page == Page::Sim) { CancelEdit(); return; }
    if (_Family.Members.empty() && _Family.LastName.empty()) { _State = State::Cancelled; Log += "[cas] cancelled\n"; return; }
    OpenDialog(_Art.Str(129, 7, "Cancel?"), _Art.Str(129, 8, "Are you sure you want to cancel and lose this family?"),
               { _Art.Str(152, 2, "Yes"), _Art.Str(152, 3, "No") },
               [this](int b) { if (b == 0) { _State = State::Cancelled; Log += "[cas] cancelled\n"; } });
}

void CASScreen::SaveFamily() {
    SyncLineup();
    std::vector<CASPortraits> portraits(_Family.Members.size());
    for (size_t i = 0; i < _Family.Members.size(); i++) _Lineup[i]->RenderPortraits(portraits[i]);
    FSO::Files::LoadProgress::Step("cas: saving the family"); // TS1(sims) hardware pass
    CASFamilyWriter w(_Cfg.UserDataDir, _Cfg.DataRoot + "/GameData/Objects/Objects.far");
    w.GUIDInUse = GUIDInUse;
    w.Random32 = [this]() { return (uint32_t)_Rng(); };
    _SaveResult = w.Save(_Family, portraits);
    if (!_SaveResult.Ok) {
        _State = State::Editing;
        Log += "[cas] save failed: " + _SaveResult.Error + "\n";
        OpenDialog(_Art.Str(132, 18, "Error!"), _SaveResult.Error, { _Art.Str(152, 0, "OK") }, nullptr);
        return;
    }
    _State = State::Finished;
    char buf[160];
    snprintf(buf, sizeof buf, "[cas] saved family '%s': FAMI #%d (family number %d), %zu Sims\n", _Family.LastName.c_str(),
             (int)_SaveResult.FamilyID, (int)_SaveResult.FamilyNumber, _SaveResult.GUIDs.size());
    Log += buf;
    for (size_t i = 0; i < _SaveResult.Files.size(); i++) {
        snprintf(buf, sizeof buf, "[cas]   %s  GUID 0x%08X  neighbour %d\n", _SaveResult.Files[i].c_str(), (unsigned)_SaveResult.GUIDs[i], (int)_SaveResult.NeighbourIDs[i]);
        Log += buf;
    }
}

bool CASScreen::ReloadEngineNeighbourhood(const std::string& userDataDir) {
    auto content = FSO::Content::Content::Get();
    if (!content || !content->Neighborhood) return false;
    auto& nb = *content->Neighborhood;
    std::string dir = userDataDir.empty() ? nb.UserPath : userDataDir;
    // InitSpecific(id): id 0 -> "UserData", id n -> "UserData<n+1>"
    int id = 0;
    auto slash = dir.find_last_of("/\\");
    std::string leaf = slash == std::string::npos ? dir : dir.substr(slash + 1);
    if (leaf.size() > 8 && (leaf.compare(0, 8, "UserData") == 0 || leaf.compare(0, 8, "userdata") == 0)) id = atoi(leaf.c_str() + 8) - 1;
    nb.InitSpecific(id);
    nb.LoadCharacters();
    return nb.MainResource != nullptr;
}

// ------------------------------------------------------------------------------------------- input

void CASScreen::Layout() {
    _Controls.clear();
    auto add = [this](const Rect& r, const std::string& id, bool en = true) { _Controls.push_back({ r, id, en }); };
    if (_Dialog.Open) {
        int n = (int)_Dialog.Buttons.size();
        for (int i = 0; i < n; i++) {
            int w = 76, gap = 12, total = n * w + (n - 1) * gap;
            add(Rect(160 - total / 2 + i * (w + gap), 158, w, 24), "dlg" + std::to_string(i));
        }
        return;
    }
    if (_Page == Page::Sim) {
        add(L::Name, "firstname");
        for (int t = 0; t < TraitCount; t++) add(Rect(L::Personality.X, L::TraitY0 + t * L::TraitDY - 3, L::Personality.W, L::TraitDY), "trait" + std::to_string(t));
        static const char* ids[7] = { "male", "female", "adult", "child", "light", "medium", "dark" };
        for (int i = 0; i < 7; i++) {
            Rect p = _Art.ClusterPos[i];
            add(Rect(L::ClusterAt.X + p.X, L::ClusterAt.Y + p.Y, p.W, p.H), ids[i]);
        }
        add(Rect(L::Arrows.X + 2, L::Arrows.Y + 16, 24, 30), "headprev");
        add(Rect(L::Arrows.X + 46, L::Arrows.Y + 16, 24, 30), "headnext");
        add(Rect(L::Arrows.X + 2, L::Arrows.Y + 88, 24, 30), "bodyprev");
        add(Rect(L::Arrows.X + 46, L::Arrows.Y + 88, 24, 30), "bodynext");
        add(L::Bio, "bio");
        add(L::Random, "random");
        add(L::Cancel, "cancel");
        add(L::Done, "done", !_Edit.FirstName.empty());
        return;
    }
    add(F::Name, "familyname");
    for (int i = 0; i < CASFamilyDesc::MaxMembers; i++) {
        int col = i % 4, row = i / 4;
        add(Rect(F::Members.X + 3 + col * 50, F::Members.Y + 3 + row * 60, 48, 58), "member" + std::to_string(i), i < (int)_Family.Members.size());
    }
    add(_Art.FamilyBtnPos[0], "add", (int)_Family.Members.size() < CASFamilyDesc::MaxMembers);
    add(_Art.FamilyBtnPos[1], "delete", _Selected >= 0 && _Selected < (int)_Family.Members.size());
    add(_Art.FamilyBtnPos[2], "edit", _Selected >= 0 && _Selected < (int)_Family.Members.size());
    add(F::Cancel, "cancel");
    add(F::Done, "done", _Family.CanFinish());
}

const CASScreen::Control* CASScreen::Hit(int x, int y) const {
    // later controls are drawn on top: search backwards
    for (auto it = _Controls.rbegin(); it != _Controls.rend(); ++it) if (it->R.Contains(x, y)) return &*it;
    return nullptr;
}

void CASScreen::OnTap(const std::string& id, int x, int y) {
    (void)y;
    if (id.compare(0, 5, "trait") != 0 && id.compare(0, 4, "head") != 0 && id.compare(0, 4, "body") != 0) {
        if (id == "random") CasSound("ui_cac_cycleparts");
        else CasSound("ui_click");
    }
    if (id.compare(0, 3, "dlg") == 0) {
        int b = atoi(id.c_str() + 3);
        auto cb = _Dialog.OnClose;
        _Dialog = Dialog();
        if (cb) cb(b);
        return;
    }
    if (_Page == Page::Sim) {
        if (id == "firstname" || id == "bio") EditText(id);
        else if (id.compare(0, 5, "trait") == 0) {
            int t = id[5] - '0';
            _TraitSel = t;
            _InfoTrait = t;
            int lx = L::Personality.X + L::LedX;
            if (x >= lx - 4) {
                int v = (int)std::ceil((x - lx) * 10.0 / L::LedW);
                v = std::max(0, std::min(10, v));
                if (v == _Edit.Traits[t] && v == 1) v = 0; // tapping the only lit LED clears it
                SetTrait(t, v);
            }
        }
        else if (id == "male") SetGender(CASGender::Male);
        else if (id == "female") SetGender(CASGender::Female);
        else if (id == "adult") SetAge(CASAge::Adult);
        else if (id == "child") SetAge(CASAge::Child);
        else if (id == "light") SetSkin(CASSkin::Light);
        else if (id == "medium") SetSkin(CASSkin::Medium);
        else if (id == "dark") SetSkin(CASSkin::Dark);
        else if (id == "headprev") StepHead(-1);
        else if (id == "headnext") StepHead(1);
        else if (id == "bodyprev") StepBody(-1);
        else if (id == "bodynext") StepBody(1);
        else if (id == "random") { RandomLook(); RandomPersonality(); }
        else if (id == "cancel") RequestCancel();
        else if (id == "done") RequestDone();
        return;
    }
    if (id == "familyname") EditText(id);
    else if (id.compare(0, 6, "member") == 0) {
        int i = atoi(id.c_str() + 6);
        if (i == _Selected) BeginEdit(i); // tap the selected Sim again to edit
        else _Selected = i;
    }
    else if (id == "add") BeginEdit(-1);
    else if (id == "edit") BeginEdit(_Selected);
    else if (id == "delete") {
        OpenDialog(_Art.Str(129, 5, "Delete Sim?"), _Art.Str(129, 6, "Are you sure you would like to delete this Sim?"),
                   { _Art.Str(152, 2, "Yes"), _Art.Str(152, 3, "No") }, [this](int b) {
                       if (b != 0 || _Selected < 0 || _Selected >= (int)_Family.Members.size()) return;
                       Log += "[cas] deleted " + _Family.Members[(size_t)_Selected].FirstName + "\n";
                       _Family.Members.erase(_Family.Members.begin() + _Selected);
                       if (_Selected < (int)_Lineup.size()) { _Lineup.erase(_Lineup.begin() + _Selected); _LineupPhase.erase(_LineupPhase.begin() + _Selected); }
                       if (_Selected >= (int)_Family.Members.size()) _Selected = (int)_Family.Members.size() - 1;
                   });
    }
    else if (id == "cancel") RequestCancel();
    else if (id == "done") RequestDone();
}

void CASScreen::OnButtons(const AppInput& in) {
    uint32_t d = in.Down;
    if (_Dialog.Open) {
        if (d & (BTN_A | BTN_START)) OnTap("dlg0", 0, 0);
        else if (d & BTN_B) OnTap("dlg" + std::to_string(std::max(0, (int)_Dialog.Buttons.size() - 1)), 0, 0);
        return;
    }
    if (_Page == Page::Sim) {
        if (d & BTN_UP) { _TraitSel = _TraitSel <= 0 ? TraitCount - 1 : _TraitSel - 1; _InfoTrait = _TraitSel; }
        if (d & BTN_DOWN) { _TraitSel = (_TraitSel + 1) % TraitCount; _InfoTrait = _TraitSel; }
        if (_TraitSel >= 0 && (d & BTN_LEFT)) SetTrait(_TraitSel, _Edit.Traits[_TraitSel] - 1);
        if (_TraitSel >= 0 && (d & BTN_RIGHT)) SetTrait(_TraitSel, _Edit.Traits[_TraitSel] + 1);
        if (d & BTN_X) RandomLook();
        if (d & BTN_Y) RandomPersonality();
        if (d & BTN_SELECT) EditText("firstname");
        if (d & (BTN_A | BTN_START)) RequestDone();
        else if (d & BTN_B) RequestCancel();
        return;
    }
    int n = (int)_Family.Members.size();
    if (n > 0 && (d & BTN_LEFT)) _Selected = _Selected <= 0 ? n - 1 : _Selected - 1;
    if (n > 0 && (d & BTN_RIGHT)) _Selected = (_Selected + 1) % n;
    if (d & BTN_X) { if (n < CASFamilyDesc::MaxMembers) BeginEdit(-1); return; }
    if (d & BTN_A) { if (_Selected >= 0 && _Selected < n) BeginEdit(_Selected); else if (n < CASFamilyDesc::MaxMembers) BeginEdit(-1); return; }
    if (d & BTN_Y) { if (_Selected >= 0 && _Selected < n) OnTap("delete", 0, 0); return; }
    if (d & BTN_SELECT) EditText("familyname");
    if (d & BTN_START) RequestDone();
    else if (d & BTN_B) RequestCancel();
}

void CASScreen::Update(const AppInput& in, double dtMs) {
    _Time += dtMs;
    _FrameNo++;
    if (!_Ready || _State == State::Finished || _State == State::Cancelled) return;
    if (_State == State::Saving) { SaveFamily(); return; }

    Layout();
    // touch: press on a control, release on the same control = tap; a drag that starts elsewhere turns the Sim
    if (in.TouchDown) {
        _Touching = true; _Dragging = false;
        _TouchX0 = _TouchLastX = in.TouchX;
        auto c = Hit(in.TouchX, in.TouchY);
        _Pressed = (c && c->Enabled) ? c->Id : std::string();
        // LED rows react immediately (and while dragging along them)
        if (_Pressed.compare(0, 5, "trait") == 0) OnTap(_Pressed, in.TouchX, in.TouchY);
    } else if (in.Touching && _Touching) {
        int dx = in.TouchX - _TouchLastX;
        if (_Pressed.compare(0, 5, "trait") == 0) { if (dx != 0) OnTap(_Pressed, in.TouchX, in.TouchY); }
        else if (std::abs(in.TouchX - _TouchX0) > 6 && !_Dialog.Open) {
            _Dragging = true; _Pressed.clear();
            _Rot += dx * 0.03f;
            _SinceManualRot = 0;
        }
        _TouchLastX = in.TouchX;
    }
    if (in.TouchUp && _Touching) {
        _Touching = false;
        auto c = Hit(in.TouchX, in.TouchY);
        if (!_Dragging && c && c->Enabled && c->Id == _Pressed && _Pressed.compare(0, 5, "trait") != 0) {
            std::string id = _Pressed;
            _Pressed.clear();
            OnTap(id, in.TouchX, in.TouchY);
        }
        _Pressed.clear();
        _Dragging = false;
    }
    OnButtons(in);

    // turning: circle pad / C-stick / L R; otherwise the turntable (FreeSims UISim.AutoRotate)
    float turn = in.CircleX + in.CStickX;
    if (in.Held & BTN_L) turn -= 1;
    if (in.Held & BTN_R) turn += 1;
    if (std::fabs(turn) > 0.15f) { _Rot += turn * (float)(dtMs * 0.0035); _SinceManualRot = 0; }
    else {
        _SinceManualRot += dtMs;
        if (_SinceManualRot > 2500) _Rot += (float)(dtMs * 0.00045);
    }
    if (_Rot > (float)M_PI * 2) _Rot -= (float)M_PI * 2;
    if (_Rot < 0) _Rot += (float)M_PI * 2;

    if (_Page == Page::Sim) _Preview.Update(dtMs);
    else for (auto& p : _Lineup) p->Update(dtMs);
}

// ------------------------------------------------------------------------------------------- drawing helpers

void CASScreen::DrawPanel(Canvas& c, const Rect& r) const {
    // the embossed dark-navy inset panels of the CAS backdrop
    c.FillRoundRect(r, 4, PanelFill);
    c.FillRect(r.X + 3, r.Y, r.W - 6, 1, PanelShade);
    c.FillRect(r.X, r.Y + 3, 1, r.H - 6, PanelShade);
    c.FillRect(r.X + 3, r.Y + r.H - 1, r.W - 6, 1, PanelLight);
    c.FillRect(r.X + r.W - 1, r.Y + 3, 1, r.H - 6, PanelLight);
}

void CASScreen::DrawSprite(Canvas& c, const CASSprite& s, int frame, int x, int y) const {
    if (s.Empty()) return;
    frame = std::max(0, std::min(frame, (int)s.Frames.size() - 1));
    auto& f = s.Frames[(size_t)frame];
    c.Blit(f.Px.data(), f.Width, f.Height, x, y, true);
}

void CASScreen::DrawArtButton(Canvas& c, const CASSprite& s, const Rect& r, const std::string& id, bool selected, bool enabled,
                              const std::string& label, int frameCount) const {
    bool pressed = !_Pressed.empty() && _Pressed == id && _Touching;
    if (!s.Empty()) {
        int frame = !enabled ? (frameCount == 6 ? 4 : 3) : pressed ? 2 : selected ? 1 : 0;
        DrawSprite(c, s, frame, r.X + (r.W - s.W()) / 2, r.Y + (r.H - s.H()) / 2);
        return;
    }
    // no UIGraphics.far: a plain button
    c.FillRoundRect(r, 5, !enabled ? RGBA(30, 40, 80) : pressed ? RGBA(200, 220, 255) : selected ? RGBA(0, 150, 200) : RGBA(16, 90, 170));
    c.StrokeRoundRect(r.X, r.Y, r.W, r.H, 5, RGBA(120, 170, 230));
    DrawTextCentered(c, FontId::Bold, r, Ellipsize(FontId::Bold, label, r.W - 4), enabled ? theme::TextBright : TextHint);
}

// Zodiac glyphs as strokes on a 16x16 grid centred on 0 (drawn 2px wide)
static const float* const* GlyphStrokes(int sign, int& count, const int*& lens) {
    static const float aries0[] = { -7,-2, -6,-6, -3,-6, 0,-2, 0,7 }, aries1[] = { 7,-2, 6,-6, 3,-6, 0,-2 };
    static const float taurus0[] = { -7,-7, -5,-4, -2,-2.5f, 2,-2.5f, 5,-4, 7,-7 }, taurus1[] = { 0,-2, 3,-1, 4.5f,2, 3,5.5f, 0,6.5f, -3,5.5f, -4.5f,2, -3,-1, 0,-2 };
    static const float gem0[] = { -6,-6, 0,-5, 6,-6 }, gem1[] = { -6,6, 0,5, 6,6 }, gem2[] = { -3,-5.4f, -3,5.4f }, gem3[] = { 3,-5.4f, 3,5.4f };
    static const float can0[] = { -7,-2, -4,-5, 2,-6, 7,-3 }, can1[] = { 7,2, 4,5, -2,6, -7,3 }, can2[] = { -6,-2, -4,0, -2,-2, -4,-4, -6,-2 }, can3[] = { 6,2, 4,0, 2,2, 4,4, 6,2 };
    static const float leo0[] = { -6,3, -4,0.5f, -1,1.5f, -1,4.5f, -4,5.5f, -6,3 }, leo1[] = { -1,2, -2,-3, 0,-6, 3,-6, 4.5f,-3, 2,2, 3,6, 6,6, 7,4 };
    static const float vir0[] = { -7,-5, -7,5 }, vir1[] = { -7,-3, -5,-5, -3,-3, -3,5 }, vir2[] = { -3,-3, -1,-5, 1,-3, 1,4, 3,6, 6,3, 4,1, 1,3 };
    static const float lib0[] = { -7,6, 7,6 }, lib1[] = { -7,2, -3,2, -3.5f,-1, -1.5f,-4.5f, 1.5f,-4.5f, 3.5f,-1, 3,2, 7,2 };
    static const float sco0[] = { -7,-5, -7,5 }, sco1[] = { -7,-3, -5,-5, -3,-3, -3,5 }, sco2[] = { -3,-3, -1,-5, 1,-3, 1,5, 4,5, 7,2 }, sco3[] = { 4,2, 7,2, 7,5 };
    static const float sag0[] = { -6,6, 6,-6 }, sag1[] = { 0,-6, 6,-6, 6,0 }, sag2[] = { -5,-1, 1,5 };
    static const float cap0[] = { -7,-5, -5,-5, -3,5, -1,-3, 1,-5, 3,-3, 3,3, 5,5, 7,3, 5,1, 3,3, 1,6, -2,6 };
    static const float aqu0[] = { -7,-1, -4,-4, -1,-1, 2,-4, 5,-1, 7,-3 }, aqu1[] = { -7,5, -4,2, -1,5, 2,2, 5,5, 7,3 };
    static const float pis0[] = { -6,-6, -3,-3, -2,0, -3,3, -6,6 }, pis1[] = { 6,-6, 3,-3, 2,0, 3,3, 6,6 }, pis2[] = { -6,0, 6,0 };
    static const float* aries[] = { aries0, aries1 }; static const int ariesL[] = { 5, 4 };
    static const float* taurus[] = { taurus0, taurus1 }; static const int taurusL[] = { 6, 9 };
    static const float* gem[] = { gem0, gem1, gem2, gem3 }; static const int gemL[] = { 3, 3, 2, 2 };
    static const float* can[] = { can0, can1, can2, can3 }; static const int canL[] = { 4, 4, 5, 5 };
    static const float* leo[] = { leo0, leo1 }; static const int leoL[] = { 6, 9 };
    static const float* vir[] = { vir0, vir1, vir2 }; static const int virL[] = { 2, 4, 8 };
    static const float* lib[] = { lib0, lib1 }; static const int libL[] = { 2, 8 };
    static const float* sco[] = { sco0, sco1, sco2, sco3 }; static const int scoL[] = { 2, 4, 6, 3 };
    static const float* sag[] = { sag0, sag1, sag2 }; static const int sagL[] = { 2, 3, 2 };
    static const float* cap[] = { cap0 }; static const int capL[] = { 13 };
    static const float* aqu[] = { aqu0, aqu1 }; static const int aquL[] = { 6, 6 };
    static const float* pis[] = { pis0, pis1, pis2 }; static const int pisL[] = { 5, 5, 2 };
    static const float* const* all[12] = { aries, taurus, gem, can, leo, vir, lib, sco, sag, cap, aqu, pis };
    static const int* allL[12] = { ariesL, taurusL, gemL, canL, leoL, virL, libL, scoL, sagL, capL, aquL, pisL };
    static const int counts[12] = { 2, 2, 4, 4, 2, 3, 2, 4, 3, 1, 2, 3 };
    count = counts[sign];
    lens = allL[sign];
    return all[sign];
}

void CASScreen::DrawZodiac(Canvas& c, int cx, int cy, int r, int sign) const {
    // a round "medallion" in the CAS colours with the sign's glyph
    c.FillCircle(cx, cy, r, RGBA(0, 60, 150));
    c.FillCircle(cx, cy, r - 2, RGBA(0, 20, 80));
    c.StrokeCircle(cx, cy, r, RGBA(120, 190, 255));
    c.StrokeCircle(cx, cy, r - 2, RGBA(40, 90, 170));
    if (sign < 0 || sign >= ZodiacCount) { DrawTextCentered(c, FontId::Title, Rect(cx - r, cy - r, 2 * r, 2 * r), "?", TextCyan); return; }
    int count = 0; const int* lens = nullptr;
    auto strokes = GlyphStrokes(sign, count, lens);
    float s = r / 11.0f;
    for (int k = 0; k < count; k++)
        for (int i = 0; i + 1 < lens[k]; i++) {
            float x0 = cx + strokes[k][i * 2] * s, y0 = cy + strokes[k][i * 2 + 1] * s;
            float x1 = cx + strokes[k][i * 2 + 2] * s, y1 = cy + strokes[k][i * 2 + 3] * s;
            for (int ox = 0; ox < 2; ox++) for (int oy = 0; oy < 2; oy++)
                c.Line((int)std::lround(x0) + ox, (int)std::lround(y0) + oy, (int)std::lround(x1) + ox, (int)std::lround(y1) + oy, TextCyan);
        }
}

void CASScreen::DrawDice(Canvas& c, const Rect& r, bool pressed) const {
    int s = std::min(r.W, r.H) - 4;
    int x = r.X + 4, y = r.Y + (r.H - s) / 2;
    c.FillRoundRect(x, y, s, s, 4, pressed ? RGBA(0, 220, 255) : RGBA(235, 240, 250));
    c.StrokeRoundRect(x, y, s, s, 4, RGBA(20, 40, 90));
    int pip = std::max(2, s / 7);
    auto dot = [&](float fx, float fy) { c.FillCircle(x + (int)(fx * s), y + (int)(fy * s), pip, RGBA(10, 30, 90)); };
    dot(0.27f, 0.27f); dot(0.73f, 0.73f); dot(0.5f, 0.5f); dot(0.73f, 0.27f); dot(0.27f, 0.73f);
}

std::string CASScreen::TraitName(int t) const {
    static const char* fb[TraitCount] = { "Neat", "Outgoing", "Active", "Playful", "Nice" };
    return _Art.Str(130, 17 + t, fb[t]);
}

std::string CASScreen::FullName(const CASSimDesc& s) const {
    std::string n = s.FirstName;
    if (!_Family.LastName.empty()) n += (n.empty() ? "" : " ") + _Family.LastName;
    return n;
}

static int Wrapped(Canvas& c, FontId f, const Rect& r, const std::string& text, uint32_t col, int maxLines) {
    auto lines = WrapText(f, text, r.W);
    int lh = LineHeight(f), y = r.Y, n = 0;
    for (auto& l : lines) {
        if (n >= maxLines || y + lh > r.Y + r.H) break;
        std::string s = l;
        if (n == maxLines - 1 && (size_t)(n + 1) < lines.size()) s = Ellipsize(f, s + " ...", r.W);
        DrawText(c, f, r.X, y, s, col);
        y += lh; n++;
    }
    return y;
}

// ------------------------------------------------------------------------------------------- Create A Sim

void CASScreen::DrawSimTop(Canvas& c) {
    if (_Art.TopBg.Width) c.Blit(_Art.TopBg.Px.data(), 400, 240, 0, 0, false); else c.Clear(theme::ScreenBg);
    // the Sim panel (the lit frame of the original CAS backdrop)
    Rect panel(4, 4, 166, 233);
    if (_Art.PreviewPanel.Width) c.Blit(_Art.PreviewPanel.Px.data(), _Art.PreviewPanel.Width, _Art.PreviewPanel.Height, panel.X, panel.Y, false);
    else { DrawPanel(c, panel); c.GradientV(panel.X + 6, panel.Y + 8, panel.W - 12, panel.H - 16, RGBA(110, 120, 175), RGBA(30, 32, 90)); }
    float footX = panel.X + panel.W / 2.0f, footY = panel.Y + panel.H - 22.0f;
    // soft floor shadow under the feet
    for (int i = 0; i < 3; i++) {
        int rx = 30 - i * 7, ry = 6 - i;
        for (int y = -ry; y <= ry; y++) {
            int w = (int)(rx * std::sqrt(1.0 - (double)(y * y) / (ry * ry + 0.01)));
            c.FillRect((int)footX - w, (int)footY + y + 1, 2 * w, 1, RGBA(0, 0, 30, 40));
        }
    }
    float heightPx = 200.0f * (_Preview.StandingHeight() / 5.6f);
    _Preview.Draw(c, Rect(panel.X + 7, panel.Y + 7, panel.W - 14, panel.H - 14), footX, footY, heightPx, _Rot, _Cfg.Bilinear);

    // right: title, name, sign, personality / bio
    Rect title(176, 4, 220, 22);
    DrawPanel(c, title);
    DrawTextCentered(c, FontId::Bold, title, _Art.Str(130, 15, "CREATE A SIM"), theme::TextBright);
    Rect name(176, 30, 220, 26);
    DrawPanel(c, name);
    if (_Edit.FirstName.empty()) DrawTextCentered(c, FontId::Bold, name, _Art.Str(130, 26, "Enter First Name:"), TextHint);
    else DrawTextCentered(c, FontId::Bold, name, Ellipsize(FontId::Bold, FullName(_Edit), name.W - 8), theme::TextBright);

    Rect sign(176, 60, 220, 50);
    DrawPanel(c, sign);
    int z = _Edit.Zodiac();
    DrawZodiac(c, sign.X + 25, sign.Y + 25, 20, z);
    static const char* signFb[12] = { "Aries", "Taurus", "Gemini", "Cancer", "Leo", "Virgo", "Libra", "Scorpio", "Sagittarius", "Capricorn", "Aquarius", "Pisces" };
    DrawText(c, FontId::Title, sign.X + 52, sign.Y + 5, z >= 0 ? _Art.Str(164, z, signFb[z]) : std::string("?"), theme::TextBright);
    DrawText(c, FontId::Small, sign.X + 54, sign.Y + 30, _Art.Str(130, 27, "Astrological Sign"), TextHint);

    Rect info(176, 114, 220, 122);
    DrawPanel(c, info);
    Rect in(info.X + 7, info.Y + 5, info.W - 14, info.H - 10);
    if (_InfoTrait >= 0) {
        int t = _InfoTrait, v = _Edit.Traits[t];
        int level = v <= 3 ? 0 : v <= 6 ? 1 : 2;
        int base = 1 + t * 7;
        static const char* lvl[3] = { "Low", "Medium", "High" };
        std::string head = _Art.Personality(base, TraitName(t).c_str()) + ": " + _Art.Personality(base + 1 + level, lvl[level]);
        DrawText(c, FontId::Bold, in.X, in.Y, Ellipsize(FontId::Bold, head, in.W), theme::TextYellow);
        // LEDs of this trait
        for (int i = 0; i < 10; i++) c.FillRect(in.X + i * 9, in.Y + 18, 7, 5, i < v ? RGBA(0, 230, 255) : RGBA(30, 34, 90));
        Wrapped(c, FontId::Small, Rect(in.X, in.Y + 27, in.W, in.H - 27), _Art.Personality(base + 4 + level, ""), theme::Text, 6);
    } else {
        std::string h = _Art.Str(130, 24, "BIO");
        if (!_Edit.FirstName.empty()) {
            h = _Art.Str(130, 25, "Bio for %s");
            auto p = h.find("%s");
            if (p != std::string::npos) h.replace(p, 2, _Edit.FirstName);
        }
        DrawText(c, FontId::Bold, in.X, in.Y, h, theme::TextYellow);
        Wrapped(c, FontId::Small, Rect(in.X, in.Y + 18, in.W, in.H - 18), _Edit.Bio.empty() ? std::string("-") : _Edit.Bio, theme::Text, 7);
    }
}

void CASScreen::DrawSimBottom(Canvas& c) {
    if (_Art.BottomBg.Width) c.Blit(_Art.BottomBg.Px.data(), 320, 240, 0, 0, false); else c.Clear(theme::ScreenBg);
    // first name
    DrawPanel(c, L::Name);
    if (_Edit.FirstName.empty()) DrawText(c, FontId::Bold, L::Name.X + 7, L::Name.Y + 4, _Art.Str(130, 26, "Enter First Name:"), TextHint);
    else DrawText(c, FontId::Bold, L::Name.X + 7, L::Name.Y + 4, Ellipsize(FontId::Bold, _Edit.FirstName, L::Name.W - 14), theme::TextBright);
    // points left (numeric) next to it
    DrawPanel(c, L::Points);
    {
        std::string s = std::to_string(_Edit.PointsLeft());
        DrawText(c, FontId::Small, L::Points.X + 6, L::Points.Y + 5, _Art.Str(130, 16, "PERSONALITY"), TextHint);
        DrawText(c, FontId::Bold, L::Points.X + L::Points.W - 6 - TextWidth(FontId::Bold, s), L::Points.Y + 4, s,
                 _Edit.PointsLeft() > 0 ? TextCyan : theme::TextDim);
    }
    // personality: label + 10 LEDs per trait, then the points-remaining bar
    DrawPanel(c, L::Personality);
    for (int t = 0; t < TraitCount; t++) {
        int y = L::TraitY0 + t * L::TraitDY;
        bool sel = t == _TraitSel;
        if (sel) c.FillRoundRect(L::Personality.X + 3, y - 3, L::Personality.W - 6, L::TraitDY - 1, 3, RGBA(20, 40, 120));
        DrawText(c, FontId::Small, L::Personality.X + 6, y, TraitName(t), sel ? theme::TextBright : theme::Text);
        int lx = L::Personality.X + L::LedX;
        int lit = _Edit.Traits[t] * L::LedW / 10;
        if (_Art.LEDOff.Width) {
            c.Blit(_Art.LEDOff.Px.data(), _Art.LEDOff.Width, _Art.LEDOff.Height, lx, y, false);
            if (lit > 0 && _Art.LEDOn.Width)
                for (int yy = 0; yy < L::LedH; yy++) for (int xx = 0; xx < lit; xx++) c.Px[(size_t)(y + yy) * c.Width + lx + xx] = _Art.LEDOn.Px[(size_t)yy * _Art.LEDOn.Width + xx];
        } else {
            for (int i = 0; i < 10; i++) c.FillRect(lx + i * 8 + 1, y, 6, L::LedH, i < _Edit.Traits[t] ? RGBA(0, 230, 255) : RGBA(40, 44, 100));
        }
    }
    {
        int lit = _Edit.PointsLeft() * L::PointsBar.W / PersonalityPoints;
        if (_Art.PointsOff.Width) {
            c.Blit(_Art.PointsOff.Px.data(), _Art.PointsOff.Width, _Art.PointsOff.Height, L::PointsBar.X, L::PointsBar.Y, false);
            if (lit > 0 && _Art.PointsOn.Width)
                for (int yy = 0; yy < L::PointsBar.H; yy++) for (int xx = 0; xx < lit; xx++)
                    c.Px[(size_t)(L::PointsBar.Y + yy) * c.Width + L::PointsBar.X + xx] = _Art.PointsOn.Px[(size_t)yy * _Art.PointsOn.Width + xx];
        } else {
            c.FillRect(L::PointsBar, RGBA(40, 44, 100));
            c.FillRect(L::PointsBar.X, L::PointsBar.Y, lit, L::PointsBar.H, RGBA(0, 230, 255));
        }
    }
    // the quarter-circle cluster: age, skin, sex
    if (_Art.Cluster.Width) c.Blit(_Art.Cluster.Px.data(), _Art.Cluster.Width, _Art.Cluster.Height, L::ClusterAt.X, L::ClusterAt.Y, false);
    const CASSprite* spr[7] = { &_Art.Male, &_Art.Female, &_Art.Adult, &_Art.Child, &_Art.Light, &_Art.Medium, &_Art.Dark };
    static const char* ids[7] = { "male", "female", "adult", "child", "light", "medium", "dark" };
    static const char* fb[7] = { "M", "F", "Adult", "Child", "L", "M", "D" };
    static const int strIdx[7] = { 3, 2, 0, 1, 6, 5, 4 };
    bool selected[7] = { _Edit.Gender == CASGender::Male, _Edit.Gender == CASGender::Female, _Edit.Age == CASAge::Adult,
                         _Edit.Age == CASAge::Child, _Edit.Skin == CASSkin::Light, _Edit.Skin == CASSkin::Medium, _Edit.Skin == CASSkin::Dark };
    for (int i = 0; i < 7; i++) {
        Rect p = _Art.ClusterPos[i];
        Rect r(L::ClusterAt.X + p.X, L::ClusterAt.Y + p.Y, p.W, p.H);
        DrawArtButton(c, *spr[i], r, ids[i], selected[i], true, _Art.Str(130, strIdx[i], fb[i]));
    }
    // head / body arrows with the position in the catalogue
    DrawPanel(c, L::Arrows);
    for (int k = 0; k < 2; k++) {
        int y = L::Arrows.Y + 4 + k * 72;
        std::string lab = k == 0 ? "Head" : "Body";
        DrawTextCentered(c, FontId::Bold, Rect(L::Arrows.X, y, L::Arrows.W, 14), lab, theme::TextBright);
        std::string pid = k == 0 ? "headprev" : "bodyprev", nid = k == 0 ? "headnext" : "bodynext";
        DrawArtButton(c, _Art.ArrowL, Rect(L::Arrows.X + 2, y + 12, 24, 30), pid, false, true, "<");
        DrawArtButton(c, _Art.ArrowR, Rect(L::Arrows.X + 46, y + 12, 24, 30), nid, false, true, ">");
        int idx = k == 0 ? _HeadIdx : _BodyIdx;
        size_t n = k == 0 ? Heads().size() : Bodies().size();
        std::string cnt = std::to_string(idx + 1);
        std::string tot = std::to_string(n);
        DrawTextCentered(c, FontId::Small, Rect(L::Arrows.X + 22, y + 14, 28, 13), cnt, TextCyan);
        DrawTextCentered(c, FontId::Small, Rect(L::Arrows.X + 22, y + 28, 28, 13), tot, TextHint);
        // the skin's own name, e.g. "baldbeard01"
        const auto& o = k == 0 ? _Edit.Head : _Edit.Body;
        std::string nm = o.Texture.size() > 10 ? o.Texture.substr(o.Texture.find('_') + 1) : o.Texture;
        DrawTextCentered(c, FontId::Small, Rect(L::Arrows.X + 2, y + 44, L::Arrows.W - 4, 13), Ellipsize(FontId::Small, nm, L::Arrows.W - 6), theme::TextDim);
    }
    // bio
    DrawPanel(c, L::Bio);
    {
        std::string h = _Art.Str(130, 24, "BIO");
        if (!_Edit.FirstName.empty()) { h = _Art.Str(130, 25, "Bio for %s"); auto p = h.find("%s"); if (p != std::string::npos) h.replace(p, 2, _Edit.FirstName); }
        DrawText(c, FontId::Small, L::Bio.X + 6, L::Bio.Y + 3, h, TextHint);
        Wrapped(c, FontId::Small, Rect(L::Bio.X + 6, L::Bio.Y + 17, L::Bio.W - 12, 27), _Edit.Bio.empty() ? std::string("...") : _Edit.Bio, theme::TextBright, 2);
    }
    // random
    {
        bool pr = _Pressed == "random" && _Touching;
        DrawPanel(c, L::Random);
        DrawDice(c, Rect(L::Random.X, L::Random.Y + 1, 24, 23), pr);
        DrawText(c, FontId::Bold, L::Random.X + 28, L::Random.Y + 5, "Random", pr ? TextCyan : theme::TextBright);
    }
    // done / cancel
    DrawArtButton(c, _Art.SimCancel, L::Cancel, "cancel", false, true, _Art.Str(130, 12, "Cancel"), 6);
    DrawArtButton(c, _Art.SimDone, L::Done, "done", false, !_Edit.FirstName.empty(), _Art.Str(130, 11, "Done"), 6);
}

// ------------------------------------------------------------------------------------------- Create A Family

void CASScreen::DrawFamilyTop(Canvas& c) {
    if (_Art.FamilyTop.Width) c.Blit(_Art.FamilyTop.Px.data(), 400, 240, 0, 0, false);
    else { c.Clear(theme::ScreenBg); DrawPanel(c, Rect(4, 4, 392, 232)); }
    std::string title = _Family.LastName.empty() ? _Art.Str(129, 9, "CREATE A FAMILY") : _Family.LastName;
    DrawTextCentered(c, FontId::Title, Rect(0, 8, 400, 24), title, _Family.LastName.empty() ? TextHint : theme::TextBright);
    std::string funds = Money(_Family.Budget);
    DrawText(c, FontId::Bold, 392 - TextWidth(FontId::Bold, funds) - 8, 12, funds, RGBA(150, 230, 120));
    int n = (int)_Family.Members.size();
    if (n == 0) {
        DrawTextCentered(c, FontId::Bold, Rect(0, 100, 400, 20), _Art.Str(129, 0, "Add a New Sim"), TextCyan);
        DrawTextCentered(c, FontId::Small, Rect(0, 122, 400, 16), "Tap the  +  button on the touch screen", TextHint);
        return;
    }
    SyncLineup();
    float slot = std::min(96.0f, 384.0f / n);
    float x0 = 200 - slot * n / 2 + slot / 2;
    float scale = n <= 4 ? 1.0f : n <= 6 ? 0.86f : 0.74f;
    for (int i = 0; i < n; i++) {
        auto& p = *_Lineup[(size_t)i];
        float fx = x0 + i * slot, fy = 206;
        bool sel = i == _Selected;
        if (sel) {
            for (int y = -5; y <= 5; y++) { int w = (int)(26 * std::sqrt(1.0 - y * y / 26.0)); c.FillRect((int)fx - w, (int)fy + y, 2 * w, 1, RGBA(0, 200, 255, 50)); }
        }
        float h = 158.0f * scale * (p.StandingHeight() / 5.6f);
        auto r = p.Draw(c, Rect((int)(fx - slot / 2), 30, (int)slot, 200), fx, fy, h, 0.0f, false);
        if (sel && r.MaxY >= r.MinY) {
            // the plumbob over the selected Sim
            int px = (int)fx, py = r.MinY - 14 + (int)(2 * std::sin(_Time * 0.004));
            c.FillTriangle(px, py - 9, px - 6, py, px + 6, py, theme::Plumbob);
            c.FillTriangle(px - 6, py, px + 6, py, px, py + 10, theme::PlumbobDark);
            c.Line(px, py - 9, px, py + 10, RGBA(170, 255, 140));
        }
        std::string nm = Ellipsize(FontId::Small, _Family.Members[(size_t)i].FirstName, (int)slot - 2);
        DrawTextCentered(c, FontId::Small, Rect((int)(fx - slot / 2), 214, (int)slot, 14), nm, sel ? theme::TextBright : theme::Text);
    }
}

void CASScreen::DrawFamilyBottom(Canvas& c) {
    if (_Art.FamilyBottom.Width) c.Blit(_Art.FamilyBottom.Px.data(), 320, 240, 0, 0, false);
    else { c.Clear(theme::ScreenBg); DrawPanel(c, F::Name); DrawPanel(c, F::Members); }
    // last name
    if (_Family.LastName.empty()) DrawTextCentered(c, FontId::Small, F::Name, _Art.Str(129, 12, "Enter Last Name:"), TextHint);
    else DrawTextCentered(c, FontId::Bold, Rect(F::Name.X, F::Name.Y - 1, F::Name.W, F::Name.H), Ellipsize(FontId::Bold, _Family.LastName, F::Name.W - 4), theme::TextBright);
    // members: portraits in two rows of four
    for (int i = 0; i < CASFamilyDesc::MaxMembers; i++) {
        int col = i % 4, row = i / 4;
        Rect cell(F::Members.X + 3 + col * 50, F::Members.Y + 3 + row * 60, 48, 58);
        if (i >= (int)_Family.Members.size()) {
            c.StrokeRoundRect(cell.X + 4, cell.Y + 4, 40, 40, 4, RGBA(30, 34, 90));
            continue;
        }
        bool sel = i == _Selected;
        Rect face(cell.X + 4, cell.Y + 2, 40, 40);
        c.FillRoundRect(face, 4, sel ? RGBA(0, 90, 160) : RGBA(16, 24, 80));
        _Lineup[(size_t)i]->DrawHead(c, Rect(face.X + 1, face.Y + 1, face.W - 2, face.H - 2), face.X + 20.0f, face.Y + 22.0f, 40.0f, 0.0f, false);
        c.StrokeRoundRect(face.X, face.Y, face.W, face.H, 4, sel ? RGBA(0, 255, 255) : RGBA(70, 90, 170));
        DrawTextCentered(c, FontId::Small, Rect(cell.X - 1, cell.Y + 43, cell.W + 2, 13),
                         Ellipsize(FontId::Small, _Family.Members[(size_t)i].FirstName, cell.W + 2), sel ? theme::TextBright : theme::Text);
    }
    // Add / Delete / Edit
    bool haveSel = _Selected >= 0 && _Selected < (int)_Family.Members.size();
    DrawArtButton(c, _Art.Add, _Art.FamilyBtnPos[0], "add", false, (int)_Family.Members.size() < CASFamilyDesc::MaxMembers, "+");
    DrawArtButton(c, _Art.Delete, _Art.FamilyBtnPos[1], "delete", false, haveSel, "Del");
    DrawArtButton(c, _Art.Edit, _Art.FamilyBtnPos[2], "edit", false, haveSel, "Edit");
    // the selected Sim's summary
    DrawPanel(c, F::Info);
    if (haveSel) {
        auto& m = _Family.Members[(size_t)_Selected];
        std::string line = FullName(m) + "  -  " + _Art.Str(130, m.Age == CASAge::Adult ? 0 : 1, m.Age == CASAge::Adult ? "Adult" : "Child") + " " +
                           _Art.Str(130, m.Gender == CASGender::Male ? 3 : 2, m.Gender == CASGender::Male ? "Male" : "Female");
        DrawText(c, FontId::Bold, F::Info.X + 6, F::Info.Y + 3, Ellipsize(FontId::Bold, line, F::Info.W - 12), theme::TextBright);
        int x = F::Info.X + 6;
        for (int t = 0; t < TraitCount; t++) {
            std::string s = TraitName(t).substr(0, 4) + " " + std::to_string(m.Traits[t]);
            DrawText(c, FontId::Small, x, F::Info.Y + 21, s, theme::Text);
            x += 52;
        }
        int z = m.Zodiac();
        if (z >= 0) DrawZodiac(c, F::Info.X + F::Info.W - 12, F::Info.Y + 26, 9, z);
    } else {
        std::string s = _Family.Members.empty() ? _Art.Str(129, 0, "Add a New Sim") : _Art.Str(128, 12, "Personality for %s");
        auto p = s.find("%s"); if (p != std::string::npos) s.replace(p, 2, "...");
        DrawTextCentered(c, FontId::Small, F::Info, s, TextHint);
    }
    // funds + Done / Cancel
    DrawArtButton(c, _Art.FamCancel, F::Cancel, "cancel", false, true, _Art.Str(129, 4, "Cancel"), 6);
    DrawArtButton(c, _Art.FamDone, F::Done, "done", false, _Family.CanFinish(), _Art.Str(129, 3, "Done"), 6);
    std::string funds = Money(_Family.Budget);
    DrawTextCentered(c, FontId::Bold, Rect(114, 216, 92, 16), funds, RGBA(150, 230, 120));
}

void CASScreen::DrawDialog(Canvas& c) {
    c.Dim(0, 0, c.Width, c.Height, 120);
    Rect r(24, 40, 272, 150);
    DrawPanel(c, r);
    c.StrokeRoundRect(r.X, r.Y, r.W, r.H, 4, RGBA(90, 120, 220));
    DrawTextCentered(c, FontId::Bold, Rect(r.X, r.Y + 6, r.W, 16), _Dialog.Title, theme::TextYellow);
    Wrapped(c, FontId::Small, Rect(r.X + 12, r.Y + 28, r.W - 24, 84), _Dialog.Text, theme::TextBright, 6);
    int n = (int)_Dialog.Buttons.size();
    for (int i = 0; i < n; i++) {
        int w = 76, gap = 12, total = n * w + (n - 1) * gap;
        Rect b(160 - total / 2 + i * (w + gap), 158, w, 24);
        bool pr = _Touching && _Pressed == "dlg" + std::to_string(i);
        c.FillRoundRect(b, 6, pr ? RGBA(0, 200, 255) : RGBA(16, 90, 170));
        c.StrokeRoundRect(b.X, b.Y, b.W, b.H, 6, RGBA(140, 190, 255));
        DrawTextCentered(c, FontId::Bold, b, _Dialog.Buttons[(size_t)i], theme::TextBright);
    }
}

void CASScreen::Draw(Canvas& top, Canvas& bottom) {
    if (top.Width != 400 || top.Height != 240) top.Resize(400, 240);
    if (bottom.Width != 320 || bottom.Height != 240) bottom.Resize(320, 240);
    if (!_Ready) {
        top.Clear(theme::ScreenBg); bottom.Clear(theme::ScreenBg);
        DrawTextCentered(top, FontId::Bold, Rect(0, 100, 400, 20), "Create A Family is unavailable", theme::TextYellow);
        Wrapped(bottom, FontId::Small, Rect(10, 90, 300, 60), Error, theme::Text, 4);
        return;
    }
    Layout();
    if (_Page == Page::Sim) { DrawSimTop(top); DrawSimBottom(bottom); }
    else {
        int skip = std::max(1, _Cfg.LineupFrameSkip);
        if (skip > 1 && (_FrameNo % skip) != 0 && _TopCache.Width == 400 && !_Touching) top.Px = _TopCache.Px;
        else { DrawFamilyTop(top); if (skip > 1) _TopCache = top; }
        DrawFamilyBottom(bottom);
    }
    if (_State == State::Saving) {
        bottom.Dim(0, 0, 320, 240, 120);
        DrawTextCentered(bottom, FontId::Bold, Rect(0, 110, 320, 20), "Saving...", theme::TextYellow);
    }
    if (_Dialog.Open) DrawDialog(bottom);
}

// ------------------------------------------------------------------------------------------- scripting

bool CASScreen::ScriptButtonPos(const std::string& id, int& x, int& y) {
    Layout();
    // led<t>_<v>: the v-th LED of trait t
    if (id.size() >= 6 && id.compare(0, 3, "led") == 0 && _Page == Page::Sim && !_Dialog.Open) {
        int t = id[3] - '0', v = atoi(id.c_str() + 5);
        if (t < 0 || t >= TraitCount) return false;
        x = L::Personality.X + L::LedX + (int)((v - 0.5) * L::LedW / 10.0);
        y = L::TraitY0 + t * L::TraitDY + L::LedH / 2;
        return true;
    }
    for (auto& c : _Controls)
        if (c.Id == id) { x = c.R.X + c.R.W / 2; y = c.R.Y + c.R.H / 2; return true; }
    return false;
}

std::string CASScreen::Describe() const {
    std::ostringstream o;
    o << "page " << (_Page == Page::Sim ? "sim" : "family") << ", family '" << _Family.LastName << "' (" << _Family.Members.size() << " Sims, "
      << Money(_Family.Budget) << ")" << (_Dialog.Open ? ", dialog '" + _Dialog.Title + "'" : std::string()) << "\n";
    for (size_t i = 0; i < _Family.Members.size(); i++) {
        auto& m = _Family.Members[i];
        o << "  " << i << ": " << m.FirstName << " " << (m.Age == CASAge::Adult ? "adult" : "child") << " " << (m.Gender == CASGender::Male ? "male" : "female")
          << " " << SkinCode(m.Skin) << " head " << m.Head.Texture << " body " << m.Body.Texture << " traits";
        for (int t = 0; t < TraitCount; t++) o << " " << m.Traits[t];
        o << " sign " << m.Zodiac() << "\n";
    }
    if (_Page == Page::Sim) {
        o << "  editing: " << _Edit.FirstName << " head " << _HeadIdx << "/" << Heads().size() << " " << _Edit.Head.Texture << " body " << _BodyIdx << "/"
          << Bodies().size() << " " << _Edit.Body.Texture << " points left " << _Edit.PointsLeft() << " sign " << _Edit.Zodiac() << "\n";
    }
    return o.str();
}

}}
