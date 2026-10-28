/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims app — see LotScreen.h
#include "app/LotScreen.h"
#include <cstring>
#include "files/formats/iff/chunks/STR.h"
#include "app/cas/CASFamilyWriter.h"
#include "app/cas/CASSimPreview.h"
#include "files/formats/iff/chunks/CTSS.h"
#include "audio/GameAudio.h"
#include "app/UISounds.h"
#include "app/Font.h"
#include "app/Theme.h"
#include "app/WorldPicking.h"
#include "app/ui/UIGraphics.h"
#include "app/ui/GameText.h"
#include "app/ui/UIModePanel.h"
#include "app/ui/UIUCP.h"
#include "app/ui/UIOptionsPanel.h"
#include "app/ui/CheatState.h"
#include "simantics/engine/VMRoutingFrame.h"
#include "simantics/VMArchitecture.h"
#include "simantics/model/VMArchitectureTerrain.h"
#include "app/live/UILiveMode.h"
#include "app/live/UIHeadlineRenderer.h"
#include "app/buy/UIBuyMode.h"
#include "app/buy/UIObjectHolder.h"
#include "app/build/UIBuildMode.h"
#include "content/ContentManager.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"
#include "simantics/VMArchitecture.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/entities/VMGameObject.h"
#include "simantics/entities/VMMultitileGroup.h"
#include "simantics/engine/VMThread.h"
#include "simantics/engine/VMQueuedAction.h"
#include "simantics/model/VMPersonDataVariable.h"
#include "files/formats/iff/chunks/BMP.h"
#include "files/formats/iff/chunks/OBJD.h"
#include "world/World.h"
#include "world/model/Blueprint.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

namespace simapp {

using namespace FSO::SimAntics;

static const int16_t SELECT_SIM_PARAM = -32768; // pie entry "Select <name>" (not a TTAB interaction)

const Rect LotScreen::GadgetRect(0, 0, 48, 240);
const Rect LotScreen::PanelRect(48, 0, 272, 240);

static std::string Lower(const std::string& s) {
    std::string r = s;
    for (auto& c : r) c = (char)std::tolower((unsigned char)c);
    return r;
}

static std::string FirstName(const std::string& n) {
    size_t s = n.find(' ');
    return s == std::string::npos ? n : n.substr(0, s);
}

LotScreen::LotScreen(LotSession* session, UIStrings* str, Canvas* top, Canvas* bottom)
    : S(session), Str(str), Top(top), Bottom(bottom) {
    auto content = FSO::Content::Content::Get();
    std::string root = content ? content->SimsCompleteDir : std::string(".");
    UIGraphics::Get().Init(root);
    GameText::Get().Load(root + "/GameData");
    UIHeadlineRenderer::LoadSprites(root + "/GameData");
    UIHeadlineRenderer::IconProvider = [this](VMEntity* e) { return IconFor(e); };
    Holder.reset(new UIObjectHolder(S));
    Holder->OnFunds = [this](int delta, VMEntity* at) { MoneyPopup(delta, at); };
    Live.reset(new UILiveMode(this));
    Buy.reset(new UIBuyMode(this));
    Build.reset(new UIBuildMode(this));
    Options.reset(new UIOptionsPanel(this));
    _LastBudget = S->Budget();
    Live->Activate();
}

LotScreen::~LotScreen() {
    Pie.Close();
    if (Holder) Holder->ClearSelected();
    UIHeadlineRenderer::IconProvider = nullptr;
}

// TS1(sims): the selected Sim's plumbob. TS1 draws it from the executable (no sprite in Sprites.iff, no mesh in
// GameData): a green, vertically stretched octahedron spinning about its vertical axis, its facets shaded by a light
// from the upper left, always the same green in TS1 (mood colours came with The Sims 2). Four equator corners at
// angle a + k*90 degrees; a facet is drawn when its outward normal faces the viewer, back to front.
static void DrawPlumbob(Canvas& c, float cx, float cy, float halfW, float halfH, float angle) {
    const float PI = 3.14159265f;
    float lx = -0.55f, ly = -0.6f, lz = 0.58f; // light direction (towards the light; x right, y up, z to viewer)
    struct Facet { float depth; int x0, y0, x1, y1, x2, y2; uint32_t col; };
    Facet f[8]; int n = 0;
    for (int k = 0; k < 4; k++) {
        float a0 = angle + k * PI * 0.5f, a1 = a0 + PI * 0.5f, am = a0 + PI * 0.25f;
        float x0 = std::cos(a0) * halfW, z0 = std::sin(a0), x1 = std::cos(a1) * halfW, z1 = std::sin(a1);
        float zm = std::sin(am); // the facet pair's horizontal facing (toward the viewer when > 0)
        if (zm <= -0.05f) continue;
        for (int half = 0; half < 2; half++) { // 0 = upper facet, 1 = lower
            // facet normal: horizontal part along (cos am, sin am) scaled, vertical part up/down
            float hx = std::cos(am) * 0.70710678f * halfH, hz = zm * 0.70710678f * halfH;
            float ny = (half == 0 ? 1.0f : -1.0f) * 0.70710678f * halfW * 0.9f;
            float len = std::sqrt(hx * hx + hz * hz + ny * ny);
            float d = (hx * lx + ny * ly * -1.0f + hz * lz) / (len > 0 ? len : 1); // y up in light space
            float l = 0.35f + 0.65f * std::max(0.0f, d);
            if (half == 1) l *= 0.78f;
            float r = 60 * l + 20, g = 255 * l, b = 50 * l + 10;
            Facet& ff = f[n++];
            ff.depth = zm;
            ff.x0 = (int)std::lround(cx + x0); ff.y0 = (int)std::lround(cy);
            ff.x1 = (int)std::lround(cx + x1); ff.y1 = (int)std::lround(cy);
            ff.x2 = (int)std::lround(cx); ff.y2 = (int)std::lround(half == 0 ? cy - halfH : cy + halfH);
            ff.col = RGBA((int)std::min(255.0f, r), (int)std::min(255.0f, g), (int)std::min(255.0f, b));
            (void)z0; (void)z1;
        }
    }
    // back to front
    for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++) if (f[j].depth < f[i].depth) std::swap(f[i], f[j]);
    for (int i = 0; i < n; i++) c.FillTriangle(f[i].x0, f[i].y0, f[i].x1, f[i].y1, f[i].x2, f[i].y2, f[i].col);
    // a thin bright rim on the equator and the silhouette, as the original's lit edges
    int top = (int)std::lround(cy - halfH), bot = (int)std::lround(cy + halfH);
    float ex = 0;
    for (int k = 0; k < 4; k++) ex = std::max(ex, std::fabs(std::cos(angle + k * PI * 0.5f)) * halfW);
    int l = (int)std::lround(cx - ex), r = (int)std::lround(cx + ex), m = (int)std::lround(cx), y = (int)std::lround(cy);
    c.Line(l, y, m, top, RGBA(20, 90, 20, 200)); c.Line(m, top, r, y, RGBA(20, 90, 20, 200));
    c.Line(l, y, m, bot, RGBA(10, 60, 10, 200)); c.Line(m, bot, r, y, RGBA(10, 60, 10, 200));
}

static UIModePanel* PanelOf(LotScreen* l) {
    switch (l->CurrentMode) {
        case LotScreen::Mode::Buy: return (UIModePanel*)l->Buy.get();
        case LotScreen::Mode::Build: return (UIModePanel*)l->Build.get();
        case LotScreen::Mode::Options: return (UIModePanel*)l->Options.get();
        default: return (UIModePanel*)l->Live.get();
    }
}

