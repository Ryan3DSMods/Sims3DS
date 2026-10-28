/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Primitives.VMLookTowards (primitives/VMLookTowards.cs)
#pragma once
#include <cstdint>
#include "simantics/_files_contract.h"
#include "simantics/engine/VMPrimitiveHandler.h"
#include "world/model/LotTilePos.h"

namespace FSO { namespace SimAntics { namespace Primitives {

// This primitive allows the sim to look at objects or other people eg. when talking to them. Not important right now
// but crucial for tv/eating conversations to make sense

enum class VMLookTowardsMode : uint8_t
{
    HeadTowardsObject = 0,
    BodyTowardsCamera = 1,
    BodyTowardsStackObj = 2,
    BodyAwayFromStackObj = 3,
    BodyTowardsAverageStackObj = 4,
    BodyAwayFromAverageStackObj = 5
};

class VMLookTowards : public Engine::VMPrimitiveHandler
{
public:
    Engine::VMPrimitiveExitCode Execute(Engine::VMStackFrame* context, Engine::VMPrimitiveOperand* args) override;

private:
    double GetDirectionTo(const FSO::LotView::Model::LotTilePos& pos1, const FSO::LotView::Model::LotTilePos& pos2);
    FSO::Files::Formats::IFF::Chunks::SLOTFlags RadianToFlags(double rad);
};

class VMLookTowardsOperand : public Engine::VMPrimitiveOperand
{
public:
    VMLookTowardsMode Mode = VMLookTowardsMode::HeadTowardsObject;

    void Read(const uint8_t* bytes) override;
    void Write(uint8_t* bytes) override;
};

}}}
