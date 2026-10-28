/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.LotView.World (World/World.cs) — represents world (I.E lots in the game.)
// Software 2D renderer: World::Draw(Framebuffer&) paints the lot into any RGBA framebuffer.
// Avatars (World3D/_3DWorldBatch in C#) are drawn with src/vitaboy/AvatarRenderer after the 2D pass, depth tested
// against the world. STUB(3ds): roofs, sub-worlds, mouse/edge scrolling.
#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include "world/WorldState.h"
#include "world/World2D.h"
#include "world/Framebuffer.h"
#include "world/model/Blueprint.h"
#include "world/utils/_2DWorldBatch.h"

namespace FSO { namespace Vitaboy { class WorldCamera; } namespace LotView { namespace Components { class AvatarComponent; } } }

namespace FSO { namespace LotView {

class World {
public:
    /// Creates a new World instance with a view of width x height pixels.
    explicit World(int32_t width = 400, int32_t height = 240);
    ~World();

    /** How many pixels from each edge of the screen before we start scrolling the view **/
    int32_t ScrollBounds = 20;
    /// TS1(sims): keep the view centre on the lot (see PreDraw); false = FreeSims' looser TSO bounds.
    bool TS1ScrollBounds = true;
    /// ADDED(sims): no scroll bounds at all while set (front/LotSave draws the house pictures in passes whose view
    /// centres lie off the lot)
    bool FreeCenter = false;
    uint32_t FrameCounter = 0;
    float Opacity = 1.0f;

    WorldState State;
    /// Background colour for pixels nothing is drawn on (RGBA packed, see Framebuffer).
    uint32_t ClearColor = Framebuffer::Pack(0, 0, 0, 255);
    /// Optional: draws avatars (3D in FreeSims) into the framebuffer after the 2D world, with the world depth.
    std::function<void(World&, Framebuffer&)> AvatarDrawHook;
    /// Draw Blueprint.Avatars with the Vitaboy software renderer after the 2D pass.
    bool DrawAvatars = true;
    /// TS1 "Shadows" option: soft round shadow on the floor under each Sim.
    bool DrawAvatarShadows = true;

    int8_t Stories() const { return Blueprint ? Blueprint->Stories : (int8_t)1; }
    FSO::LotView::Model::Blueprint* GetBlueprint() { return Blueprint; }

    virtual void InitBlueprint(FSO::LotView::Model::Blueprint* blueprint);

    void InvalidateZoom();
    void InvalidatePreciseZoom();
    void InvalidateRotation();
    void InvalidateScroll();
    void InvalidateFloor();

    void Scroll(const st::Vector2& dir);
    void GetScrollBasis(st::Vector2 output[2]);

    void Update();
    /// Pre-Draw: bounds the scroll, updates objects, re-renders damaged static layers.
    void PreDraw();
    /// Paints the world into target (resizing the view to the framebuffer's size if needed).
    void Draw(Framebuffer& target);

    /// Static layer re-render scroll margin (pixels, see World2D::SCROLL_BUFFER).
    void SetScrollBuffer(int32_t px) { _2DWorld.SCROLL_BUFFER = px; }
    World2D& Get2DWorld() { return _2DWorld; }
    Utils::_2DWorldBatch& Batch() { return _2DBatch; }

protected:
    bool HasInitGPU = true;
    bool HasInitBlueprint = false;
    bool HasInit = false;

    FSO::LotView::Model::Blueprint* Blueprint = nullptr;

private:
    World2D _2DWorld;
    Utils::_2DWorldBatch _2DBatch;

    void InternalDraw(Framebuffer& target);
    void DrawAvatarComponents(Framebuffer& target);
    void DrawAvatarShadow(Framebuffer& target, const st::Vector3& tilePos, float radius);
    void DrawCensor(Framebuffer& target, Components::AvatarComponent* ava, const FSO::Vitaboy::WorldCamera& cam, int minX, int minY, int maxX, int maxY);
};

}}
