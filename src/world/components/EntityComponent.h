/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.LotView.Components.EntityComponent (World/components/EntityComponent.cs)
// The VM writes Position/Direction/Room/Visible/Container directly (C# property setters), so they are plain
// fields here; the renderer detects changes itself (ObjectComponent::Update). C#'s Position *getter* (which
// follows the container slot) is DrawPosition().
#pragma once
#include <cstdint>
#include "core/math.h"
#include "world/components/WorldComponent.h"
#include "world/model/Direction.h"

namespace FSO { namespace LotView { namespace Model { class Blueprint; } } }

namespace FSO { namespace LotView { namespace Components {

class EntityComponent : public WorldComponent {
public:
    int16_t ObjectID = 0; //set this any time it changes so that hit test works.

    /// Position of the object in tile units (C# _Position; written by the VM).
    st::Vector3 Position;
    bool Visible = true;
    uint16_t Room = 0;
    EntityComponent* Container = nullptr;
    int32_t ContainerSlot = 0;
    void* Headline = nullptr; // STUB(3ds): Texture2D headline (VMRuntimeHeadline renderer is not ported)
    FSO::LotView::Model::Direction Direction = FSO::LotView::Model::Direction::NORTH;

    /// TS1(sims): the blueprint whose terrain altitude lifts this entity (FreeSO EntityComponent.Position
    /// adds blueprint.InterpAltitude). Set by Blueprint::AddObject / ChangeObjectLocation / AddAvatar.
    const FSO::LotView::Model::Blueprint* AltitudeSource = nullptr;
    /// FreeSO: altitude under the centre of the object's tile (the VM position is the tile corner).
    float AltitudeAt(const st::Vector3& p, float centre) const;

    /// C# Position getter: follows the container slot when contained.
    virtual st::Vector3 DrawPosition() {
        if (Container == nullptr) {
            if (AltitudeSource == nullptr) return Position;
            return st::Vector3(Position.X, Position.Y, Position.Z + AltitudeAt(Position, 0.5f));
        }
        else return Container->GetSLOTPosition(ContainerSlot);
    }

    virtual st::Vector3 GetSLOTPosition(int32_t slot) { (void)slot; return st::Vector3(0, 0, 0); }

    // STUB(3ds): the VM-facing overload; use GetScreenPos(WorldState&) in the renderer.
    st::Vector2 GetScreenPos(void* /*worldState*/) const { return st::Vector2(); }
};

}}}