void LotScreen::SetMode(Mode m) {
    if (m == CurrentMode) return;
    PanelOf(this)->Deactivate();
    Pie.Close(); PieTarget = nullptr;
    TopTip.clear(); // FIX(sims) QA2: a buy-mode placement error ("Must place on a table or surface") stayed over live/options mode
    bool wasBuildBuy = CurrentMode == Mode::Buy || CurrentMode == Mode::Build;
    CurrentMode = m;
    bool isBuildBuy = m == Mode::Buy || m == Mode::Build;
    // TS1(sims): leaving buy/build ends the "changed my mind" full refund window
    if (wasBuildBuy && !isBuildBuy) Holder->BoughtThisSession.clear();
    PanelOf(this)->Activate();
    if (Log) *Log += std::string("mode ") + (m == Mode::Live ? "live" : m == Mode::Buy ? "buy" : m == Mode::Build ? "build" : "options") + "\n";
}

int LotScreen::EffectiveSpeed() const {
    for (auto& d : S->Dialogs) if (d.Block) return 0; // TS1 pauses while a dialog waits for an answer
    if (CurrentMode != Mode::Live) return 0;            // TS1: time stands still in buy/build/options
    return Speed;
}

std::string LotScreen::HitAt(int x, int y) const {
    for (size_t i = Hits.size(); i-- > 0;) if (Hits[i].R.Contains(x, y)) return Hits[i].Id; // topmost = last drawn
    return "";
}

// ======================================================================= shared widgets

void LotScreen::DrawPanelFrame(Canvas& c, const Rect& r) {
    c.GradientV(r.X, r.Y, r.W, r.H, RGBA(16, 24, 92), RGBA(6, 10, 60));
    c.FillRect(r.X, r.Y, r.W, 1, RGBA(120, 130, 210));
    c.FillRect(r.X, r.Bottom() - 1, r.W, 1, RGBA(0, 0, 30));
}

void LotScreen::DrawArtButton(Canvas& c, const Rect& r, const std::string& art, int frames, bool selected, bool disabled,
                              const std::string& fallbackLabel, const std::string& hitId) {
    ButtonState st = disabled ? ButtonState::Disabled : (selected ? ButtonState::Highlight : ButtonState::Normal);
    bool drawn = !art.empty() && UIGraphics::Get().DrawButton(c, art, st, r, frames);
    if (!drawn) {
        c.FillRoundRect(r, 5, disabled ? RGBA(40, 50, 80) : (selected ? theme::ButtonHi : theme::ButtonBg));
        c.StrokeRoundRect(r.X, r.Y, r.W, r.H, 5, selected ? RGBA(0, 255, 255) : theme::PanelEdge);
        if (!fallbackLabel.empty())
            DrawTextCentered(c, FontId::Small, r, Ellipsize(FontId::Small, fallbackLabel, r.W - 2), disabled ? theme::TextDim : (selected ? RGBA(0, 255, 255) : theme::TextBright));
    }
    if (!hitId.empty() && !disabled) Hit(r, hitId);
}

void LotScreen::DrawTouchView(Canvas& c, const Rect& area) {
    auto& f = S->LastFrame();
    float sc = std::min((float)area.W / TOP_W, (float)area.H / TOP_H);
    int w = (int)(TOP_W * sc), h = (int)(TOP_H * sc);
    Rect r(area.X + (area.W - w) / 2, area.Y + (area.H - h) / 2, w, h);
    if (r.W != ViewRect.W || r.H != ViewRect.H) ViewFrameNo = 0;
    ViewRect = r; ViewScale = sc;
    if (f.Width > 0) {
        if (ViewThumb.Width != w || ViewThumb.Height != h) { ViewThumb.Resize(w, h); ViewFrameNo = 0; }
        if (ViewFrameNo++ % (uint32_t)std::max(1, ThumbInterval) == 0)
            ViewThumb.BlitScaled(f.Color.data(), f.Width, f.Height, Rect(0, 0, w, h));
        c.Blit(ViewThumb.Px.data(), w, h, r.X, r.Y, false);
    }
    c.StrokeRect(r.X - 1, r.Y - 1, r.W + 2, r.H + 2, RGBA(90, 100, 190));
    // the selected Sim's plumbob, the cursor
    if (auto sim = S->SelectedSim()) {
        float x, y;
        if (CurrentMode == Mode::Live && sim->Position().Level <= S->World()->State.Level() && S->EntityScreenPos(sim, x, y, 2.05f)) {
            int px, py; TopToView(x, y, px, py); py -= 4;
            if (r.Contains(px, py)) { c.FillTriangle(px - 3, py, px + 3, py, px, py - 5, theme::Plumbob); c.FillTriangle(px - 3, py, px + 3, py, px, py + 5, theme::PlumbobDark); }
        }
    }
    if (CursorShown) {
        int px, py; TopToView((float)CursorX, (float)CursorY, px, py);
        c.Line(px - 3, py, px + 3, py, RGBA(255, 255, 255, 200));
        c.Line(px, py - 3, px, py + 3, RGBA(255, 255, 255, 200));
    }
    Hit(r, "view");
}

bool LotScreen::ViewToTop(int bx, int by, int& tx, int& ty) const {
    if (ViewScale <= 0) return false;
    tx = (int)std::lround((bx - ViewRect.X) / ViewScale);
    ty = (int)std::lround((by - ViewRect.Y) / ViewScale);
    tx = std::max(0, std::min(TOP_W - 1, tx));
    ty = std::max(0, std::min(TOP_H - 1, ty));
    return true;
}

void LotScreen::TopToView(float tx, float ty, int& bx, int& by) const {
    bx = ViewRect.X + (int)std::lround(tx * ViewScale);
    by = ViewRect.Y + (int)std::lround(ty * ViewScale);
}

void LotScreen::ShowTopTip(const std::string& s, int x, int y, uint32_t color, double ms) {
    TopTip = s; TopTipX = x; TopTipY = y; TopTipColor = color; TopTipUntil = Time + ms;
}

// ======================================================================= icons

static bool DecodeBmpChunk(FSO::Files::Formats::IFF::Chunks::BMP* bmp, Canvas& out, bool keyCorner) {
    if (bmp == nullptr || !UIGraphics::DecodeBMP(bmp->GetData(), out) || out.Px.empty()) return false;
    if (keyCorner && (out.Px[0] >> 24)) {
        uint32_t key = out.Px[0] & 0x00FFFFFFu;
        for (auto& p : out.Px) if ((p & 0x00FFFFFFu) == key) p = 0;
    }
    if (keyCorner) {
        // FIX(sims) FB1: catalogue BMP_ = button frames side by side (FreeSO UICatalogItem): keep the normal frame
        int frames = (out.Height > 0 && out.Width / out.Height > 2) ? 4 : (out.Height > 48 || out.Width == out.Height) ? 1 : 2;
        if (frames > 1) {
            int fw = out.Width / frames;
            Canvas f(fw, out.Height);
            for (int y = 0; y < out.Height; y++) for (int x = 0; x < fw; x++) f.Px[(size_t)y * fw + x] = out.Px[(size_t)y * out.Width + x];
            out = std::move(f);
        }
    }
    return true;
}

