/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.LotView.WorldComponent (World/WorldComponent.cs)
#pragma once
#include <cstdint>
#include "core/math.h"
#include "world/model/Direction.h"

namespace FSO { namespace LotView {

class WorldState;

class WorldComponent {
public:
    virtual ~WorldComponent() {}
    /** Instance ID **/
    int64_t ID = 0;

    virtual float PreferredDrawOrder() { return 0.0f; }

    // STUB(3ds): C# takes a GraphicsDevice; the VM calls Initialize(nullptr, nullptr).
    virtual void Initialize(void* /*device*/, WorldState* world) { if (world != nullptr) OnWorldChanged(*world); }

    virtual void Draw(WorldState& world) { (void)world; }
    virtual void Update(WorldState& world) { (void)world; }

    virtual void OnRotationChanged(WorldState& world) { OnWorldChanged(world); }
    virtual void OnZoomChanged(WorldState& world) { OnWorldChanged(world); }
    virtual void OnScrollChanged(WorldState& world) { OnWorldChanged(world); }
    virtual void OnWorldChanged(WorldState& world) { (void)world; }
    virtual void OnPositionChanged() {}

    int16_t TileX = -2;
    int16_t TileY = -2;
    int8_t Level = -2;

    st::Vector3 TilePosition() const { return st::Vector3(TileX, TileY, Level); }
};

namespace Components { using FSO::LotView::WorldComponent; }

}}
