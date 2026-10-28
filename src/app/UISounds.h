/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims app — FreeSims UI/Model/UISounds.cs: the HIT event names of the UI sounds (SimsGeneratedHitSource.hot),
// and the one call the lot UI makes to play them. // FIX(sims) SND
//
// UI events are global HIT threads (no owner object): they are not in any VMEntity::SoundThreads, so they keep
// playing while the lot's own sounds are paused (game paused, buy/build mode: App::AudioFrame) — as in TS1.
//
// Bank facts (TS1 SimsGeneratedHitSource.hot): ui_object_place and ui_buy_place both play
// SoundData\UI\ui_object_place.xa (the cash register "ka-ching" of a purchase); ui_object_move_place is the
// softer thud of putting down an object that was already bought; ui_moneyback plays ui_object_moneyback.xa.
#pragma once

namespace simapp {
namespace UISounds {
    // Generic
    constexpr const char* Error = "ui_error";
    constexpr const char* Click = "ui_click";
    constexpr const char* Whoosh = "ui_whoosh";

    constexpr const char* QueueDelete = "ui_queue_delete";
    constexpr const char* QueueAdd = "ui_queued";

    constexpr const char* BuildDragToolPlace = "ui_bld_dragtool_place";
    constexpr const char* BuildDragToolDown = "ui_bld_dragtool_mousedown";
    constexpr const char* BuildDragToolUp = "ui_bld_dragtool_mouseup";

    constexpr const char* BuyPlace = "ui_buy_place"; //ca-ching!
    constexpr const char* MoneyBack = "ui_moneyback";
    constexpr const char* Remove = "ui_remove";

    constexpr const char* ObjectMoneyBack = "ui_object_moneyback";
    constexpr const char* ObjectMovePlace = "ui_object_move_place";
    constexpr const char* ObjectPlace = "ui_object_place";
    constexpr const char* ObjectRotate = "ui_object_rotate";

    constexpr const char* PieMenuAppear = "ui_piemenu_appear";
    constexpr const char* PieMenuHighlight = "ui_piemenu_highlight";
    constexpr const char* PieMenuSelect = "ui_piemenu_select";

    // Terrain (TS1 events; FreeSims/FreeSO's terrain tools use the drag tool sounds instead)
    constexpr const char* TerrainLevelMouseUp = "ui_terrain_level_mouseup";
    constexpr const char* TerrainLevelMouseDown = "ui_terrain_level_mousedown";
    constexpr const char* TerrainLower = "ui_terrain_lower";
    constexpr const char* TerrainRaise = "ui_terrain_raise";
}

/// HITVM.Get().PlaySoundEvent(evt) for a UI sound (no-op without SoundData).
void PlayUISound(const char* evt);
}