const Canvas* LotScreen::IconFor(VMEntity* e) {
    if (auto av = dynamic_cast<VMAvatar*>(e)) return FaceFor(av, 0);
    auto go = dynamic_cast<VMGameObject*>(e);
    if (go == nullptr || go->Object == nullptr || go->Object->OBJ == nullptr || go->Object->Resource == nullptr) return nullptr;
    // multitile parts: the master's catalogue icon
    uint64_t key = go->Object->GUID;
    auto it = _Icons.find(key);
    if (it != _Icons.end()) return it->second.get();
    std::unique_ptr<Canvas> c(new Canvas());
    auto bmp = go->Object->Resource->Get<FSO::Files::Formats::IFF::Chunks::BMP>(go->Object->OBJ->CatalogStringsID);
    if (bmp == nullptr && go->MasterDefinition != nullptr)
        bmp = go->Object->Resource->Get<FSO::Files::Formats::IFF::Chunks::BMP>(go->MasterDefinition->CatalogStringsID);
    if (!DecodeBmpChunk(bmp, *c, true)) c.reset();
    auto res = c.get();
    _Icons[key] = std::move(c);
    return res;
}

const Canvas* LotScreen::IconForGUID(uint32_t guid) {
    uint64_t key = guid;
    auto it = _Icons.find(key);
    if (it != _Icons.end()) return it->second.get();
    std::unique_ptr<Canvas> c;
    auto content = FSO::Content::Content::Get();
    auto obj = content ? content->WorldObjects->Get(guid, true) : nullptr;
    if (obj != nullptr && obj->OBJ != nullptr && obj->Resource != nullptr) {
        c.reset(new Canvas());
        if (!DecodeBmpChunk(obj->Resource->Get<FSO::Files::Formats::IFF::Chunks::BMP>(obj->OBJ->CatalogStringsID), *c, true)) c.reset();
    }
    auto res = c.get();
    _Icons[key] = std::move(c);
    return res;
}

std::string LotScreen::NameForGUID(uint32_t guid) {
    auto content = FSO::Content::Content::Get();
    auto obj = content ? content->WorldObjects->Get(guid, true) : nullptr;
    if (obj == nullptr || obj->OBJ == nullptr) return std::string();
    if (obj->Resource != nullptr) {
        if (auto ctss = obj->Resource->Get<FSO::Files::Formats::IFF::Chunks::CTSS>(obj->OBJ->CatalogStringsID)) {
            auto n = ctss->GetString(0);
            if (!n.empty()) return ToLatin1(n);
        }
    }
    return ToLatin1(obj->OBJ->ChunkLabel);
}

const Canvas* LotScreen::FaceFor(VMAvatar* a, int frame) {
    if (a == nullptr || a->Object == nullptr || a->Object->Resource == nullptr) return nullptr;
    // TS1(sims): the character file's BMP_ 2002 "faces" = 3 portraits (normal, highlighted, selected)
    uint64_t key = (a->Object->GUID << 2) | (uint64_t)(frame & 3);
    auto it = _Faces.find(key);
    if (it != _Faces.end()) return it->second.get();
    std::unique_ptr<Canvas> out;
    Canvas all;
    if (DecodeBmpChunk(a->Object->Resource->Get<FSO::Files::Formats::IFF::Chunks::BMP>(2002), all, false) && all.Width >= 3) {
        int fw = all.Width / 3;
        out.reset(new Canvas(fw, all.Height));
        for (int y = 0; y < all.Height; y++)
            for (int x = 0; x < fw; x++) out->Px[(size_t)y * fw + x] = all.Px[(size_t)y * all.Width + frame * fw + x];
    } else if (DecodeBmpChunk(a->Object->Resource->Get<FSO::Files::Formats::IFF::Chunks::BMP>(2004), all, false)) {
        out.reset(new Canvas(all)); // 25x25 thumbnail
    } else if (auto str = a->Object->Resource->Get<FSO::Files::Formats::IFF::Chunks::STR>(200)) {
        // TS1(sims): a character without portraits (imported .FAM members, babies, grown-up children): draw the
        // face from its body strings like Create-A-Sim does (CASSimPreview::RenderPortraits)
        auto bs = [&](int i) { return i < str->Length() ? str->GetString(i) : std::string(); };
        auto split = [](const std::string& v, const char* key, FSO::Content::TS1::CAS::CASOutfit& o) {
            auto c = v.find(','); auto k = v.find(key);
            if (c == std::string::npos || k == std::string::npos) return;
            o.Mesh = Lower(v.substr(0, c)); o.Texture = Lower(v.substr(k + strlen(key)));
        };
        using namespace FSO::Content::TS1::CAS;
        CASSimDesc d;
        d.Age = bs(0) == "child" ? CASAge::Child : CASAge::Adult;
        d.Gender = bs(12) == "male" ? CASGender::Male : CASGender::Female;
        auto skin = bs(14);
        d.Skin = skin == "drk" ? CASSkin::Dark : (skin == "med" ? CASSkin::Medium : CASSkin::Light);
        split(bs(1), "BODY=", d.Body);
        split(bs(2), "HEAD-HEAD=", d.Head);
        if (!d.Head.Empty() && !d.Body.Empty() && bs(0) != "dog" && bs(0) != "cat" && bs(0) != "kat") {
            simapp::cas::CASSimPreview preview;
            preview.Set(d);
            if (preview.Valid()) {
                simapp::cas::CASPortraits p;
                preview.RenderPortraits(p);
                if (p.Faces.Width >= 3) {
                    int fw = p.Faces.Width / 3;
                    out.reset(new Canvas(fw, p.Faces.Height));
                    for (int y = 0; y < p.Faces.Height; y++)
                        for (int x = 0; x < fw; x++) out->Px[(size_t)y * fw + x] = p.Faces.Px[(size_t)y * p.Faces.Width + frame * fw + x];
                }
            }
        }
    }
    auto res = out.get();
    _Faces[key] = std::move(out);
    return res;
}

// ======================================================================= money

void LotScreen::MoneyPopup(int amount, VMEntity* at) {
    if (amount == 0) return;
    if (at == nullptr) at = S->SelectedSim();
    Popup p;
    p.Amount = amount; p.Start = Time; p.X = -1; p.Y = -1;
    if (at != nullptr && at->WorldUI != nullptr) {
        auto pos = at->WorldUI->DrawPosition();
        p.X = pos.X; p.Y = pos.Y;
        if (dynamic_cast<VMGameObject*>(at)) { p.X += 0.5f; p.Y += 0.5f; }
    }
    if (_Popups.size() < 6) _Popups.push_back(p);
    _LastBudget = S->Budget();
}

// FIX(sims) QA2: wall/floor/terrain costs used the selected Sim's position: off screen they piled up, cut off at the
// screen edge ("-$486" x4 at the right border). They now rise from where the stylus let go.
void LotScreen::MoneyPopupAt(int amount, int topX, int topY) {
    if (amount == 0) return;
    Popup p;
    p.Amount = amount; p.Start = Time; p.X = -1000; p.Y = -1000; p.SX = topX; p.SY = topY;
    if (_Popups.size() < 6) _Popups.push_back(p);
    _LastBudget = S->Budget();
}

// FreeSO VMAvatar.ShowMoneyHeadline (person data MoneyAmmountOverHead / scope MoneyOverHead32Bit): the behaviour asks
// for the amount over the Sim's head. A funds change of the same amount shown just before (TrackBudget) moves there
// instead of appearing twice.
void LotScreen::PollMoneyHeadlines() {
    for (auto e : S->VM()->Entities) {
        auto av = dynamic_cast<VMAvatar*>(e);
        if (av == nullptr) continue;
        auto it = _MoneyHeadlineSerial.find(av->ObjectID);
        if (it == _MoneyHeadlineSerial.end()) { _MoneyHeadlineSerial[av->ObjectID] = av->MoneyHeadlineSerial; continue; }
        if (it->second == av->MoneyHeadlineSerial) continue;
        it->second = av->MoneyHeadlineSerial;
        int amount = av->MoneyHeadlineValue;
        if (amount == 0 || av->WorldUI == nullptr) continue;
        auto pos = av->WorldUI->DrawPosition();
        float lift = (av->GetPersonData(FSO::SimAntics::Model::VMPersonDataVariable::PersonsAge) < 18) ? 1.7f : 2.3f;
        bool merged = false;
        for (auto& p : _Popups)
            if (!p.Head && p.Amount == amount && Time - p.Start < 1500) { p.X = pos.X; p.Y = pos.Y; p.Z = pos.Z; p.Lift = lift; p.Head = true; merged = true; break; }
        if (merged || _Popups.size() >= 6) continue;
        Popup p;
        p.Amount = amount; p.Start = Time; p.X = pos.X; p.Y = pos.Y; p.Z = pos.Z; p.Lift = lift; p.Head = true;
        _Popups.push_back(p);
    }
}

