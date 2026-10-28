/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims app — FreeSims UI/Panels/UILiveMode.cs: the live mode panel — family portraits (UIPersonGrid),
// speed controls, the interaction queue (UIInteractionQueue: tap an icon to cancel, the red cross is
// Other\QueueCancel.bmp) and the sub-panels TS1 has behind its live mode buttons (Live.iff STR# 134 "ModeTips"):
// Mood (UIMotiveDisplay), Job (career, salary, hours, performance + the six skills), Personality (five traits,
// ten points each), Relationships (faces with daily / lifetime bars), House (size, rooms, value).
// ADDED(sims): the lot is on the top screen, so the panel keeps a scaled touch view of it for picking with
// the stylus; tapping the open sub-panel's button again enlarges the sub-panel over the touch view.
#pragma once
#include <cstdint>
#include <vector>
#include "app/ui/UIModePanel.h"
#include "app/live/UIMotiveDisplay.h"

namespace FSO { namespace SimAntics { class VMAvatar; } }

namespace simapp {

class UILiveMode : public UIModePanel {
public:
    enum class Tab { Mood = 0, Job = 1, Personality = 2, Relationships = 3, House = 4, Inventory = 5 }; // TS1(sims): Inventory (Hot Date+)
    explicit UILiveMode(LotScreen* owner);
    void Activate() override;
    void Draw(Canvas& c, const Rect& area) override;
    bool Tap(const std::string& id) override;
    void PointerUpdate() override;
    bool Buttons(const AppInput& in) override;
    std::string CursorArt() const override;

    Tab CurrentTab = Tab::Mood;
    bool Expanded = false;
    UIMotiveDisplay Motives;

private:
    std::vector<uint16_t> _QueueUIDs;
    int _RelScroll = 0;
    int _MotiveSim = -1;
    std::string _CursorArt = "Shared/cursors/LiveNothing.cur";
    int _HoverCheckX = -1, _HoverCheckY = -1;
    void DrawPortraits(Canvas& c, const Rect& r);
    void DrawSpeed(Canvas& c, const Rect& r);
    void DrawQueue(Canvas& c, const Rect& r, FSO::SimAntics::VMAvatar* sim);
    void DrawTabs(Canvas& c, const Rect& r);
    void DrawMood(Canvas& c, const Rect& r, FSO::SimAntics::VMAvatar* sim);
    void DrawJob(Canvas& c, const Rect& r, FSO::SimAntics::VMAvatar* sim);
    void DrawPersonality(Canvas& c, const Rect& r, FSO::SimAntics::VMAvatar* sim);
    void DrawRelationships(Canvas& c, const Rect& r, FSO::SimAntics::VMAvatar* sim);
    void DrawHouse(Canvas& c, const Rect& r);
    void DrawInventory(Canvas& c, const Rect& r, FSO::SimAntics::VMAvatar* sim);
    int _InvSort = 0, _InvScroll = 0;
    void DrawPips(Canvas& c, int x, int y, int value1000, int pipW, int pipH, uint32_t on, uint32_t off);
};

}
