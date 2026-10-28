/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims — the original Create-A-Sim / Create-A-Family art and strings, prepared for the 3DS screens.
//
// Art: UIGraphics/UIGraphics.far (Res_Nbhd.RT: kDesignCharBkg "Nbhd/CreateACharBack.bmp", kDesignFamilyBkg
// "Nbhd/DsgnFamBkg.bmp", kDesignChar*Btn, kDesignCharPersLED, kDesignFamily Add/Delete/Edit/Done/Cancel,
// "Nbhd/CACPointRemainingBars.bmp"). The bitmaps are 8-bit with magenta (255,0,255) as the transparent colour;
// buttons are horizontal strips of 4 states (normal, highlighted, pressed, disabled; Done/Cancel have 6).
// Everything is decoded and resampled once (area average, premultiplied alpha) to the sizes the layout uses.
// UIGraphics.far is optional on the SD card: without it the screens draw plain panels and labelled buttons.
//
// Strings: GameData/UIText.iff STR# 129 'DesignFamilyStrs', 130 'DesignCharStrs', 152 'DefaultDialogButtons',
// 164 'signs', 128 'PickFamilyStrs'; GameData/Live.iff STR# 131 'Personality' (trait titles/descriptions).
#pragma once
#include <map>
#include <string>
#include <vector>
#include "app/Canvas.h"

namespace simapp { namespace cas {

/// A button bitmap: one canvas per state.
struct CASSprite {
    std::vector<Canvas> Frames;
    bool Empty() const { return Frames.empty(); }
    int W() const { return Frames.empty() ? 0 : Frames[0].Width; }
    int H() const { return Frames.empty() ? 0 : Frames[0].Height; }
};

class CASArt {
public:
    bool Load(const std::string& dataRoot);
    bool HaveArt = false;

    // backgrounds (already at screen size)
    Canvas TopBg, BottomBg;        // plain textured blue (the CAS backdrop without its panels)
    Canvas FamilyBottom;           // DsgnFamBkg at 0.4 (320x240): name bar, member panel, Add/Delete/Edit slots
    Canvas FamilyTop;              // DsgnFamBkg's member panel stretched to the top screen
    Canvas PreviewPanel;           // CreateACharBack's Sim panel (the lit gradient frame), top-screen size
    Canvas Cluster;                // CreateACharBack's quarter-circle button cluster background
    Canvas LEDOff, LEDOn;          // a personality row: 10 LEDs dark / lit
    Canvas PointsOff, PointsOn;    // "points remaining": 25 segments dark / lit

    // buttons (scaled)
    CASSprite Male, Female, Adult, Child, Light, Medium, Dark; // positioned inside Cluster (ClusterPos)
    CASSprite ArrowL, ArrowR, SimDone, SimCancel, FamDone, FamCancel, Add, Delete, Edit;
    /// Button positions inside the cluster canvas (pixels), in the order Male, Female, Adult, Child, Light, Medium, Dark.
    Rect ClusterPos[7];
    float ClusterScale = 0.5f;
    /// Add/Delete/Edit positions on FamilyBottom.
    Rect FamilyBtnPos[3];

    // strings
    std::string Str(int table, int index, const char* fallback) const;
    std::string Personality(int index, const char* fallback) const; // Live.iff STR#131

    /// Area-average resample (premultiplied alpha) of src's rectangle to w x h.
    static Canvas Resample(const Canvas& src, const Rect& from, int w, int h);

private:
    std::map<int, std::string> _Strings;   // table*1000+index -> UIText
    std::map<int, std::string> _Live131;
    void LoadStrings(const std::string& gameData);
};

}}