void LotScreen::TrackBudget() {
    PollMoneyHeadlines();
    int b = S->Budget();
    if (_LastBudget < 0) { _LastBudget = b; return; }
    if (b != _LastBudget) {
        // already over a Sim's head (money headline of the same amount just now): no second popup
        for (auto& p : _Popups) if (p.Head && p.Amount == b - _LastBudget && Time - p.Start < 1500) { _LastBudget = b; return; }
        // TS1: job pay, bills, sold paintings... appear as "+§n" over the Sim that earned/spent it
        int delta = b - _LastBudget;
        VMEntity* at = nullptr;
        // the family member whose current interaction is the likely source (carpool, mailbox, easel...)
        for (auto sim : S->Family) {
            if (sim == nullptr || sim->Thread == nullptr || sim->Thread->Queue.empty()) continue;
            auto callee = sim->Thread->Queue[0]->Callee;
            if (callee != nullptr && callee != sim && callee->Position().TileX() >= 0) { at = callee; break; }
        }
        MoneyPopup(delta, at);
        if (Log) { char buf[64]; snprintf(buf, sizeof buf, "funds %+d -> %d\n", delta, b); *Log += buf; }
    }
}

// ======================================================================= input

void LotScreen::OpenPieAt(int tx, int ty) {
    std::vector<PieChoice> pie;
    bool isGoto = false;
    int16_t picked = 0;
    double t0 = NowMs ? NowMs() : 0;
    auto target = S->PieTargetAt(tx, ty, pie, isGoto, picked);
    PickMsLast = NowMs ? NowMs() - t0 : 0;
    if (target == nullptr) { if (Log) *Log += "tap: nothing\n"; return; }
    std::string title = isGoto ? std::string("") : ToLatin1(target->ToString());
    // FIX(sims) REL: object names that end in a line break (D.I.Y. Bookshelf) showed "??" after the pie title
    while (!title.empty() && (unsigned char)title.back() <= 32) title.pop_back();
    // a family member: offer selecting it first (TS1 selects Sims from their portraits; here also from the lot)
    int fam = S->IndexOfFamily(target);
    if (fam >= 0 && fam != S->Selected) {
        PieChoice sel;
        sel.Name = "Select " + FirstName(ToLatin1(target->ToString()));
        sel.ID = 255; sel.Param0 = SELECT_SIM_PARAM;
        pie.insert(pie.begin(), sel);
    }
    if (Log) {
        char buf[256];
        snprintf(buf, sizeof buf, "tap view -> top (%d,%d): picked #%d -> %s'%s' (%zu interactions, pick %.2f ms)\n",
                 tx, ty, picked, isGoto ? "goto " : "", title.c_str(), pie.size(), PickMsLast);
        *Log += buf;
    }
    if (pie.empty()) {
        if (!isGoto && picked > 0) ShowTopTip(GameText::Get().UI(133, 3, "There is nothing to select here."), tx, ty - 14, RGBA(255, 255, 255));
        return;
    }
    PieTarget = target;
    PieGoto = isGoto;
    PieMarkX = (float)tx; PieMarkY = (float)ty;
    Pie.Open(pie, title, Rect(0, 0, BOT_W, BOT_H));
}

// FIX(sims) SND: FreeSims UISounds through the HIT VM (UI events keep playing while the lot's sounds are paused)
void PlayUISound(const char* evt) {
    auto& ga = simaudio::GameAudio::Get();
    if (ga.Ready()) ga.PlayUI(evt);
}

// FIX(sims) SND: FreeSims UIButton plays UISounds.Click after every button click (mode, catalogue category, sub-sort,
// page arrows, build tools, options, Sim portraits, tabs...). Not buttons there, so not here: catalogue items
// (UICatalogItem: silent, Error when unaffordable), the interaction queue (QueueDelete), the money / clock readouts;
// the hold toolbar has its own sounds (place / rotate / money back, UIObjectHolder) and the speed buttons play
// TS1's ui_speed_* transitions (App::AudioFrame).
static bool TapClicks(const std::string& id) {
    auto starts = [&](const char* p) { return id.rfind(p, 0) == 0; };
    if (id == "funds" || id == "clock" || id == "panel" || id == "view") return false;
    if (starts("speed") || starts("item") || starts("bitem") || starts("queue")) return false;
    if (id == "hold_place" || id == "hold_sell" || id == "hold_rotl" || id == "hold_rotr") return false;
    return true;
}

void LotScreen::HandleTap(const std::string& id) {
    if (TapClicks(id)) PlayUISound(UISounds::Click); // FIX(sims) SND
    if (id.rfind("mode_", 0) == 0) {
        if (id == "mode_live") SetMode(Mode::Live);
        else if (id == "mode_buy") SetMode(Mode::Buy);
        else if (id == "mode_build") SetMode(Mode::Build);
        else if (id == "mode_options") SetMode(CurrentMode == Mode::Options ? Mode::Live : Mode::Options);
        return;
    }
    if (id.rfind("speed", 0) == 0) {
        int sp = id[5] - '0';
        Speed = std::max(0, std::min(3, sp));
        if (Speed) LastSpeed = Speed;
        if (CurrentMode != Mode::Live) SetMode(Mode::Live);
        return;
    }
    if (id == "funds" || id == "clock") return;
    PanelOf(this)->Tap(id);
}

void LotScreen::HandleButtons(const AppInput& in) {
    auto panel = PanelOf(this);
    bool consumed = panel->Buttons(in);
    if (in.Down & BTN_START) SetMode(CurrentMode == Mode::Options ? Mode::Live : Mode::Options);
    if (in.Down & BTN_L) S->Rotate(-1);
    if (in.Down & BTN_R) S->Rotate(+1);
    if (in.Down & BTN_UP) S->Zoom(+1);
    if (in.Down & BTN_DOWN) S->Zoom(-1);
    if (in.Down & BTN_ZL) S->ChangeLevel(-1);
    if (in.Down & BTN_ZR) S->ChangeLevel(+1);
    if (in.Down & BTN_SELECT) S->SetWallsMode(S->WallsMode + 1);
    if ((in.Down & BTN_X) && CurrentMode == Mode::Live) {
        if (Speed == 0) Speed = std::max(1, LastSpeed); else { LastSpeed = Speed; Speed = 0; }
    }
    if ((in.Down & BTN_B) && !consumed && CurrentMode == Mode::Options) SetMode(Mode::Live);
}

