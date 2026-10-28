/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.LotView.Components.ObjectComponent (World/components/ObjectComponent.cs)
//
// The C# properties DGRP / DynamicSpriteFlags(2) / Direction / Visible / Room push blueprint damage and
// invalidate the DGRPRenderer from their setters. The translated VM assigns the plain fields below instead,
// so Update() compares them with what the renderer last used and raises the same damage
// (OBJECT_GRAPHIC_CHANGE -> object moves to the dynamic layer) when something changed.
#pragma once
#include <cstdint>
#include <vector>
#include "core/math.h"
#include "world/components/EntityComponent.h"
#include "world/model/WallTile.h"
#include "world/utils/DGRPRenderer.h"
#include "simantics/_files_contract.h"

namespace FSO { namespace Content { class GameObject; } }
namespace FSO { namespace LotView { namespace Model { class Blueprint; } } }

namespace FSO { namespace LotView {

enum class WorldObjectRenderLayer { STATIC, DYNAMIC };

class WorldObjectRenderInfo {
public:
    WorldObjectRenderLayer Layer = WorldObjectRenderLayer::STATIC;
};

namespace Components {

class ObjectComponent : public EntityComponent {
public:
    explicit ObjectComponent(FSO::Content::GameObject* obj);

    FSO::Content::GameObject* Obj;

    /// The DGRP currently shown (written by VMGameObject::RefreshGraphic).
    FSO::Files::Formats::IFF::Chunks::DGRP* DGRP = nullptr;
    WorldObjectRenderInfo renderInfo;
    FSO::LotView::Model::Blueprint* blueprint = nullptr;
    int32_t DynamicCounter = 0; //how long this sprite has been dynamic without changing sprite
    std::vector<FSO::Files::Formats::IFF::Chunks::SLOTItem*> ContainerSlots;

    bool HideForCutaway = false;
    bool ForceDynamic = false; // C# property also ORs Headline != null, see GetForceDynamic()
    FSO::LotView::Model::WallSegments AdjacentWall = (FSO::LotView::Model::WallSegments)0;

    uint64_t DynamicSpriteFlags = 0;
    uint64_t DynamicSpriteFlags2 = 0;

    bool GetForceDynamic() const { return ForceDynamic || Headline != nullptr; }

    st::Rectangle Bounding() const { return dgrp.Bounding; }

    bool CutawayHidden() const { return _CutawayHidden; }
    void CutawayHidden(bool value);

    float PreferredDrawOrder() override { return 2000.0f + (this->Position.X + this->Position.Y); }

    st::Vector3 GetSLOTPosition(int32_t slot) override;

    void OnRotationChanged(WorldState& world) override;
    void OnZoomChanged(WorldState& world) override;
    void OnScrollChanged(WorldState& world) override;

    void ValidateSprite(WorldState& world);
    st::Vector2 GetScreenPos(WorldState& world);

    void Update(WorldState& world) override;
    void Draw(WorldState& world) override;

    static const st::Vector2 PosCenterOffsets[3];

private:
    Utils::DGRPRenderer dgrp;
    bool _CutawayHidden = false;

    // what the DGRPRenderer was last built from (stands in for the C# property setters)
    FSO::Files::Formats::IFF::Chunks::DGRP* LastDGRP = nullptr;
    uint64_t LastFlags = 0, LastFlags2 = 0;
    FSO::LotView::Model::Direction LastDirection = FSO::LotView::Model::Direction::NORTH;
    bool LastVisible = true;
    uint16_t LastRoom = 0;
    st::Vector3 LastPosition;
    bool HasLast = false;

    void SyncRenderer();
    void SignalGraphicChange();
    float RadianDirection() const;
};

}}}
