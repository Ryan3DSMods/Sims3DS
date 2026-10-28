/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims SimsVille/SimsAntics/primitives/VMAnimateSim.cs
// (C# namespace FSO.SimAntics.Engine.Primitives; placed in FSO::SimAntics::Primitives like the other primitives.)
// VMAnimateSimOperand is also the operand model of stackobj_notify_out_of_idle (VMNotifyOutOfIdle).
#pragma once
#include <cstdint>
#include <string>
#include "simantics/engine/VMPrimitiveHandler.h"
#include "simantics/engine/VMPrimitiveOperand.h"
#include "simantics/engine/scopes/VMAnimationScope.h"

namespace FSO { namespace SimAntics { namespace Primitives {

class VMAnimateSim : public Engine::VMPrimitiveHandler {
public:
    Engine::VMPrimitiveExitCode Execute(Engine::VMStackFrame* context, Engine::VMPrimitiveOperand* args) override;
};

class VMAnimateSimOperand : public Engine::VMPrimitiveOperand {
public:
    uint16_t AnimationID = 0;
    uint8_t LocalEventNumber = 0;
    uint8_t _pad = 0;
    Engine::Scopes::VMAnimationScope Source = (Engine::Scopes::VMAnimationScope)0;
    uint8_t Flags = 0;
    uint8_t ExpectedEventCount = 0;

    // VMPrimitiveOperand Members
    void Read(const uint8_t* bytes) override;
    void Write(uint8_t* bytes) override;

    bool StoreFrameInLocal() const { return (Flags & 32) == 32; }
    void StoreFrameInLocal(bool value) {
        if (value) Flags |= 32;
        else Flags &= (uint8_t)~32;
    }

    bool PlayBackwards() const { return (Flags & 2) == 2; }
    void PlayBackwards(bool value) {
        if (value) Flags |= 2;
        else Flags &= (uint8_t)~2;
    }

    //Mode 0: Play and Wait
    //Mode 1: ??
    //Mode 2: Stop standard carry, play and wait
    //Mode 3: ??
    uint8_t Mode() const { return (uint8_t)((Flags & 1) | ((Flags >> 3) & 2)); }
    // TS1(sims): FreeSO operand flags.
    bool Hurryable() const { return (Flags & 64) == 64; }
    bool IDFromParam() const { return (Flags & 4) == 4; }

    std::string ToString() const;
};

}}}