void LotScreen::Input(const AppInput& in, double dtMs, double timeMs) {
    // ADDED(sims): held objects are moved to room 65534 (UIObjectHolder.SetSelected); FreeSims' lighting
    // texture lit that entry, our RoomColors leaves it transparent black - light it like the outside.
    if (S->Blueprint != nullptr && S->Blueprint->RoomColors.size() > 65534) S->Blueprint->RoomColors[65534] = st::Color::White();
    Time = timeMs;
    if (PointerMovedAt < 0) PointerMovedAt = Time; // ADDED(sims): the idle time starts with the lot
    auto& s = *S;
    P.Down = P.Up = P.Click = false;
    // ADDED(sims): pointer visibility -- the stylus hides the top-screen pointer; it only counts down its idle time
    // in live mode with nothing modal open. The cut follows the active input (set below); default: keep the last cut.
    if (in.TouchDown || in.Touching) HidePointer();
    if (CurrentMode != Mode::Live || Pie.IsOpen || !s.Dialogs.empty()) PointerMovedAt = Time;
    CutFollow = false;

    // ---------------- modal: dialogs
    if (!s.Dialogs.empty()) {
        if (in.TouchDown) {
            for (auto& b : _DialogButtons) if (b.first.Contains(in.TouchX, in.TouchY)) { PlayUISound(UISounds::Click); s.RespondDialog(b.second); break; } // FIX(sims) SND: UIAlert's UIButtons
        } else if (in.Down & BTN_A) s.RespondDialog(0);
        else if (in.Down & BTN_B) s.RespondDialog(s.Dialogs.front().HasCancel ? 2 : (s.Dialogs.front().HasNo ? 1 : 0));
        _ViewTouch = false;
        return;
    }
    // ---------------- modal: pie menu
    if (Pie.IsOpen) {
        PieChoice choice;
        PieMenu::Result r = PieMenu::Result::None;
        if (in.TouchDown) r = Pie.Tap(in.TouchX, in.TouchY, choice);
        else {
            int dx = ((in.Down & BTN_RIGHT) ? 1 : 0) - ((in.Down & BTN_LEFT) ? 1 : 0);
            int dy = ((in.Down & BTN_DOWN) ? 1 : 0) - ((in.Down & BTN_UP) ? 1 : 0);
            r = Pie.Button(dx, dy, (in.Down & BTN_A) != 0, (in.Down & BTN_B) != 0, choice);
        }
        if (r == PieMenu::Result::Chosen) {
            simaudio::GameAudio::Get().PlayUI("ui_piemenu_select");
            auto vm = s.VM();
            bool alive = PieTarget != nullptr && (PieTarget == s.GotoObject || vm->GetObjectById(PieTarget->ObjectID) == PieTarget);
            if (choice.ID == 255 && choice.Param0 == SELECT_SIM_PARAM) {
                int fam = alive ? s.IndexOfFamily(PieTarget) : -1;
                if (fam >= 0) s.Select(fam);
                if (Log) *Log += "selected " + (alive ? PieTarget->ToString() : std::string("?")) + "\n";
            } else if (alive) {
                bool ok = s.PushInteraction(PieTarget, PieGoto, choice);
                simaudio::GameAudio::Get().PlayUI(ok ? "ui_queued" : "ui_error"); // FreeSims UIPieMenu: QueueAdd
                if (Log) *Log += std::string(ok ? "queued '" : "could not queue '") + choice.Name + "' on " + (PieGoto ? std::string("tile") : PieTarget->ToString()) + "\n";
            }
            PieTarget = nullptr;
        } else if (r == PieMenu::Result::Closed) PieTarget = nullptr;
        if (in.Down & BTN_START) { Pie.Close(); PieTarget = nullptr; }
        _ViewTouch = false;
        _ABtnHeld = (in.Held & BTN_A) != 0;
        return;
    }

    HandleButtons(in);
    if (LeaveRequested || QuitRequested) return;
    auto panel = PanelOf(this);

    // ---------------- cursor (circle pad) + scrolling (C-stick, edge scroll)
    float cx = in.CircleX, cy = in.CircleY;
    if (std::fabs(cx) < 0.15f) cx = 0;
    if (std::fabs(cy) < 0.15f) cy = 0;
    float sx = 0, sy = 0;
    if (cx != 0 || cy != 0) {
        ShowPointer(); // ADDED(sims)
        float px = 220.0f * (float)dtMs / 1000.0f;
        float nx = CursorX + cx * px, ny = CursorY - cy * px;
        // TS1 edge scrolling: pushing the cursor against the screen edge scrolls the lot
        const int EDGE = 6;
        if (nx < EDGE) { sx += nx - EDGE; nx = EDGE; }
        if (nx > TOP_W - 1 - EDGE) { sx += nx - (TOP_W - 1 - EDGE); nx = (float)(TOP_W - 1 - EDGE); }
        if (ny < EDGE) { sy += ny - EDGE; ny = EDGE; }
        if (ny > TOP_H - 1 - EDGE) { sy += ny - (TOP_H - 1 - EDGE); ny = (float)(TOP_H - 1 - EDGE); }
        CursorX = (int)std::lround(nx); CursorY = (int)std::lround(ny);
    }
    float kx = in.CStickX, ky = in.CStickY;
    if (std::fabs(kx) < 0.15f) kx = 0;
    if (std::fabs(ky) < 0.15f) ky = 0;
    if (kx != 0 || ky != 0) ShowPointer(); // ADDED(sims): the pad is in use again
    if (in.Down & BTN_A) ShowPointer();    // ADDED(sims): A clicks at the pointer: show where
    if (kx != 0 || ky != 0) { float px = 260.0f * (float)dtMs / 1000.0f; sx += kx * px; sy -= ky * px; }
    if (sx != 0 || sy != 0) s.ScrollScreen(sx * 1.6f, sy * 1.6f);

    // ---------------- touch
    if (in.TouchDown) {
        std::string id = HitAt(in.TouchX, in.TouchY);
        if (id == "view") {
            _ViewTouch = true; _ViewDragging = false;
            _TouchStartX = _TouchLastX = in.TouchX; _TouchStartY = _TouchLastY = in.TouchY;
        } else if (!id.empty()) {
            if (Log) *Log += "tap " + id + "\n";
            HandleTap(id);
            panel = PanelOf(this);
        }
    }
    bool dragScrolls = panel->ViewDragScrolls();
    if (_ViewTouch) {
        int tx, ty;
        int bx = std::max(ViewRect.X, std::min(ViewRect.Right() - 1, in.TouchX)), by = std::max(ViewRect.Y, std::min(ViewRect.Bottom() - 1, in.TouchY));
        ViewToTop(bx, by, tx, ty);
        if (in.Touching || in.TouchUp) { CutFollow = true; CutX = tx; CutY = ty; } // ADDED(sims): cut under the stylus
        if (dragScrolls) {
            if (in.Touching) {
                int dx = in.TouchX - _TouchLastX, dy = in.TouchY - _TouchLastY;
                if (std::abs(in.TouchX - _TouchStartX) + std::abs(in.TouchY - _TouchStartY) > 6) _ViewDragging = true;
                if (_ViewDragging && (dx || dy)) s.ScrollScreen(-dx / ViewScale, -dy / ViewScale);
                _TouchLastX = in.TouchX; _TouchLastY = in.TouchY;
            }
            if (in.TouchUp) {
                _ViewTouch = false;
                if (!_ViewDragging) {
                    CursorX = tx; CursorY = ty; CursorShown = false;
                    P.Active = true; P.FromTouch = true; P.X = tx; P.Y = ty; P.Click = true; P.Down = P.Up = true; P.Held = false;
                }
            }
        } else {
            P.Active = true; P.FromTouch = true; P.X = tx; P.Y = ty;
            P.Down = in.TouchDown; P.Held = in.Touching; P.Up = in.TouchUp; P.Click = in.TouchUp;
            CursorX = tx; CursorY = ty; CursorShown = false;
            if (in.TouchUp) _ViewTouch = false;
        }
    } else if (!P.Click) {
        // the cursor: A is the mouse button
        bool aHeld = (in.Held & BTN_A) != 0 || (in.Down & BTN_A) != 0;
        P.Active = true; P.FromTouch = false; P.X = CursorX; P.Y = CursorY;
        P.Down = (in.Down & BTN_A) != 0;
        P.Held = aHeld;
        P.Up = _ABtnHeld && !aHeld;
        P.Click = P.Down;
        _ABtnHeld = aHeld;
    }
    // ADDED(sims): live mode: the pointer fades out after POINTER_HIDE_MS without pad movement
    if (CursorShown && CurrentMode == Mode::Live && Time - PointerMovedAt >= POINTER_HIDE_MS + POINTER_FADE_MS) HidePointer();
    if (CursorShown) { CutFollow = true; CutX = CursorX; CutY = CursorY; }
    panel->PointerUpdate();
}

