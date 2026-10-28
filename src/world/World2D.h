/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.LotView.World2D (World/World2D.cs) — handles rendering the 2D world.
//
// Same layering as FreeSims: three static layers (terrain+floors, walls+roof, static objects) are rendered into
// off-screen colour+depth buffers ("scroll buffers", viewport + SCROLL_BUFFER pixels, re-rendered only when
// damaged) and dynamic objects (moved/animated in the last 120 frames) are drawn on top through the z-buffer every
// frame.
// FSO_CPP (3DS performance):
//  * The depth in the layers is absolute world depth, so scrolling shifts the cached pixels and only the strip that
//    scrolled into view is rendered (clipped), instead of re-rendering every layer when the view leaves the buffer
//    (C# re-rendered all layers every SCROLL_BUFFER pixels). The buffer origin moves in steps of SCROLL_BUFFER/4.
//  * The three layers are merged (depth tested, as drawSimpleRestoreDepth) into one Composite buffer whenever one of
//    them changes; a frame starts with a plain row copy of the composite (colour + depth) instead of clearing the
//    target and depth-testing three full-screen layers.
//  * The C# sprite-list caches (StaticWallCache/...) are not kept: a redraw re-runs the component.
#pragma once
#include <cstdint>
#include <vector>
#include <unordered_map>
#include "core/math.h"
#include "world/Framebuffer.h"

namespace FSO { namespace LotView {

class WorldState;
namespace Model { class Blueprint; }
class WorldComponent;
namespace Components { class ObjectComponent; }

class ScrollBuffer {
public:
    Framebuffer Buffer;    // C# Pixel + Depth textures
    st::Vector2 PxOffset;
    st::Vector3 WorldPosition;
    bool Valid = false;
};

class World2D {
public:
    static const int32_t NUM_2D_BUFFERS = 12;
    /// resolution to add to render size for scroll reasons (C# 512; smaller saves memory on the 3DS)
    int32_t SCROLL_BUFFER = 512;
    /// colour of pixels nothing is drawn on (World.ClearColor)
    uint32_t ClearColor = Framebuffer::Pack(0, 0, 0, 255);

    void Init(FSO::LotView::Model::Blueprint* blueprint) { this->Blueprint = blueprint; Invalidate(); }

    /// Prep work before screen is painted
    void PreDraw(WorldState& state);
    /// Paint to screen. Fills the whole target (composite copy) and draws the dynamic objects.
    void Draw(WorldState& state, Framebuffer& target);

    st::Vector2 GetScrollIncrement(const st::Vector2& pxOffset, WorldState& state);
    /// Forget every cached layer (next PreDraw renders everything).
    void Invalidate();

    /// Statistics for tools.
    int32_t StaticRedraws = 0;   // full layer renders
    int32_t StripRedraws = 0;    // partial (scrolled-in strip) layer renders
    int32_t DirtyRedraws = 0;    // partial object layer renders (an object changed layer / was removed)
    int64_t StripPixels = 0;     // pixels of those partial renders

private:
    FSO::LotView::Model::Blueprint* Blueprint = nullptr;
    ScrollBuffer StaticObjects;
    ScrollBuffer StaticFloor;
    ScrollBuffer StaticWall;
    Framebuffer Composite;
    bool CompositeValid = false;
    st::Vector2 Origin;          // world pixel of buffer pixel (0,0), shared by all layers + composite
    bool HaveOrigin = false;
    int32_t TicksSinceLight = 0;
    bool LastDrawRoofs = false;
    std::vector<uint32_t> LastRoomColors;
    /// world-pixel bounds of every object painted into the static object layer (keys may be deleted components:
    /// never dereferenced, only looked up)
    std::unordered_map<const WorldComponent*, st::Rectangle> StaticBounds;
    static st::Rectangle ObjectScreenBounds(Components::ObjectComponent* obj, WorldState& state);
    void TakeStaticBounds(WorldComponent* comp, std::vector<st::Rectangle>& dirty, bool& fallback);

    enum Layer { FLOOR = 0, WALL = 1, OBJECTS = 2 };
    ScrollBuffer& LayerBuffer(int layer) { return layer == FLOOR ? StaticFloor : (layer == WALL ? StaticWall : StaticObjects); }
    /// Renders a layer into its buffer; clip == nullptr: whole buffer (cleared first), else only that rect.
    void RenderLayer(int layer, WorldState& state, const st::Rectangle* clip);
    /// Merges the layers into the composite inside rect (buffer coordinates).
    void BuildComposite(const st::Rectangle& rect);
    static void ShiftBuffer(Framebuffer& fb, int32_t sx, int32_t sy);
    void MergeLayer(const Framebuffer& src, Framebuffer& dst, const st::Rectangle& rect) const;
    const st::Color* AmbientLight = nullptr; // Blueprint.RoomColors at composite time
    st::Color OutsideLight;
    uint32_t LastOutside = 0;
    /// FSO_CPP: the cut the wall layer was last rendered with (Blueprint.Cutaway), to redraw only the walls whose cut
    /// changed (the cursor cutaway changes a few tiles at a time; C# re-rendered the whole layer).
    std::vector<bool> LastCut;
    int32_t LastCutLevel = -1;
    bool CutDirtyRect(WorldState& state, st::Rectangle& out);
    st::Vector2 GetBufferOrigin(const st::Vector2& pxOffset) const;
};

}}
