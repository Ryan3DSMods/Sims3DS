/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Model.VMRuntimeHeadline / VMHeadlineRenderer / VMHeadlineRendererProvider / VMNullHeadlineProvider
// STUB(3ds): VMHeadlineRenderer.DrawFrame returned a Texture2D; here it returns nothing (headline timing is kept).
#pragma once
#include <cstdint>
#include <memory>
#include "simantics/engine/VMSetBalloonHeadlineOperand.h"
namespace FSO { namespace LotView { class World; } }
namespace FSO { namespace SimAntics {
class VMEntity;
namespace Model {

class VMRuntimeHeadline {
public:
    Primitives::VMSetBalloonHeadlineOperand Operand;
    VMEntity* Target;
    VMEntity* IconTarget;
    int8_t Index;
    int32_t Duration;
    int32_t Anim = 0;
    VMRuntimeHeadline(const Primitives::VMSetBalloonHeadlineOperand& op, VMEntity* targ, VMEntity* icon, int8_t index)
        : Operand(op), Target(targ), IconTarget(icon), Index(index) {
        Duration = (op.DurationInLoops() && op.Duration != -1) ? op.Duration * 15 : op.Duration;
    }
};

class VMHeadlineRenderer {
public:
    explicit VMHeadlineRenderer(VMRuntimeHeadline* headline) : Headline(headline) {}
    virtual ~VMHeadlineRenderer() {}
    virtual void* DrawFrame(FSO::LotView::World*) { return nullptr; } // STUB(3ds): Texture2D
    virtual void Dispose() {}
    /// Returns true if the headline should be killed.
    bool Update() {
        Headline->Anim++;
        if (Headline->Duration < 0) return false;
        return (--Headline->Duration <= 0);
    }
protected:
    VMRuntimeHeadline* Headline;
};

class VMHeadlineRendererProvider {
public:
    virtual ~VMHeadlineRendererProvider() {}
    virtual VMHeadlineRenderer* Get(VMRuntimeHeadline* headline) = 0;
};

class VMNullHeadlineProvider : public VMHeadlineRendererProvider {
public:
    VMHeadlineRenderer* Get(VMRuntimeHeadline* headline) override { return new VMHeadlineRenderer(headline); }
};
}}}