// ADDED(sims)
void LotScreen::ShowPointer() {
    CursorShown = true;
    PointerMovedAt = Time;
}

// ADDED(sims)
int LotScreen::PointerAlpha() const {
    if (!CursorShown) return 0;
    if (CurrentMode != Mode::Live) return 255;
    double idle = Time - PointerMovedAt - POINTER_HIDE_MS;
    if (idle <= 0) return 255;
    return std::max(0, std::min(255, (int)(255.0 * (1.0 - idle / POINTER_FADE_MS))));
}

// ======================================================================= drawing

void LotScreen::Draw() {
    TrackBudget();
    DrawTop();
    DrawBottom();
}

void LotScreen::DrawQueueTop(VMAvatar* sim) {
    // TS1: the selected Sim's action queue, icons in a row at the top left, the active one first
    if (sim == nullptr || sim->Thread == nullptr) return;
    int x = 4, shown = 0;
    for (size_t i = 0; i < sim->Thread->Queue.size() && shown < 8; i++) {
        auto& a = sim->Thread->Queue[i];
        if (a->Mode == Engine::VMQueueMode::ParentExit || a->Mode == Engine::VMQueueMode::Idle) continue;
        int sz = shown == 0 ? 34 : 26;
        Rect r(x, 4, sz, sz);
        Top->FillRoundRect(r, 4, RGBA(10, 16, 70, 210));
        Top->StrokeRoundRect(r.X, r.Y, r.W, r.H, 4, shown == 0 ? RGBA(0, 255, 255) : RGBA(110, 120, 200));
        auto icon = IconFor(a->IconOwner());
        if (icon) Top->BlitScaled(icon->Px.data(), icon->Width, icon->Height, Rect(r.X + 2, r.Y + 2, sz - 4, sz - 4), true);
        else Top->FillCircle(r.X + sz / 2, r.Y + sz / 2, sz / 3, theme::Plumbob);
        if (Cancelled.count(a->UID)) UIGraphics::Get().DrawImage(*Top, "Other/QueueCancel.bmp", Rect(r.X + 2, r.Y + 2, sz - 4, sz - 4));
        if (shown == 0) {
            std::string name = ToLatin1(a->Name);
            DrawTextShadow(*Top, FontId::Bold, 4, 41, Ellipsize(FontId::Bold, name, 190), RGBA(255, 255, 255), RGBA(0, 0, 0, 200));
        }
        x += sz + 3;
        shown++;
    }
}

void LotScreen::DrawHeadlines() {
    auto world = S->World();
    int zoom = (int)world->State.Zoom();
    int level = world->State.Level();
    for (auto e : S->VM()->Entities) {
        if (e == nullptr || e->Headline == nullptr || e->HeadlineRenderer == nullptr) continue;
        if (e->Position().Level > level || e->Position().TileX() < 0) continue;
        auto r = dynamic_cast<UIHeadlineRenderer*>(e->HeadlineRenderer.get());
        if (r == nullptr) continue;
        auto av = dynamic_cast<VMAvatar*>(e);
        // C# AvatarComponent: head bone + 0.66 tiles; ObjectComponent: position + 0.66
        float lift = av ? ((av->GetPersonData(FSO::SimAntics::Model::VMPersonDataVariable::PersonsAge) < 18) ? 1.5f : 2.05f) : 0.66f;
        if (av && av->IsPet()) lift = 1.3f;
        float x, y;
        if (!S->EntityScreenPos(e, x, y, lift)) continue;
        int w, h;
        r->Size(zoom, w, h);
        if (x < -w || y < -h || x > TOP_W + w || y > TOP_H + h) continue;
        // Sims: the balloon sits above the head and the plumbob; keep it on screen (TS1 balloons never leave the view)
        float zs = zoom == 3 ? 2.0f : (zoom == 2 ? 1.0f : 0.5f);
        int by = av ? (int)y - h / 2 - (int)(14 * zs) : (int)y;
        by = std::max(h / 2 + 2, by);
        r->Draw(*Top, (int)x, by, zoom);
    }
}

void LotScreen::DrawPopups() {
    auto world = S->World();
    for (size_t i = 0; i < _Popups.size();) {
        auto& p = _Popups[i];
        double t = (Time - p.Start) / 2500.0;
        if (t >= 1.0) { _Popups.erase(_Popups.begin() + (long)i); continue; }
        float x = TOP_W / 2.0f, y = TOP_H / 2.0f;
        if (p.X > -100) {
            auto sp = TileToScreen(*world, st::Vector3(p.X, p.Y, (p.Head ? p.Z : (world->State.Level() - 1) * 2.95f / 3.0f) + p.Lift));
            x = sp.X; y = sp.Y;
        } else if (p.SX >= 0) { x = (float)p.SX; y = (float)p.SY; }
        y -= (float)(t * 36.0);
        char buf[48];
        int a = std::abs(p.Amount);
        std::string digits = std::to_string(a), grouped;
        int c = 0;
        for (int k = (int)digits.size() - 1; k >= 0; k--) { grouped.insert(grouped.begin(), digits[(size_t)k]); if (++c % 3 == 0 && k > 0) grouped.insert(grouped.begin(), ','); }
        snprintf(buf, sizeof buf, "%s\xA7%s", p.Amount > 0 ? "+" : "-", grouped.c_str());
        int alpha = t < 0.7 ? 255 : (int)(255 * (1.0 - t) / 0.3);
        uint32_t col = p.Amount > 0 ? RGBA(90, 255, 90, alpha) : RGBA(255, 80, 70, alpha);
        int w = TextWidth(FontId::Title, buf);
        int tx = std::max(2, std::min(TOP_W - w - 2, (int)x - w / 2)); // FIX(sims) QA2: never cut off at the screen edge
        DrawTextShadow(*Top, FontId::Title, tx, (int)y, buf, col, RGBA(0, 0, 0, alpha * 3 / 4));
        i++;
    }
}

void LotScreen::DrawCursor() {
    int alpha = PointerAlpha(); // ADDED(sims): hidden after a touch / faded out after the idle time
    if (alpha <= 0) return;
    int hx = 0, hy = 0;
    auto cur = UIGraphics::Get().Cursor(PanelOf(this)->CursorArt(), hx, hy);
    if (cur == nullptr) cur = UIGraphics::Get().Cursor("Shared/cursors/hand.cur", hx, hy);
    if (cur != nullptr && alpha < 255) {
        for (int y = 0; y < cur->Height; y++)
            for (int x = 0; x < cur->Width; x++) Top->BlendA(CursorX - hx + x, CursorY - hy + y, cur->Px[(size_t)y * cur->Width + x], alpha);
    } else if (cur != nullptr) Top->Blit(cur->Px.data(), cur->Width, cur->Height, CursorX - hx, CursorY - hy, true);
    else {
        Top->FillTriangle(CursorX, CursorY, CursorX + 10, CursorY + 6, CursorX + 4, CursorY + 11, RGBA(255, 255, 255));
        Top->Line(CursorX, CursorY, CursorX + 10, CursorY + 6, RGBA(0, 0, 0));
        Top->Line(CursorX, CursorY, CursorX + 4, CursorY + 11, RGBA(0, 0, 0));
    }
}

