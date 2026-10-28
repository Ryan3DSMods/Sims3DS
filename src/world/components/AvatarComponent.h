/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.LotView.Components.AvatarComponent (World/components/AvatarComponent.cs)
// STUB(3ds): avatars are 3D (Vitaboy meshes drawn by _3DWorldBatch); the 2D software renderer does not draw
// them. This keeps the state the VM writes. GetSLOTPosition (hand bone) returns the avatar position.
#pragma once
#include <cstdint>
#include "world/components/EntityComponent.h"
#include "vitaboy/Appearance.h"

namespace FSO { namespace LotView { namespace Components {

enum class AvatarDisplayFlags : int { Normal = 0, ShowAsGhost = 1 };

class AvatarComponent : public EntityComponent {
public:
    FSO::Vitaboy::SimAvatar* Avatar = nullptr;
    float RadianDirection = 0;
    AvatarDisplayFlags DisplayFlags = AvatarDisplayFlags::Normal;
    bool IsPet = false;
    /// TS1(sims): person data CensorshipFlags (FreeSO VMCensorshipFlags: 1 pelvis, 2 spine if female, 4 head,
    /// 8/16 left/right hand, 32/64 left/right foot, 128 full body), set by the VM; World draws the TS1 mosaic.
    /// The VM clears bit 2 for male Sims.
    int32_t CensorFlags = 0;

    st::Vector3 DrawPosition() override {
        if (Container == nullptr) {
            // FreeSO AvatarComponent: AltitudeOff = InterpAltitude(_Position) (avatar positions are tile centres)
            if (AltitudeSource == nullptr) return Position;
            return st::Vector3(Position.X, Position.Y, Position.Z + AltitudeAt(Position, 0.0f));
        }
        return Container->GetSLOTPosition(ContainerSlot) + st::Vector3(0.5f, 0.5f, IsPet ? 0.0f : -1.4f);
    }
    // STUB(3ds): C# uses the R_FINGER0 bone of the animated skeleton.
    st::Vector3 GetSLOTPosition(int32_t slot) override { (void)slot; return Position; }
};

}}}
