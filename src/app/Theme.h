/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims app — UI colours, after The Sims 1 live-mode panel (dark blue panels, light blue text;
// FreeSims UIPieMenu ButtonStyle: text A5C3D6, selected 00FFFF) and the plumbob green.
#pragma once
#include "app/Canvas.h"

namespace simapp { namespace theme {

static const uint32_t ScreenBg     = RGBA(14, 24, 44);
static const uint32_t PanelTop     = RGBA(44, 72, 120);
static const uint32_t PanelBottom  = RGBA(22, 38, 70);
static const uint32_t PanelDark    = RGBA(12, 22, 44, 225);
static const uint32_t PanelEdge    = RGBA(110, 150, 200);
static const uint32_t Text         = RGBA(0xA5, 0xC3, 0xD6);
static const uint32_t TextBright   = RGBA(255, 255, 255);
static const uint32_t TextDim      = RGBA(110, 135, 165);
static const uint32_t TextYellow   = RGBA(255, 226, 120);
static const uint32_t Plumbob      = RGBA(80, 220, 60);
static const uint32_t PlumbobDark  = RGBA(30, 130, 30);
static const uint32_t ButtonBg     = RGBA(52, 86, 140);
static const uint32_t ButtonHi     = RGBA(90, 140, 205);
static const uint32_t ButtonOn     = RGBA(240, 200, 70);
static const uint32_t PieItem      = RGBA(28, 50, 92, 235);
static const uint32_t PieItemHi    = RGBA(70, 120, 190, 245);
static const uint32_t PieEdge      = RGBA(140, 185, 230);
static const uint32_t PieText      = RGBA(0xD0, 0xE4, 0xF2);
static const uint32_t PieTextHi    = RGBA(0x00, 0xFF, 0xFF);
static const uint32_t PieCentre    = RGBA(20, 36, 70, 240);
static const uint32_t BarBg        = RGBA(10, 18, 34);

/// Motive bar colour for a value -100..100 (TS1: green when satisfied, through yellow, to red).
inline uint32_t MotiveColor(int v) {
    if (v >= 0) { int t = v > 100 ? 100 : v; return RGBA(200 - t * 140 / 100, 200 + t * 20 / 100, 40); }
    int t = -v > 100 ? 100 : -v;
    return RGBA(220, 190 - t * 160 / 100, 40);
}

}}
