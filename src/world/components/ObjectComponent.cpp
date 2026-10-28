/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.LotView.Components.ObjectComponent (World/components/ObjectComponent.cs)
#include "world/components/ObjectComponent.h"
#include "world/model/Blueprint.h"
#include "world/WorldState.h"
#include "content/ContentManager.h"
#include "files/formats/iff/chunks/DGRP.h"
#include "files/formats/iff/chunks/OBJD.h"
#include "files/formats/iff/chunks/SLOT.h"
#include <cmath>

namespace FSO { namespace LotView { namespace Components {

using namespace FSO::LotView::Model;
using FSO::Files::Formats::IFF::Chunks::SLOT;

const st::Vector2 ObjectComponent::PosCenterOffsets[3] = {
    st::Vector2(2 + 16, 79 + 8),
    st::Vector2(3 + 32, 158 + 16),
    st::Vector2(5 + 64, 316 + 32)
};

// TS1(sims): EntityComponent::AltitudeAt (FreeSO EntityComponent / AvatarComponent position + InterpAltitude)
float EntityComponent::AltitudeAt(const st::Vector3& p, float centre) const {
    if (AltitudeSource == nullptr || p.X < 0 || p.Y < 0) return 0.0f;
    return AltitudeSource->InterpAltitude(st::Vector3(p.X + centre, p.Y + centre, 0));
}

ObjectComponent::ObjectComponent(FSO::Content::GameObject* obj) : Obj(obj), dgrp(nullptr) {
    if (obj != nullptr && obj->OBJ != nullptr) {
        if (obj->OBJ->BaseGraphicID > 0 && obj->Resource != nullptr) {
            auto gid = obj->OBJ->BaseGraphicID;
            this->DGRP = obj->Resource->Get<FSO::Files::Formats::IFF::Chunks::DGRP>(gid);
        }
        dgrp.DynamicSpriteBaseID = obj->OBJ->DynamicSpriteBaseId;
        dgrp.NumDynamicSprites = obj->OBJ->NumDynamicSprites;
    }
    dgrp.DGRP(this->DGRP);
}

float ObjectComponent::RadianDirection() const {
    switch (Direction) {
        case Model::Direction::NORTH: return 0;
        case Model::Direction::EAST: return (float)M_PI / 2;
        case Model::Direction::SOUTH: return (float)M_PI;
        case Model::Direction::WEST: return (float)M_PI * 1.5f;
        default: return 0;
    }
}

st::Vector3 ObjectComponent::GetSLOTPosition(int32_t slot) {
    auto item = (slot >= 0 && (int32_t)ContainerSlots.size() > slot) ? ContainerSlots[(size_t)slot] : nullptr;
    if (item != nullptr) {
        auto off = item->Offset;
        float h = (item->Height != 5 && item->Height >= 1 && item->Height <= 9) ? SLOT::HeightOffsets[item->Height - 1] : off.Z;
        st::Vector3 centerRelative(off.X * (1 / 16.0f), off.Y * (1 / 16.0f), h * (1 / 5.0f));
        float r = RadianDirection(), c = std::cos(r), s = std::sin(r);
        centerRelative = st::Vector3(centerRelative.X * c - centerRelative.Y * s, centerRelative.X * s + centerRelative.Y * c, centerRelative.Z);
        return this->DrawPosition() + centerRelative;
    } else return this->DrawPosition();
}

void ObjectComponent::CutawayHidden(bool value) {
    if (blueprint != nullptr && _CutawayHidden != value && renderInfo.Layer == WorldObjectRenderLayer::STATIC) {
        blueprint->Damage.push_back(BlueprintDamage(BlueprintDamageType::OBJECT_GRAPHIC_CHANGE, TileX, TileY, Level, this));
        DynamicCounter = 0;
    }
    _CutawayHidden = value;
}

void ObjectComponent::OnRotationChanged(WorldState& world) {
    EntityComponent::OnRotationChanged(world);
    dgrp.InvalidateRotation();
}

void ObjectComponent::OnZoomChanged(WorldState& world) {
    EntityComponent::OnZoomChanged(world);
    dgrp.InvalidateZoom();
}

void ObjectComponent::OnScrollChanged(WorldState& world) {
    EntityComponent::OnScrollChanged(world);
    dgrp.InvalidateScroll();
}

void ObjectComponent::SignalGraphicChange() {
    if (blueprint != nullptr) blueprint->Damage.push_back(BlueprintDamage(BlueprintDamageType::OBJECT_GRAPHIC_CHANGE, TileX, TileY, Level, this));
    DynamicCounter = 0;
}

// Replays the C# property setters for state the VM wrote directly.
void ObjectComponent::SyncRenderer() {
    bool first = !HasLast;
    if (first || LastDGRP != DGRP) {
        if (!first && dgrp.DGRP() != DGRP) SignalGraphicChange();
        dgrp.DGRP(DGRP);
        LastDGRP = DGRP;
    }
    if (first || LastFlags != DynamicSpriteFlags || LastFlags2 != DynamicSpriteFlags2) {
        dgrp.DynamicSpriteFlags = DynamicSpriteFlags;
        dgrp.DynamicSpriteFlags2 = DynamicSpriteFlags2;
        dgrp.InvalidateTextures();
        if (!first) SignalGraphicChange();
        LastFlags = DynamicSpriteFlags; LastFlags2 = DynamicSpriteFlags2;
    }
    if (first || LastDirection != Direction) {
        dgrp.Direction(Direction);
        dgrp.InvalidateRotation();
        if (!first) SignalGraphicChange(); // C#: a rotation also arrives as OBJECT_MOVE damage from the VM
        LastDirection = Direction;
    }
    if (first || LastRoom != Room) {
        dgrp.Room = Room;
        dgrp.InvalidateTextures(); // C# Room setter only stores it; sprites pick it up on the next rebuild
        if (!first) SignalGraphicChange();
        LastRoom = Room;
    }
    if (first || LastVisible != Visible) {
        if (!first) SignalGraphicChange();
        LastVisible = Visible;
    }
    auto pos = DrawPosition();
    if (first || LastPosition != pos) {
        if (!first) SignalGraphicChange(); // visual position changes (slots, animation) without a tile move
        LastPosition = pos;
    }
    HasLast = true;
}

void ObjectComponent::ValidateSprite(WorldState& world) {
    SyncRenderer();
    dgrp.ValidateSprite(world);
}

st::Vector2 ObjectComponent::GetScreenPos(WorldState& world) {
    return world.WorldSpace.GetScreenFromTile(DrawPosition()) + world.WorldSpace.GetScreenOffset() + PosCenterOffsets[(int)world.Zoom() - 1];
}

void ObjectComponent::Update(WorldState& world) {
    SyncRenderer();
    if (Headline != nullptr) {
        if (blueprint != nullptr && renderInfo.Layer == WorldObjectRenderLayer::STATIC) blueprint->Damage.push_back(BlueprintDamage(BlueprintDamageType::OBJECT_GRAPHIC_CHANGE, TileX, TileY, Level, this));
        DynamicCounter = 0; //keep windows and doors on the top floor on the dynamic layer.
    }

    if (HideForCutaway && Level > 0) {
        if (!(world.BuildMode() > 1) && world.DynamicCutaway && Level == world.Level()) {
            if (blueprint != nullptr && renderInfo.Layer == WorldObjectRenderLayer::STATIC) blueprint->Damage.push_back(BlueprintDamage(BlueprintDamageType::OBJECT_GRAPHIC_CHANGE, TileX, TileY, Level, this));
            DynamicCounter = 0; //keep windows and doors on the top floor on the dynamic layer.
        }

        if (Level != world.Level() || world.BuildMode() > 1) CutawayHidden(false);
        else if (blueprint != nullptr && blueprint->Walls != nullptr) {
            auto p = DrawPosition();
            st::Point tilePos((int32_t)std::round(p.X), (int32_t)std::round(p.Y));

            if (tilePos.X >= 0 && tilePos.X < blueprint->Width && tilePos.Y >= 0 && tilePos.Y < blueprint->Height) {
                auto& wall = (*blueprint->Walls)[(size_t)(Level - 1)][(size_t)(tilePos.Y * blueprint->Width + tilePos.X)];
                st::Point cutTest;
                // CutawayTests dictionary
                auto lookup = [](WallSegments seg, st::Point& out) {
                    switch ((int)seg) {
                        case (int)WallSegments::BottomLeft: out = st::Point(0, 1); return true;
                        case (int)WallSegments::TopLeft: out = st::Point(-1, 0); return true;
                        case (int)WallSegments::TopRight: out = st::Point(0, -1); return true;
                        case (int)WallSegments::BottomRight: out = st::Point(1, 0); return true;
                    }
                    return false;
                };
                if (!lookup(AdjacentWall & wall.Segments, cutTest)) {
                    lookup(wall.OccupiedWalls & wall.Segments, cutTest);
                }
                st::Point positions[2] = { tilePos, tilePos + cutTest };

                bool canContinue = true;
                for (auto& pos : positions) {
                    canContinue = canContinue && (pos.X >= 0 && pos.X < blueprint->Width && pos.Y >= 0 && pos.Y < blueprint->Height
                        && (size_t)(pos.Y * blueprint->Width + pos.X) < blueprint->Cutaway.size()
                        && blueprint->Cutaway[(size_t)(pos.Y * blueprint->Width + pos.X)]);
                    if (!canContinue) break;
                }
                CutawayHidden(canContinue);
            }
        }
    }

    bool forceDynamic = GetForceDynamic();
    auto cont = dynamic_cast<ObjectComponent*>(Container);
    if (cont != nullptr) {
        forceDynamic = cont->GetForceDynamic();
        if (forceDynamic && renderInfo.Layer == WorldObjectRenderLayer::STATIC && blueprint != nullptr) blueprint->Damage.push_back(BlueprintDamage(BlueprintDamageType::OBJECT_GRAPHIC_CHANGE, TileX, TileY, Level, this));
    }
    if (renderInfo.Layer == WorldObjectRenderLayer::DYNAMIC && !forceDynamic && DynamicCounter++ > 120 && blueprint != nullptr) {
        blueprint->Damage.push_back(BlueprintDamage(BlueprintDamageType::OBJECT_RETURN_TO_STATIC, TileX, TileY, Level, this));
    }
}

void ObjectComponent::Draw(WorldState& world) {
    auto pos = DrawPosition();
    if (!Visible || (!world.DrawOOB && (pos.X < -2043 && pos.Y < -2043))) return;
    if (CutawayHidden()) return;
    if (this->DGRP != nullptr) {
        SyncRenderer();
        dgrp.Draw(world);
    }
    // STUB(3ds): headline balloons (Texture2D rendered by VMRuntimeHeadline) are not drawn.
}

}}}
