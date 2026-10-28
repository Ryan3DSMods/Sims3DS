/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims SimsVille/SimsAntics/primitives/VMDrop.cs
#pragma once
#include <cstdint>
#include <array>
#include "simantics/engine/VMPrimitiveHandler.h"
#include "world/model/LotTilePos.h"

namespace FSO { namespace SimAntics { namespace Primitives {

class VMDrop : public Engine::VMPrimitiveHandler {
private:
    static const std::array<FSO::LotView::Model::LotTilePos, 8> Positions;
public:
    Engine::VMPrimitiveExitCode Execute(Engine::VMStackFrame* context, Engine::VMPrimitiveOperand* args) override;
};

class VMDropOperand : public Engine::VMPrimitiveOperand {
public:
    // region VMPrimitiveOperand Members
    void Read(const uint8_t* bytes) override;
    void Write(uint8_t* bytes) override;
    // endregion
};

}}}
