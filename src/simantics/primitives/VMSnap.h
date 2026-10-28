/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Primitives.VMSnap / VMSnapOperand / VMSnapSlotScope (primitives/VMSnap.cs)
#pragma once
#include <cstdint>
#include "simantics/engine/VMPrimitiveHandler.h"
#include "simantics/engine/VMPrimitiveOperand.h"
#include "world/model/LotTilePos.h"
#include "world/model/Direction.h"

namespace FSO { namespace SimAntics {
class VMEntity;
class VMContext;
namespace Primitives {

enum class VMSnapSlotScope : int32_t
{
    StackVariable = 0,
    BeContained = 1,
    InFront = 2,
    Literal = 3,
    Global = 4
};

class VMSnap : public Engine::VMPrimitiveHandler {
public:
    Engine::VMPrimitiveExitCode Execute(Engine::VMStackFrame* context, Engine::VMPrimitiveOperand* args) override;

private:
    bool SetPosition(VMEntity* entity, const FSO::LotView::Model::LotTilePos& pos, FSO::LotView::Model::Direction dir, VMContext* context);
    bool SetPosition(VMEntity* entity, const FSO::LotView::Model::LotTilePos& pos, float radDir, VMContext* context);
};

class VMSnapOperand : public Engine::VMPrimitiveOperand {
public:
    uint16_t Index = 0;
    VMSnapSlotScope Mode = VMSnapSlotScope::StackVariable;
    uint8_t Flags = 0;

    // #region VMPrimitiveOperand Members
    void Read(const uint8_t* bytes) override;
    void Write(uint8_t* bytes) override;
    // #endregion
};

}}}
