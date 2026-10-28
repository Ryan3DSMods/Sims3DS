/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims app — see UILiveMode.h (FreeSims UI/Panels/UILiveMode.cs, UIInteractionQueue.cs, UIPersonGrid.cs,
// UIPersonPage.cs, UIJobInfo.cs)
#include "app/live/UILiveMode.h"
#include "audio/GameAudio.h"
#include "app/LotScreen.h"
#include "app/Font.h"
#include "app/Theme.h"
#include "app/WorldPicking.h"
#include "app/ui/UIGraphics.h"
#include "app/ui/GameText.h"
#include "app/ui/UIUCP.h"
#include "content/ContentManager.h"
#include "content/TS1JobProvider.h"
#include "content/TS1NeighbourProvider.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"
#include "simantics/VMArchitecture.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/entities/VMGameObject.h"
#include "simantics/entities/VMMultitileGroup.h"
#include "simantics/engine/VMThread.h"
#include "simantics/engine/VMQueuedAction.h"
#include "simantics/engine/VMTS1Neighbourhood.h"
#include "simantics/model/VMMotive.h"
#include "simantics/model/VMPersonDataVariable.h"
#include "files/formats/iff/chunks/BMP.h"
#include "files/formats/iff/chunks/CARR.h"
#include "files/formats/iff/chunks/NBRS.h"
#include "files/formats/iff/chunks/OBJD.h"
#include "world/World.h"
#include "world/model/FloorTile.h"
#include <algorithm>
#include <cstdio>
#include <map>
#include <memory>
#include <set>

namespace simapp {

using namespace FSO::SimAntics;
using FSO::SimAntics::Model::VMMotive;
using FSO::SimAntics::Model::VMPersonDataVariable;

UILiveMode::UILiveMode(LotScreen* owner) : UIModePanel(owner) {
    auto& gt = GameText::Get();
    // Live.iff STR# 130: Hunger 1, Energy 3, Comfort 5, Fun 7, Hygiene 9, Social 11, Bladder 13, Room 15
    static const int idx[8] = { 1, 5, 9, 13, 3, 7, 11, 15 };
    for (int i = 0; i < 8; i++) Motives.MotiveNames[i] = gt.Live(130, idx[i], Motives.MotiveNames[i].c_str());
}

void UILiveMode::Activate() { Motives.Reset(); _MotiveSim = -1; }

std::string UILiveMode::CursorArt() const { return _CursorArt; }

// ======================================================================= input

bool UILiveMode::Tap(const std::string& id) {
    auto& s = *L->S;
    if (id.rfind("sim", 0) == 0) {
        int i = atoi(id.c_str() + 3);
        if (i == s.Selected) s.CenterOn(s.SelectedSim()); // TS1: clicking the selected portrait centres the view
        s.Select(i);
        return true;
    }
    if (id.rfind("queue", 0) == 0) {
        int i = atoi(id.c_str() + 5);
        if (i >= 0 && i < (int)_QueueUIDs.size()) {
            s.CancelAction(_QueueUIDs[(size_t)i]); // UIInteractionQueue -> VMNetInteractionCancelCmd
            simaudio::GameAudio::Get().PlayUI("ui_queue_delete");
            L->Cancelled.insert(_QueueUIDs[(size_t)i]);
            if (L->Log) *L->Log += "cancel queue item\n";
        }
        return true;
    }
    if (id.rfind("tab_", 0) == 0) {
        Tab t = Tab::Mood;
        if (id == "tab_job") t = Tab::Job;
        else if (id == "tab_pers") t = Tab::Personality;
        else if (id == "tab_rel") t = Tab::Relationships;
        else if (id == "tab_house") t = Tab::House;
        else if (id == "tab_inv") t = Tab::Inventory;
        if (t == CurrentTab) Expanded = !Expanded;
        else CurrentTab = t;
        _RelScroll = 0;
        return true;
    }
    if (id.rfind("inv_sort", 0) == 0) { _InvSort = atoi(id.c_str() + 8); _InvScroll = 0; return true; }
    if (id == "inv_prev") { _InvScroll = std::max(0, _InvScroll - 1); return true; }
    if (id == "inv_next") { _InvScroll++; return true; }
    if (id == "rel_prev") { _RelScroll = std::max(0, _RelScroll - 1); return true; }
    if (id == "rel_next") { _RelScroll++; return true; }
    if (id == "panel") { Expanded = !Expanded; return true; }
    return false;
}

bool UILiveMode::Buttons(const AppInput& in) {
    auto& s = *L->S;
    if (!s.Family.empty()) {
        int n = (int)s.Family.size();
        if (in.Down & BTN_RIGHT) s.Select((s.Selected + 1) % n);
        if (in.Down & BTN_LEFT) s.Select((s.Selected + n - 1) % n);
    }
    if (in.Down & BTN_Y) s.CenterOn(s.SelectedSim());
    if ((in.Down & BTN_B) && Expanded) { Expanded = false; return true; }
    return false;
}

void UILiveMode::PointerUpdate() {
    auto& P = L->P;
    if (P.Click) { L->OpenPieAt(P.X, P.Y); return; }
    // TS1 cursors: the hand changes over Sims / objects (cheap: only when the cursor moved)
    if (!P.FromTouch && L->CursorShown && (P.X != _HoverCheckX || P.Y != _HoverCheckY)) {
        _HoverCheckX = P.X; _HoverCheckY = P.Y;
        auto hit = PickObjectAt(*L->S->World(), &L->S->LastFrame(), P.X, P.Y, 1);
        if (hit.ObjectID == 0) _CursorArt = "Shared/cursors/LiveNothing.cur";
        else _CursorArt = hit.IsAvatar ? "Shared/cursors/LivePerson.cur" : "Shared/cursors/LiveObjectAvail.cur";
    }
}

// ======================================================================= drawing

void UILiveMode::DrawPips(Canvas& c, int x, int y, int v, int pipW, int pipH, uint32_t on, uint32_t off) {
    int n = std::max(0, std::min(10, (v + 50) / 100));
    for (int i = 0; i < 10; i++) {
        c.FillRect(x + i * (pipW + 1), y, pipW, pipH, i < n ? on : off);
    }
}

void UILiveMode::DrawPortraits(Canvas& c, const Rect& r) {
    auto& s = *L->S;
    int n = (int)s.Family.size();
    if (n == 0) {
        DrawText(c, FontId::Small, r.X + 4, r.Y + 9, GameText::Get().UI(133, 0, "No one lives here!"), theme::Text);
        return;
    }
    int w = std::min(30, (r.W - 2) / n);
    for (int i = 0; i < n; i++) {
        auto sim = s.Family[(size_t)i];
        Rect pr(r.X + i * w, r.Y, w - 2, r.H);
        bool sel = i == s.Selected;
        auto face = L->FaceFor(sim, sel ? 2 : 0);
        c.FillRect(pr, RGBA(0, 0, 40));
        if (face) c.BlitScaled(face->Px.data(), face->Width, face->Height, Rect(pr.X + 1, pr.Y + 1, pr.W - 2, pr.H - 2), false);
        else DrawTextCentered(c, FontId::Small, pr, ToLatin1(sim->ToString()).substr(0, 2), theme::TextBright);
        c.StrokeRect(pr.X, pr.Y, pr.W, pr.H, sel ? RGBA(0, 255, 255) : RGBA(70, 80, 160));
        if (sel) { // the plumbob over the selected portrait
            int px = pr.X + pr.W - 5, py = pr.Y + 5;
            c.FillTriangle(px - 3, py, px + 3, py, px, py - 5, theme::Plumbob);
            c.FillTriangle(px - 3, py, px + 3, py, px, py + 5, theme::PlumbobDark);
        }
        char id[16]; snprintf(id, sizeof id, "sim%d", i);
        L->Hit(pr, id);
    }
}

void UILiveMode::DrawSpeed(Canvas& c, const Rect& r) {
    // TS1 speed buttons: pause, 1, 2, 3 (cpanel\Buttons\pause.bmp, Speed1/2/3.bmp)
    auto& gt = GameText::Get();
    static const char* art[4] = { "cpanel/Buttons/pause.bmp", "cpanel/Buttons/Speed1.bmp", "cpanel/Buttons/Speed2.bmp", "cpanel/Buttons/Speed3.bmp" };
    static const int artW[4] = { 15, 11, 20, 32 };
    static const int artH[4] = { 30, 15, 15, 15 };
    int x = r.X;
    for (int i = 0; i < 4; i++) {
        int w = artW[i] * (i == 0 ? 1 : 3) / (i == 0 ? 1 : 2), h = (i == 0) ? 28 : artH[i] * 3 / 2;
        Rect br(x, r.Y + (r.H - h) / 2, w, h);
        bool on = L->Speed == i;
        char id[8]; snprintf(id, sizeof id, "speed%d", i);
        if (!UIGraphics::Get().DrawButton(c, art[i], on ? ButtonState::Highlight : ButtonState::Normal, br)) {
            c.FillRoundRect(br, 4, on ? theme::ButtonOn : theme::ButtonBg);
            uint32_t ic = on ? RGBA(30, 30, 30) : theme::TextBright;
            int cx = br.X + br.W / 2, cy = br.Y + br.H / 2;
            if (i == 0) { c.FillRect(cx - 4, cy - 5, 3, 10, ic); c.FillRect(cx + 1, cy - 5, 3, 10, ic); }
            else for (int k = 0; k < i; k++) { int x0 = cx - i * 5 / 2 + k * 5; c.FillTriangle(x0, cy - 5, x0, cy + 5, x0 + 5, cy, ic); }
        }
        if (on && i > 0) c.StrokeRect(br.X - 1, br.Y - 1, br.W + 2, br.H + 2, RGBA(0, 255, 255));
        L->Hit(Rect(br.X - 1, r.Y, br.W + 3, r.H), id); // fat-finger: the whole row height
        x += w + 3;
    }
    if (L->Speed == 0) {
        // TS1 pauses with the speed label flashing
        if (((int)(L->Time / 500)) % 2 == 0) DrawText(c, FontId::Small, x + 1, r.Y + 9, gt.UI(148, 0, "Paused"), RGBA(255, 230, 120));
    }
}

void UILiveMode::DrawQueue(Canvas& c, const Rect& r, VMAvatar* sim) {
    c.FillRect(r, RGBA(4, 8, 50));
    c.StrokeRect(r.X, r.Y, r.W, r.H, RGBA(70, 80, 160));
    _QueueUIDs.clear();
    if (sim == nullptr || sim->Thread == nullptr) return;
    int slot = std::min(r.W - 4, (r.H - 4) / 3);
    int y = r.Y + 2;
    for (size_t i = 0; i < sim->Thread->Queue.size() && _QueueUIDs.size() < 3; i++) {
        auto& a = sim->Thread->Queue[i];
        if (a->Mode == Engine::VMQueueMode::ParentExit || a->Mode == Engine::VMQueueMode::Idle) continue;
        Rect qr(r.X + (r.W - slot) / 2, y, slot, slot);
        bool active = _QueueUIDs.empty();
        c.FillRoundRect(qr, 4, active ? RGBA(30, 50, 140) : RGBA(16, 26, 90));
        c.StrokeRoundRect(qr.X, qr.Y, qr.W, qr.H, 4, active ? RGBA(0, 255, 255) : RGBA(90, 100, 190));
        auto icon = L->IconFor(a->IconOwner());
        if (icon) c.BlitScaled(icon->Px.data(), icon->Width, icon->Height, Rect(qr.X + 3, qr.Y + 3, qr.W - 6, qr.H - 6), true);
        else c.FillCircle(qr.X + qr.W / 2, qr.Y + qr.H / 2, qr.W / 3, theme::Plumbob);
        if (L->Cancelled.count(a->UID) && !UIGraphics::Get().DrawImage(c, "Other/QueueCancel.bmp", Rect(qr.X + 3, qr.Y + 3, qr.W - 6, qr.H - 6))) {
            c.Line(qr.X + 4, qr.Y + 4, qr.Right() - 5, qr.Bottom() - 5, RGBA(230, 60, 50));
            c.Line(qr.Right() - 5, qr.Y + 4, qr.X + 4, qr.Bottom() - 5, RGBA(230, 60, 50));
        }
        char id[16]; snprintf(id, sizeof id, "queue%d", (int)_QueueUIDs.size());
        L->Hit(qr, id);
        _QueueUIDs.push_back(a->UID);
        y += slot + 2;
    }
    if (_QueueUIDs.empty()) DrawTextCentered(c, FontId::Small, r, "-", theme::TextDim);
}

void UILiveMode::DrawTabs(Canvas& c, const Rect& r) {
    // TS1 live mode buttons (cpanel\Buttons\Mood.bmp, Job.bmp, Personality.bmp, Relationship.BMP, House.bmp)
    // TS1(sims): Hot Date's inventory button (cpanel\Buttons\Gift.bmp, Live.iff ModeTips 6 "Inventory")
    static const char* art[6] = { "cpanel/Buttons/Mood.bmp", "cpanel/Buttons/Job.bmp", "cpanel/Buttons/Personality.bmp",
                                  "cpanel/Buttons/Relationship.BMP", "cpanel/Buttons/House.bmp", "cpanel/Buttons/Gift.bmp" };
    static const char* ids[6] = { "tab_mood", "tab_job", "tab_pers", "tab_rel", "tab_house", "tab_inv" };
    static const int tipIdx[6] = { 0, 2, 4, 1, 3, 6 }; // Live.iff STR# 134 ModeTips
    static const char* fallback[6] = { "Mood", "Job", "Personality", "Relationships", "House", "Inventory" };
    int w = r.W / 6;
    for (int i = 0; i < 6; i++) {
        Rect tr(r.X + i * w, r.Y, w - 2, r.H);
        bool sel = (int)CurrentTab == i;
        c.FillRoundRect(tr, 4, sel ? RGBA(30, 60, 160) : RGBA(14, 22, 90));
        if (sel) c.StrokeRoundRect(tr.X, tr.Y, tr.W, tr.H, 4, RGBA(0, 255, 255));
        int ih = r.H - 2, iw = (i == 0) ? ih * 32 / 39 : ih * 27 / 30;
        Rect ir(tr.X + 3, tr.Y + 1, iw, ih);
        if (!UIGraphics::Get().DrawButton(c, art[i], sel ? ButtonState::Highlight : ButtonState::Normal, ir))
            c.FillCircle(ir.X + iw / 2, ir.Y + ih / 2, ih / 3, sel ? RGBA(0, 255, 255) : theme::Text);
        std::string label = GameText::Get().Live(134, tipIdx[i], fallback[i]);
        DrawText(c, FontId::Small, ir.Right() + 2, tr.Y + (tr.H - LineHeight(FontId::Small)) / 2,
                 Ellipsize(FontId::Small, label, tr.W - iw - 6), sel ? RGBA(0, 255, 255) : theme::Text);
        L->Hit(Rect(tr.X - 1, r.Y - 3, w, r.H + 6), ids[i]); // FIX(sims) FB1: 24 px tall touch area (the art stays 18)
    }
}

void UILiveMode::DrawMood(Canvas& c, const Rect& r, VMAvatar* sim) {
    static const VMMotive order[8] = { VMMotive::Hunger, VMMotive::Comfort, VMMotive::Hygiene, VMMotive::Bladder,
                                       VMMotive::Energy, VMMotive::Fun, VMMotive::Social, VMMotive::Room };
    for (int i = 0; i < 8; i++) Motives.MotiveValues[i] = (int16_t)std::max(-100, std::min(100, (int)sim->GetMotiveData(order[i])));
    Motives.Draw(c, r);
}

void UILiveMode::DrawJob(Canvas& c, const Rect& r, VMAvatar* sim) {
    auto& gt = GameText::Get();
    // left: career; right: the six skills (TS1 job panel)
    // FIX(sims) QA2: on the touch screen the skill names were cut to "Coo... Me... Cha... Cre...": narrower career column
    // and thinner skill pips there; the performance bar goes on the working-hours row (it was clipped at the bottom edge).
    int splitX = r.X + r.W * (r.H > 80 ? 45 : 37) / 100;
    int hoursY = -1, hoursEndX = 0;
    int jobType = sim->GetPersonData(VMPersonDataVariable::JobType);
    int level = sim->GetPersonData(VMPersonDataVariable::JobPromotionLevel);
    int perf = sim->GetPersonData(VMPersonDataVariable::JobPerformance);
    auto content = FSO::Content::Content::Get();
    FSO::Files::Formats::IFF::Chunks::CARR* job = (content && content->Jobs && jobType > 0) ? content->Jobs->GetJob((int16_t)jobType) : nullptr;
    int lh = LineHeight(FontId::Small);
    int y = r.Y + 2;
    if (job == nullptr || level < 0 || level >= (int)job->JobLevels.size()) {
        DrawText(c, FontId::Bold, r.X + 4, y, jobType > 0 ? job == nullptr ? std::string("Job #") + std::to_string(jobType) : ToLatin1(job->Name) : gt.Live(136, 7, "Unemployed"), RGBA(255, 230, 120));
        y += lh + 2;
        if (jobType <= 0 && r.H > 60) {
            for (auto& l : WrapText(FontId::Small, gt.Live(137, 13, ""), splitX - r.X - 8)) { if (y > r.Bottom() - lh) break; DrawText(c, FontId::Small, r.X + 4, y, l, theme::Text); y += lh; }
        }
    } else {
        auto& jl = job->JobLevels[(size_t)level];
        DrawText(c, FontId::Bold, r.X + 4, y, Ellipsize(FontId::Bold, ToLatin1(jl.JobName), splitX - r.X - 6), RGBA(255, 230, 120));
        y += lh + 1;
        char buf[96];
        snprintf(buf, sizeof buf, "%s %s", gt.Live(137, 14, "Salary").c_str(), UIUCP::Money(jl.Salary).c_str());
        DrawText(c, FontId::Small, r.X + 4, y, buf, RGBA(120, 255, 120)); y += lh;
        auto hr = [](int h) { char b[16]; int hh = h % 12; if (hh == 0) hh = 12; snprintf(b, sizeof b, "%d%s", hh, h < 12 ? "AM" : "PM"); return std::string(b); };
        snprintf(buf, sizeof buf, "%s-%s", hr(jl.StartTime).c_str(), hr(jl.EndTime).c_str());
        DrawText(c, FontId::Small, r.X + 4, y, buf, theme::Text);
        hoursY = y; hoursEndX = r.X + 4 + TextWidth(FontId::Small, buf);
        y += lh;
    }
    bool barOnHours = hoursY >= 0 && r.H <= 80 && splitX - 8 - (hoursEndX + 6) >= 30;
    if (jobType > 0 && (barOnHours || y + 8 <= r.Bottom())) {
        // performance bar (-100..100, cpanel\JobSubBars.bmp)
        Rect bar = barOnHours ? Rect(hoursEndX + 6, hoursY + (lh - 6) / 2, splitX - 8 - (hoursEndX + 6), 6) : Rect(r.X + 4, y + 1, splitX - r.X - 12, 6);
        c.FillRect(bar, RGBA(0, 0, 30));
        int p = std::max(-100, std::min(100, perf));
        int mid = bar.X + bar.W / 2;
        if (p >= 0) c.FillRect(mid, bar.Y, p * bar.W / 200, bar.H, RGBA(40, 220, 60));
        else c.FillRect(mid + p * bar.W / 200, bar.Y, -p * bar.W / 200, bar.H, RGBA(230, 50, 40));
        c.FillRect(mid, bar.Y - 1, 1, bar.H + 2, RGBA(255, 255, 255));
    }
    // FIX(sims): Superstar fame - the star meter of the TS1 job panel (cpanel\FameSubBarsOff.bmp under
    // cpanel\FameSubBars.bmp: 5 stars of 16 px), shown once the Sim has fame. Person data 81 = fame level 0..10 in
    // half stars (GameData/fame.iff STR#1 "Nobody" .. "Superstar"), 80 = fame score (Generic Sims Call 34/36). It was
    // not drawn at all.
    {
        int level = std::max(0, std::min(10, (int)sim->GetPersonData((VMPersonDataVariable)81)));
        int score = sim->GetPersonData((VMPersonDataVariable)80);
        int barW = std::min(splitX - r.X - 8, r.H > 80 ? 60 : 40), barH = barW * 16 / 80;
        // below the career when the panel is tall (top screen), else at the right end of the title row (3DS bottom)
        int top = y + (jobType > 0 && !barOnHours ? 9 : 0), left = r.X + 4;
        if (top + barH > r.Bottom()) { top = r.Y + 2; left = splitX - 4 - barW; }
        if (level > 0 || score != 0) {
            Rect dst(left, top, barW, barH);
            if (!UIGraphics::Get().DrawImage(c, "cpanel/Backgrounds/FameSubBarsOff.bmp", dst)) c.FillRect(dst, RGBA(0, 0, 60));
            if (level > 0) {
                Rect on(dst.X, dst.Y, barW * level / 10, barH);
                if (!UIGraphics::Get().DrawImage(c, "cpanel/FameSubBars.bmp", on, Rect(0, 0, 8 * level, 16))) c.FillRect(on, RGBA(0, 230, 255));
            }
        }
    }
    // skills: Cooking, Mechanical, Charisma, Body, Logic, Creativity (Live.iff STR# 136 1..6)
    static const VMPersonDataVariable skills[6] = { VMPersonDataVariable::CookingSkill, VMPersonDataVariable::MechanicalSkill,
        VMPersonDataVariable::CharismaSkill, VMPersonDataVariable::BodySkill, VMPersonDataVariable::LogicSkill, VMPersonDataVariable::CreativitySkill };
    static const char* names[6] = { "Cooking", "Mechanical", "Charisma", "Body", "Logic", "Creativity" };
    int sx = splitX + 2, sw = r.Right() - sx;
    bool big = r.H > 80;
    int cols = big ? 1 : 2, rows = big ? 6 : 3;
    int colW = sw / cols, rowH = std::max(8, r.H / rows);
    int pipW = big ? 8 : 2;
    int labelW = colW - (pipW + 1) * 10 - 4;
    // FIX(sims) QA3: in the two-column layout (touch screen) "Mechanical" was cut to "Mechan...": the columns are now
    // as wide as their longest label, the spare width split between them.
    int colX[2] = { sx, sx + colW }, colLabelW[2] = { labelW, labelW };
    if (cols == 2) {
        int pipsW = (pipW + 1) * 10, lw[2] = { 0, 0 };
        for (int i = 0; i < 6; i++) lw[i / rows] = std::max(lw[i / rows], TextWidth(FontId::Small, gt.Live(136, i + 1, names[i])));
        int spare = sw - (lw[0] + lw[1] + 2 * (pipsW + 2));
        if (spare < 0 && pipW > 1) { pipW = 1; pipsW = (pipW + 1) * 10; spare = sw - (lw[0] + lw[1] + 2 * (pipsW + 2)); }
        if (spare >= 0) {
            colLabelW[0] = lw[0] + spare / 4; colLabelW[1] = lw[1] + spare / 4;
            colX[1] = sx + colLabelW[0] + pipsW + 2 + spare / 2;
        }
    }
    for (int i = 0; i < 6; i++) {
        int col = i / rows, row = i % rows;
        int xx = colX[col], yy = r.Y + row * rowH;
        DrawText(c, FontId::Small, xx, yy + (rowH - lh) / 2, Ellipsize(FontId::Small, gt.Live(136, i + 1, names[i]), colLabelW[col]), theme::Text);
        DrawPips(c, xx + colLabelW[col] + 2, yy + (rowH - 6) / 2, sim->GetPersonData(skills[i]), pipW, 6, RGBA(80, 230, 255), RGBA(30, 40, 90));
    }
}

void UILiveMode::DrawPersonality(Canvas& c, const Rect& r, VMAvatar* sim) {
    auto& gt = GameText::Get();
    // Neat, Outgoing, Active, Playful, Nice (Live.iff STR# 131 1, 8, 15, 22, 29; person data 7, 6, 3, 5, 2)
    static const VMPersonDataVariable traits[5] = { VMPersonDataVariable::NeatPersonality, VMPersonDataVariable::OutgoingPersonality,
        VMPersonDataVariable::ActivePersonality, VMPersonDataVariable::PlayfulPersonality, VMPersonDataVariable::NicePersonality };
    static const char* names[5] = { "Neat", "Outgoing", "Active", "Playful", "Nice" };
    bool big = r.H > 80;
    int lh = LineHeight(FontId::Small);
    if (!big) {
        // two columns: 3 + 2 rows
        int colW = r.W / 2, rowH = r.H / 3;
        for (int i = 0; i < 5; i++) {
            int col = i / 3, row = i % 3;
            int x = r.X + 4 + col * colW, y = r.Y + row * rowH;
            DrawText(c, FontId::Small, x, y + (rowH - lh) / 2, gt.Live(131, 1 + i * 7, names[i]), theme::Text);
            DrawPips(c, x + 54, y + (rowH - 7) / 2, sim->GetPersonData(traits[i]), 6, 7, RGBA(255, 210, 60), RGBA(40, 40, 90));
        }
        return;
    }
    int rowH = r.H / 5;
    for (int i = 0; i < 5; i++) {
        int y = r.Y + i * rowH;
        int v = sim->GetPersonData(traits[i]);
        DrawText(c, FontId::Bold, r.X + 4, y + 1, gt.Live(131, 1 + i * 7, names[i]), RGBA(255, 230, 120));
        DrawPips(c, r.X + 74, y + 3, v, 8, 9, RGBA(255, 210, 60), RGBA(40, 40, 90));
        // the description for the trait's level (low / medium / high: STR# 131 base + 4..6)
        int band = v < 340 ? 0 : (v < 670 ? 1 : 2);
        std::string desc = gt.Live(131, 1 + i * 7 + 4 + band, "");
        auto lines = WrapText(FontId::Small, desc, r.W - 10);
        int yy = y + lh + 3;
        for (auto& l : lines) { if (yy + lh > y + rowH) break; DrawText(c, FontId::Small, r.X + 6, yy, l, theme::TextDim); yy += lh; }
    }
}

void UILiveMode::DrawRelationships(Canvas& c, const Rect& r, VMAvatar* sim) {
    using FSO::SimAntics::Engine::Utils::VMTS1Neighbourhood;
    auto provider = VMTS1Neighbourhood::Provider();
    int16_t me = VMTS1Neighbourhood::NeighborIDOf(sim);
    auto mine = me > 0 ? VMTS1Neighbourhood::GetNeighbor(me) : nullptr;
    if (provider == nullptr || mine == nullptr || mine->Relationships.empty()) {
        DrawTextCentered(c, FontId::Small, r, "No relationships yet", theme::TextDim);
        return;
    }
    // sorted by daily relationship, best first
    std::vector<std::pair<int, int16_t>> rels;
    for (auto& kv : mine->Relationships) {
        if (kv.first == me || kv.second.empty()) continue;
        rels.push_back({ kv.second[0], (int16_t)kv.first });
    }
    std::sort(rels.begin(), rels.end(), [](const std::pair<int, int16_t>& a, const std::pair<int, int16_t>& b) { return a.first > b.first; });
    int cellW = 52;
    int perPage = std::max(1, (r.W - 24) / cellW);
    if (r.H > 80) perPage *= std::max(1, r.H / 60);
    int cols = std::max(1, (r.W - 24) / cellW);
    _RelScroll = std::min(_RelScroll, std::max(0, ((int)rels.size() - 1) / perPage));
    int start = _RelScroll * perPage;
    auto content = FSO::Content::Content::Get();
    int lh = LineHeight(FontId::Small);
    for (int k = 0; k < perPage && start + k < (int)rels.size(); k++) {
        int16_t other = rels[(size_t)(start + k)].second;
        auto nb = VMTS1Neighbourhood::GetNeighbor(other);
        auto& vals = mine->Relationships[other];
        int daily = vals.size() > 0 ? vals[0] : 0;
        int life = vals.size() > 2 ? vals[2] : daily;
        int col = k % cols, row = k / cols;
        Rect cell(r.X + 12 + col * cellW, r.Y + row * 60, cellW - 4, std::min(r.H, 58));
        // face on the relationship colour (character BMP_ 2003 "rel. images": 5 colours, red .. green)
        int band = daily < -50 ? 0 : daily < 0 ? 1 : daily < 25 ? 2 : daily < 50 ? 3 : 4;
        int fs = std::min(cell.W - 14, cell.H - 14);
        Rect fr(cell.X + (cell.W - fs) / 2, cell.Y, fs, fs);
        bool drawn = false;
        if (nb != nullptr && content) {
            // decoded once per person and colour band
            static std::map<uint64_t, std::unique_ptr<Canvas>> relFaces;
            uint64_t key = ((uint64_t)nb->GUID << 3) | (uint64_t)band;
            auto it = relFaces.find(key);
            if (it == relFaces.end()) {
                std::unique_ptr<Canvas> frame;
                auto obj = content->WorldObjects->Get(nb->GUID, true);
                if (obj && obj->Resource) {
                    auto bmp = obj->Resource->Get<FSO::Files::Formats::IFF::Chunks::BMP>(2003);
                    Canvas all;
                    if (bmp && UIGraphics::DecodeBMP(bmp->GetData(), all) && all.Width >= 5) {
                        int fw = all.Width / 5;
                        frame.reset(new Canvas(fw, all.Height));
                        for (int yy = 0; yy < all.Height; yy++)
                            for (int xx = 0; xx < fw; xx++) frame->Px[(size_t)yy * fw + xx] = all.Px[(size_t)yy * all.Width + band * fw + xx] | 0xFF000000u;
                    }
                }
                if (relFaces.size() > 256) relFaces.clear();
                it = relFaces.emplace(key, std::move(frame)).first;
            }
            if (it->second) { c.BlitScaled(it->second->Px.data(), it->second->Width, it->second->Height, fr, false); drawn = true; }
        }
        if (!drawn) {
            static const uint32_t bandCol[5] = { RGBA(200, 30, 30), RGBA(130, 50, 40), RGBA(70, 100, 100), RGBA(40, 140, 60), RGBA(30, 210, 40) };
            c.FillRect(fr, bandCol[band]);
            if (nb) DrawTextCentered(c, FontId::Small, fr, ToLatin1(nb->Name).substr(0, 3), RGBA(255, 255, 255));
        }
        // daily (top) and lifetime (bottom) bars
        auto bar = [&](int y, int v) {
            Rect b(cell.X, y, cell.W, 3);
            c.FillRect(b, RGBA(0, 0, 30));
            int vv = std::max(-100, std::min(100, v));
            int mid = b.X + b.W / 2;
            if (vv >= 0) c.FillRect(mid, b.Y, vv * b.W / 200, b.H, RGBA(40, 220, 60));
            else c.FillRect(mid + vv * b.W / 200, b.Y, -vv * b.W / 200, b.H, RGBA(230, 50, 40));
        };
        bar(fr.Bottom() + 2, daily);
        bar(fr.Bottom() + 7, life);
        if (r.H > 80 && nb) DrawTextCentered(c, FontId::Small, Rect(cell.X - 2, fr.Bottom() + 10, cell.W + 4, lh), Ellipsize(FontId::Small, ToLatin1(nb->Name), cell.W + 4), theme::Text);
    }
    // page arrows
    if (_RelScroll > 0) { c.FillTriangle(r.X + 2, r.Y + r.H / 2, r.X + 9, r.Y + r.H / 2 - 7, r.X + 9, r.Y + r.H / 2 + 7, RGBA(0, 255, 255)); L->Hit(Rect(r.X, r.Y, 11, r.H), "rel_prev"); }
    if (start + perPage < (int)rels.size()) { c.FillTriangle(r.Right() - 2, r.Y + r.H / 2, r.Right() - 9, r.Y + r.H / 2 - 7, r.Right() - 9, r.Y + r.H / 2 + 7, RGBA(0, 255, 255)); L->Hit(Rect(r.Right() - 11, r.Y, 11, r.H), "rel_next"); }
}

// TS1(sims): the inventory panel (Hot Date+): the selected Sim's tokens from the neighbourhood (NGBH inventory,
// FreeSO TS1NeighborhoodProvider.GetInventoryByNID, filled by primitive 51 Manage Inventory): gifts (Hot Date),
// souvenirs (Vacation), spell ingredients / MagiCoins (Makin' Magic) ... sorted by the three TS1 buttons
// (cpanel\Buttons\InventoryGiftSort / MagicSort / OtherSort.bmp). Token types (FreeSO): 2 = system tokens (dates,
// autofollow; hidden), 4 = Magic Town purchasables, 6 = vacation purchasables; ASSUMED: 4/5 and MagiCoins are "magic",
// other non-system objects that are gift tokens (OBJD type 12) or type 1 are "gifts", the rest "other".
void UILiveMode::DrawInventory(Canvas& c, const Rect& r, VMAvatar* sim) {
    static const char* sortArt[3] = { "cpanel/Buttons/InventoryGiftSort.bmp", "cpanel/Buttons/InventoryMagicSort.bmp", "cpanel/Buttons/InventoryOtherSort.bmp" };
    for (int i = 0; i < 3; i++) {
        Rect br(r.X + 2, r.Y + 2 + i * 16, 16, 15);
        if (r.H < 50) br = Rect(r.X + 2 + i * 17, r.Y + 2, 16, 15);
        if (!UIGraphics::Get().DrawButton(c, sortArt[i], _InvSort == i ? ButtonState::Pressed : ButtonState::Normal, br))
            c.FillRoundRect(br, 3, _InvSort == i ? RGBA(30, 120, 170) : theme::ButtonBg);
        L->Hit(br, "inv_sort" + std::to_string(i));
    }
    auto content = FSO::Content::Content::Get();
    auto prov = content ? content->Neighborhood.get() : nullptr;
    int nid = sim->GetPersonData(VMPersonDataVariable::NeighborId);
    auto inv = prov ? prov->GetInventoryByNID((int16_t)nid) : nullptr;
    struct Entry { uint32_t GUID; int Count; };
    std::vector<Entry> items;
    if (inv != nullptr) {
        for (auto& it : *inv) {
            if (it.Type == 2 || it.GUID < 0x100 || it.Count == 0) continue; // system tokens (follow home, autofollow ...)
            auto obj = content->WorldObjects->Get(it.GUID, true);
            bool magic = it.Type == 4 || it.Type == 5 || it.GUID == 0x99E81BEC; // 0x99E81BEC: MagiCoins (FreeSO)
            bool gift = !magic && (it.Type == 1 || (obj && obj->OBJ && obj->OBJ->ObjectType == FSO::Files::Formats::IFF::Chunks::OBJDType::GiftToken));
            int sort = magic ? 1 : (gift ? 0 : 2);
            if (sort == _InvSort) items.push_back({ it.GUID, it.Count });
        }
    }
    int x0 = (r.H < 50) ? r.X + 56 : r.X + 22;
    Rect grid(x0, r.Y + 2, r.Right() - x0 - 14, r.H - 4);
    if (items.empty()) {
        static const char* none[3] = { "No gifts.", "No magic items.", "Nothing else." };
        DrawTextCentered(c, FontId::Small, grid, none[_InvSort], theme::TextDim);
        return;
    }
    int cell = 26, cols = std::max(1, grid.W / (cell + 2)), rows = std::max(1, grid.H / (cell + 2));
    int perPage = cols * rows, pages = ((int)items.size() + perPage - 1) / perPage;
    if (_InvScroll >= pages) _InvScroll = pages - 1;
    for (int k = 0; k < perPage; k++) {
        size_t i = (size_t)(_InvScroll * perPage + k);
        if (i >= items.size()) break;
        Rect cr(grid.X + (k % cols) * (cell + 2), grid.Y + (k / cols) * (cell + 2), cell, cell);
        c.FillRoundRect(cr, 3, RGBA(14, 22, 90));
        if (auto icon = L->IconForGUID(items[i].GUID)) {
            float sc = std::min((cell - 2) / (float)icon->Width, (cell - 2) / (float)icon->Height);
            int w = (int)(icon->Width * sc), h = (int)(icon->Height * sc);
            c.BlitScaled(icon->Px.data(), icon->Width, icon->Height, Rect(cr.X + (cell - w) / 2, cr.Y + (cell - h) / 2, w, h), true);
        } else {
            DrawTextCentered(c, FontId::Small, cr, Ellipsize(FontId::Small, L->NameForGUID(items[i].GUID), cell), theme::Text);
        }
        if (items[i].Count > 1) {
            std::string n = std::to_string(items[i].Count);
            DrawText(c, FontId::Small, cr.Right() - TextWidth(FontId::Small, n) - 1, cr.Bottom() - LineHeight(FontId::Small), n, RGBA(255, 230, 120));
        }
    }
    if (pages > 1) {
        Rect pn(r.Right() - 12, r.Y + 2, 11, r.H / 2 - 2), pp(r.Right() - 12, r.Y + r.H / 2, 11, r.H / 2 - 2);
        c.FillTriangle(pn.X + 2, pn.Bottom() - 2, pn.Right() - 2, pn.Bottom() - 2, pn.X + pn.W / 2, pn.Y + 2, _InvScroll > 0 ? RGBA(0, 255, 255) : RGBA(50, 60, 100));
        c.FillTriangle(pp.X + 2, pp.Y + 2, pp.Right() - 2, pp.Y + 2, pp.X + pp.W / 2, pp.Bottom() - 2, _InvScroll + 1 < pages ? RGBA(0, 255, 255) : RGBA(50, 60, 100));
        L->Hit(pn, "inv_prev"); L->Hit(pp, "inv_next");
    }
}

void UILiveMode::DrawHouse(Canvas& c, const Rect& r) {
    auto& gt = GameText::Get();
    auto& s = *L->S;
    auto arch = s.Context()->Architecture();
    // TS1 house panel numbers: square feet (indoor floor tiles x 100), bedrooms, bathrooms, value
    int tiles = 0;
    for (size_t lv = 0; lv < arch->Floors.size() && lv < 2; lv++) {
        auto& fl = arch->Floors[lv];
        auto& rooms = arch->Rooms[lv].Map;
        for (size_t i = 0; i < fl.size(); i++) {
            if (fl[i].Pattern == 0) continue;
            uint32_t room = (i < rooms.size()) ? (rooms[i] & 0xFFFF) : 0;
            if (room > 0 && room < arch->RoomData.size() && !arch->RoomData[room].IsOutside) tiles++;
        }
    }
    int beds = 0, baths = 0, value = 0;
    std::set<void*> groups;
    for (auto e : s.VM()->Entities) {
        auto go = dynamic_cast<VMGameObject*>(e);
        if (go == nullptr || go->MultitileGroup == nullptr || groups.count(go->MultitileGroup)) continue;
        groups.insert(go->MultitileGroup);
        auto base = go->MultitileGroup->BaseObject();
        auto def = base->MasterDefinition ? base->MasterDefinition : (base->Object ? base->Object->OBJ : nullptr);
        if (def == nullptr) continue;
        if (def->Price > 0) value += def->Price;
        if ((def->FunctionFlags & 0x01) && (def->RoomFlags & 0x02) && def->Price >= 300) beds++;   // seating/bedroom = beds
        if ((def->FunctionFlags & 0x10) && def->BHAV_WashHandsID == 0 && (def->RoomFlags & 0x04) && def->Price >= 200) baths++;
    }
    int lh = LineHeight(FontId::Small);
    char buf[96];
    int y = r.Y + 2;
    auto line = [&](const std::string& s1) { DrawText(c, FontId::Small, r.X + 6, y, s1, theme::Text); y += lh + 1; };
    std::string fmt = gt.Live(133, 6, "Sq. Ft.: %d");
    snprintf(buf, sizeof buf, fmt.c_str(), tiles * 100); line(buf);
    fmt = gt.Live(133, 7, "Bedrooms: %d"); snprintf(buf, sizeof buf, fmt.c_str(), std::max(0, beds)); line(buf);
    fmt = gt.Live(133, 8, "Bathrooms: %d"); snprintf(buf, sizeof buf, fmt.c_str(), std::max(0, baths)); line(buf);
    DrawText(c, FontId::Bold, r.X + r.W / 2, r.Y + 2, gt.Live(133, 0, "House"), RGBA(255, 230, 120));
    snprintf(buf, sizeof buf, "Furnishings %s", UIUCP::Money(value).c_str());
    DrawText(c, FontId::Small, r.X + r.W / 2, r.Y + 3 + lh, buf, RGBA(120, 255, 120));
    std::string fam = s.FamilyName.empty() ? std::string("") : ("The " + ToLatin1(s.FamilyName) + " family");
    DrawText(c, FontId::Small, r.X + r.W / 2, r.Y + 4 + 2 * lh, Ellipsize(FontId::Small, fam, r.W / 2 - 4), theme::Text);
}

void UILiveMode::Draw(Canvas& c, const Rect& a) {
    auto& s = *L->S;
    auto sim = s.SelectedSim();
    L->DrawPanelFrame(c, a);
    // motive history follows the selected Sim
    if (s.Selected != _MotiveSim) { Motives.Reset(); _MotiveSim = s.Selected; }
    if (sim != nullptr) {
        static const VMMotive order[8] = { VMMotive::Hunger, VMMotive::Comfort, VMMotive::Hygiene, VMMotive::Bladder,
                                           VMMotive::Energy, VMMotive::Fun, VMMotive::Social, VMMotive::Room };
        for (int i = 0; i < 8; i++) Motives.MotiveValues[i] = (int16_t)std::max(-100, std::min(100, (int)sim->GetMotiveData(order[i])));
        Motives.Update();
    }

    // row 0: portraits + speed
    DrawPortraits(c, Rect(a.X + 3, a.Y + 1, a.W - 112, 30));
    DrawSpeed(c, Rect(a.Right() - 108, a.Y + 1, 106, 30));

    Rect info;
    if (!Expanded) {
        // the touch view + the queue column
        L->DrawTouchView(c, Rect(a.X + 1, a.Y + 33, 225, 134));
        DrawQueue(c, Rect(a.X + 228, a.Y + 33, a.W - 229, 134), sim);
        DrawTabs(c, Rect(a.X + 2, a.Y + 169, a.W - 2, 18));
        info = Rect(a.X + 2, a.Y + 189, a.W - 4, a.H - 190);
    } else {
        L->ViewRect = Rect(); // no touch view while the sub-panel is enlarged
        DrawTabs(c, Rect(a.X + 2, a.Y + 33, a.W - 2, 18));
        info = Rect(a.X + 2, a.Y + 54, a.W - 4, a.H - 56);
    }
    c.FillRect(info, RGBA(4, 8, 50, 200));
    if (sim == nullptr) { DrawTextCentered(c, FontId::Small, info, GameText::Get().UI(133, 1, "Select a Sim"), theme::TextDim); return; }
    switch (CurrentTab) {
        case Tab::Mood: DrawMood(c, Rect(info.X, info.Y + 1, info.W, info.H - 2), sim); break;
        case Tab::Job: DrawJob(c, info, sim); break;
        case Tab::Personality: DrawPersonality(c, info, sim); break;
        case Tab::Relationships: DrawRelationships(c, info, sim); break;
        case Tab::House: DrawHouse(c, info); break;
        case Tab::Inventory: DrawInventory(c, info, sim); break;
    }
}

}