void LotScreen::DrawTop() {
    auto& s = *S;
    auto& f = s.LastFrame();
    if (f.Width == TOP_W && f.Height == TOP_H) std::copy(f.Color.begin(), f.Color.end(), Top->Px.begin());
    auto sim = s.SelectedSim();
    bool live = CurrentMode == Mode::Live || CurrentMode == Mode::Options;
    if (live) {
        DrawHeadlines();
        // plumbob over the selected Sim (TS1)
        if (sim != nullptr && sim->Position().Level <= s.World()->State.Level()) {
            float x, y;
            auto zoom = s.World()->State.Zoom();
            float zs = (zoom == FSO::LotView::WorldZoom::Near) ? 2.0f : (zoom == FSO::LotView::WorldZoom::Medium ? 1.0f : 0.5f);
            float lift = (sim->GetPersonData(FSO::SimAntics::Model::VMPersonDataVariable::PersonsAge) < 18) ? 1.5f : 2.05f;
            if (s.EntityScreenPos(sim, x, y, lift)) {
                int px = (int)x, py = (int)y - (int)(8 * zs);
                int w = std::max(3, (int)(6 * zs)), h = std::max(5, (int)(10 * zs));
                py += (int)(std::sin((float)Time / 300.0f) * 1.5f * zs);
                // TS1(sims): spinning, shaded 3-D plumbob (one turn every 2.4 s)
                DrawPlumbob(*Top, (float)px, (float)py, (float)w, (float)h, (float)std::fmod(Time / 2400.0, 1.0) * 6.2831853f);
            }
        }
        if (Pie.IsOpen && PieMarkX >= 0) {
            int r = 6 + (int)(2 * std::sin((float)Time / 120.0f));
            Top->StrokeCircle((int)PieMarkX, (int)PieMarkY, r, RGBA(255, 255, 255, 230));
            Top->StrokeCircle((int)PieMarkX, (int)PieMarkY, r + 1, RGBA(0, 0, 0, 150));
        }
        if (CurrentMode == Mode::Live) DrawQueueTop(sim);
    }
    DrawPopups();
    PanelOf(this)->DrawTop(*Top);
    if (!TopTip.empty() && Time < TopTipUntil) {
        int w = TextWidth(FontId::Bold, TopTip) + 10;
        int x = std::max(2, std::min(TOP_W - w - 2, TopTipX - w / 2)), y = std::max(2, std::min(TOP_H - 20, TopTipY));
        Top->FillRoundRect(x, y, w, 17, 4, RGBA(255, 255, 210, 235));
        Top->StrokeRoundRect(x, y, w, 17, 4, RGBA(0, 0, 0, 200));
        DrawText(*Top, FontId::Bold, x + 5, y + 2, TopTip, TopTipColor ? TopTipColor : RGBA(0, 0, 0));
    }
    // level indicator when not on the ground floor (TS1 shows the story in the view controls)
    if (s.World()->State.Level() > 1) {
        char lv[32]; snprintf(lv, sizeof lv, "%s", GameText::Get().UI(138, s.World()->State.Level() - 1, "Second Story").c_str());
        DrawTextShadow(*Top, FontId::Small, TOP_W - 4 - TextWidth(FontId::Small, lv), TOP_H - 16, lv, RGBA(255, 255, 255), RGBA(0, 0, 0, 180));
    }
    DrawCheatOverlays();
    DrawCursor();
    if (ShowStats && !StatsLine.empty()) {
        Top->FillRect(0, TOP_H - 14, TOP_W, 14, RGBA(0, 0, 0, 150));
        DrawText(*Top, FontId::Small, 4, TOP_H - 14, StatsLine, RGBA(200, 255, 200));
    }
}

// TS1(sims): the debug views the original's cheats switch on (app/ui/UICheatConsole):
//   tile_info       the tile under the cursor: position, story, room, floor pattern, wall segments, terrain, grass
//   route_balloons  a balloon on every waypoint of the routes the Sims are walking (draw_routes: the path as a line)
void LotScreen::DrawCheatOverlays() {
    auto& C = CheatState::Get();
    auto world = S->World();
    auto vm = S->VM();
    if (world == nullptr || vm == nullptr) return;
    if (C.RouteBalloons || C.DrawRoutes) {
        int level = world->State.Level();
        for (auto e : vm->Entities) {
            auto av = dynamic_cast<VMAvatar*>(e);
            if (av == nullptr || av->Thread == nullptr || av->Position().Level > level) continue;
            for (auto& frame : av->Thread->Stack) {
                auto rf = dynamic_cast<FSO::SimAntics::Engine::VMRoutingFrame*>(frame.get());
                if (rf == nullptr) continue;
                std::vector<st::Vector2> pts;
                float z = (av->Position().Level - 1) * 2.95f;
                if (rf->WalkTo) for (auto& p : *rf->WalkTo) pts.push_back(TileToScreen(*world, st::Vector3(p.X / 16.0f, p.Y / 16.0f, z)));
                if (C.DrawRoutes) {
                    float sx, sy;
                    if (S->EntityScreenPos(av, sx, sy, 0.0f) && !pts.empty()) Top->Line((int)sx, (int)sy, (int)pts[0].X, (int)pts[0].Y, RGBA(255, 255, 0, 220));
                    for (size_t i = 1; i < pts.size(); i++) Top->Line((int)pts[i - 1].X, (int)pts[i - 1].Y, (int)pts[i].X, (int)pts[i].Y, RGBA(255, 255, 0, 220));
                }
                if (C.RouteBalloons) for (auto& p : pts) {
                    int x = (int)p.X, y = (int)p.Y - 10;
                    Top->FillCircle(x, y, 4, RGBA(255, 255, 255, 230));
                    Top->StrokeCircle(x, y, 4, RGBA(0, 0, 0, 200));
                    Top->Line(x, y + 4, x, y + 10, RGBA(255, 255, 255, 200));
                }
                break; // the innermost route only
            }
        }
    }
    if (C.TileInfo) {
        auto arch = vm->Context->Architecture();
        auto tile = world->State.WorldSpace.GetTileAtPosWithScroll(st::Vector2((float)CursorX, (float)CursorY));
        int tx = (int)std::floor(tile.X), ty = (int)std::floor(tile.Y), lv = world->State.Level();
        char b[160];
        if (arch && tx >= 0 && ty >= 0 && tx < arch->Width && ty < arch->Height) {
            auto f = arch->GetFloor((int16_t)tx, (int16_t)ty, (int8_t)lv);
            auto w = arch->GetWall((int16_t)tx, (int16_t)ty, (int8_t)lv);
            int room = 0;
            if (lv - 1 < (int)arch->Rooms.size() && (size_t)(ty * arch->Width + tx) < arch->Rooms[(size_t)(lv - 1)].Map.size())
                room = (int)(arch->Rooms[(size_t)(lv - 1)].Map[(size_t)(ty * arch->Width + tx)] & 0xFFFF);
            int alt = arch->Terrain ? arch->GetTerrainHeight((int16_t)tx, (int16_t)ty) : 0;
            int grass = arch->Terrain ? arch->GetTerrainGrass((int16_t)tx, (int16_t)ty) : 0;
            snprintf(b, sizeof b, "tile %d,%d  story %d  room %d  floor %d  wall %02X  alt %d  grass %d%s", tx, ty, lv, room, (int)f.Pattern,
                     (int)w.Segments, alt, grass, vm->Context->IsUserOutOfBounds(FSO::LotView::Model::LotTilePos::FromBigTile((int16_t)tx, (int16_t)ty, (int8_t)lv)) ? "  (locked)" : "");
        } else snprintf(b, sizeof b, "tile %d,%d  (off the lot)", tx, ty);
        int w = TextWidth(FontId::Small, b) + 8;
        Top->FillRect(TOP_W - w - 2, TOP_H - 32, w, 14, RGBA(0, 0, 0, 170));
        DrawText(*Top, FontId::Small, TOP_W - w + 2, TOP_H - 32, b, RGBA(255, 255, 160));
    }
}

