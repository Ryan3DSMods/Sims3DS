/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims app — FreeSims UI/Panels/WorldUI/UIHeadlineRenderer.cs: the balloons over Sims and objects
// (thought bubbles with the icon of what a Sim wants, speech balloons, motive/relationship icons, the red
// "route failed" cross...). The VM creates one renderer per VMRuntimeHeadline through the provider
// (VM::Headline); the sprites are the SPR# chunks of GameData/Sprites.iff exactly as in C#:
// group sprite = GroupOffsets[group] + index, balloon background = GroupOffsets[Balloon] + operand.Type,
// frame = ZoomFrame + animFrame * 3 (3 zoom levels per animation frame), cross = SPR# 67.
//
// ADDED(sims): C# rendered into a Texture2D that the world drew at the head bone (+0.66 tiles); there is no
// GPU here, so Draw() composes the same layers straight onto the top-screen canvas at a screen point the caller
// computes (LotScreen: head height above the entity).
#pragma once
#include <cstdint>
#include <functional>
#include "simantics/model/VMRuntimeHeadline.h"
#include "app/Canvas.h"

namespace FSO { namespace SimAntics { class VMEntity; } }

namespace simapp {

class UIHeadlineRenderer : public FSO::SimAntics::Model::VMHeadlineRenderer {
public:
    explicit UIHeadlineRenderer(FSO::SimAntics::Model::VMRuntimeHeadline* headline);
    void* DrawFrame(FSO::LotView::World* world) override { return nullptr; } // see Draw()
    void Dispose() override {}

    /// Composes the balloon centred at (cx, cy) on the canvas. zoom: 1 far, 2 medium, 3 near (WorldZoom).
    void Draw(Canvas& c, int cx, int cy, int zoom);
    /// Size of the balloon at a zoom (for placing it above the head).
    void Size(int zoom, int& w, int& h);
    FSO::SimAntics::Model::VMRuntimeHeadline* GetHeadline() { return Headline; }

    /// Icons for the algorithmic group (C#: Headline.IconTarget.GetIcon(gd, zoom)): the app's catalog icon of an
    /// object or a Sim's face. Set by the app.
    static std::function<const Canvas*(FSO::SimAntics::VMEntity*)> IconProvider;
    /// Loads GameData/Sprites.iff (C#: objectdata/globals/sprites.iff). Called once by the app.
    static void LoadSprites(const std::string& gameDataDir);
    static void Unload();

private:
    int SpriteID = -1, BGSpriteID = -1;
    static const int GroupOffsets[10];
    static const int ZoomToDiv[4];
};

class UIHeadlineRendererProvider : public FSO::SimAntics::Model::VMHeadlineRendererProvider {
public:
    FSO::SimAntics::Model::VMHeadlineRenderer* Get(FSO::SimAntics::Model::VMRuntimeHeadline* headline) override {
        return new UIHeadlineRenderer(headline);
    }
};

}