void LotScreen::DrawBottom() {
    Bottom->Clear(RGBA(4, 6, 40));
    Hits.clear();
    UIUCP::Draw(*this, *Bottom, GadgetRect);
    PanelOf(this)->Draw(*Bottom, PanelRect);
    // TS1: the active Sim's head in the middle of the pie (C# UIPieMenu draws the avatar head)
    if (Pie.IsOpen) Pie.Draw(*Bottom, S->SelectedSim() ? FaceFor(S->SelectedSim(), 0) : nullptr);
    if (!S->Dialogs.empty()) DrawDialog();
}

void LotScreen::DrawDialog() {
    auto& d = S->Dialogs.front();
    Bottom->Dim(0, 0, BOT_W, BOT_H, 100);
    auto lines = WrapText(FontId::Small, ToLatin1(d.Message), BOT_W - 48 - 40);
    if (lines.size() > 9) lines.resize(9);
    int h = 30 + std::max(40, (int)lines.size() * 13) + 34;
    Rect p(10, std::max(4, (BOT_H - h) / 2), BOT_W - 20, h);
    // TS1 dialog: dark blue frame with a light edge (Shared\Dlg\Gendlg.bmp is the TS1 tile); the object's icon left
    Bottom->FillRoundRect(p, 10, RGBA(14, 22, 96, 245));
    Bottom->StrokeRoundRect(p.X, p.Y, p.W, p.H, 10, RGBA(140, 150, 230));
    DrawTextCentered(*Bottom, FontId::Bold, Rect(p.X, p.Y + 5, p.W, 16), Ellipsize(FontId::Bold, ToLatin1(d.Title), p.W - 10), RGBA(255, 230, 120));
    int textX = p.X + 10;
    if (d.CallerID != 0) {
        auto caller = S->VM()->GetObjectById(d.CallerID);
        auto icon = caller ? IconFor(caller) : nullptr;
        if (icon) {
            Bottom->FillRoundRect(p.X + 8, p.Y + 26, 40, 40, 4, RGBA(0, 0, 30));
            Bottom->BlitScaled(icon->Px.data(), icon->Width, icon->Height, Rect(p.X + 10, p.Y + 28, 36, 36), true);
            textX = p.X + 54;
        }
    }
    int y = p.Y + 26;
    for (auto& l : lines) { DrawText(*Bottom, FontId::Small, textX, y, l, theme::TextBright); y += 13; }
    _DialogButtons.clear();
    std::vector<std::pair<std::string, int>> btns;
    if (d.HasYes) btns.push_back({ ToLatin1(d.Yes), 0 });
    if (d.HasNo) btns.push_back({ ToLatin1(d.No), 1 });
    if (d.HasCancel) btns.push_back({ ToLatin1(d.Cancel), 2 });
    if (btns.empty()) btns.push_back({ Str->Get(152, 0, "OK"), 0 });
    int bw = std::min(90, (p.W - 20) / (int)btns.size() - 6);
    int bx = p.X + (p.W - (int)btns.size() * (bw + 6)) / 2;
    for (auto& b : btns) {
        Rect r(bx, p.Bottom() - 28, bw, 22);
        if (!UIGraphics::Get().DrawButton(*Bottom, "cpanel/Buttons/ButtonTileDialog.BMP", ButtonState::Normal, r)) {
            Bottom->FillRoundRect(r, 6, theme::ButtonBg);
            Bottom->StrokeRoundRect(r.X, r.Y, r.W, r.H, 6, theme::PanelEdge);
        }
        DrawTextCentered(*Bottom, FontId::Bold, r, Ellipsize(FontId::Bold, b.first, r.W - 6), theme::TextBright);
        _DialogButtons.push_back({ r, b.second });
        bx += bw + 6;
    }
}

// ======================================================================= scripting

bool LotScreen::ScriptButtonPos(const std::string& id, int& x, int& y) const {
    for (auto& h : Hits) if (h.Id == id) { x = h.R.X + h.R.W / 2; y = h.R.Y + h.R.H / 2; return true; }
    return false;
}

bool LotScreen::ScriptObjectPos(const std::string& name, int& x, int& y, std::string* found) {
    auto& s = *S;
    auto world = s.World();
    std::string want = Lower(name);
    if (ViewRect.W <= 0) return false;
    for (auto e : s.VM()->Entities) {
        if (e == nullptr || e->WorldUI == nullptr || e == s.GotoObject) continue;
        if (Lower(e->ToString()).find(want) == std::string::npos) continue;
        float cx, cy;
        float lift = dynamic_cast<VMAvatar*>(e) ? 1.2f : 0.4f;
        if (!s.EntityScreenPos(e, cx, cy, lift)) continue;
        if (e->Position().Level != world->State.Level()) continue;
        if (cx < 30 || cy < 30 || cx > TOP_W - 30 || cy > TOP_H - 30) {
            s.CenterOn(e);
            s.Draw();
            if (!s.EntityScreenPos(e, cx, cy, lift)) continue;
        }
        int16_t want_id = e->ObjectID;
        int16_t leader = (e->MultitileGroup != nullptr) ? e->MultitileGroup->GetInteractionGroupLeader(e)->ObjectID : want_id;
        for (int rad = 0; rad <= 40; rad += 3) {
            for (int dy = -rad; dy <= rad; dy += 3) for (int dx = -rad; dx <= rad; dx += 3) {
                if (std::max(std::abs(dx), std::abs(dy)) != rad) continue;
                int tx = (int)cx + dx, ty = (int)cy + dy;
                if (tx < 0 || ty < 0 || tx >= TOP_W || ty >= TOP_H) continue;
                int bx, by; TopToView((float)tx, (float)ty, bx, by);
                int rtx, rty; ViewToTop(bx, by, rtx, rty);
                auto hit = PickObjectAt(*world, &s.LastFrame(), rtx, rty, 3);
                if (hit.ObjectID == 0) continue;
                auto he = s.VM()->GetObjectById(hit.ObjectID);
                int16_t hl = (he && he->MultitileGroup) ? he->MultitileGroup->GetInteractionGroupLeader(he)->ObjectID : hit.ObjectID;
                if (hit.ObjectID == want_id || hl == leader) {
                    x = bx; y = by;
                    if (found) *found = e->ToString();
                    return true;
                }
            }
        }
    }
    return false;
}

std::string LotScreen::DescribeQueue() {
    std::string r;
    auto sim = S->SelectedSim();
    if (sim == nullptr || sim->Thread == nullptr) return r;
    r = sim->ToString() + ":";
    for (auto& a : sim->Thread->Queue)
        r += " [" + a->Name + (a->Callee ? " @ " + a->Callee->ToString() : std::string("")) + (Cancelled.count(a->UID) ? " (cancelled)" : "") + "]";
    auto p = sim->Position();
    char b[64]; snprintf(b, sizeof b, " pos (%d,%d)", p.TileX(), p.TileY());
    return r + b;
}

}
