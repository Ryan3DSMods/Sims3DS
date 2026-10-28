/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims SimsVille/SimsAntics/primitives/VMFindLocationFor.cs
#pragma once
#include <cstdint>
#include <array>
#include "simantics/engine/VMPrimitiveHandler.h"
#include "world/model/LotTilePos.h"

namespace FSO { namespace SimAntics {
class VMEntity;
class VMContext;
namespace Primitives {

class VMFindLocationFor : public Engine::VMPrimitiveHandler {
private:
    static const std::array<FSO::LotView::Model::LotTilePos, 8> DirectionVectors;

public:
    Engine::VMPrimitiveExitCode Execute(Engine::VMStackFrame* context, Engine::VMPrimitiveOperand* args) override;

    static bool FindLocationVector(VMEntity* obj, VMEntity* refObj, VMContext* context, int32_t dir);
    static bool FindLocationFor(VMEntity* obj, VMEntity* refObj, VMContext* context);
};

class VMFindLocationForOperand : public Engine::VMPrimitiveOperand {
public:
    uint8_t Mode = 0;
    uint8_t Local = 0;
    uint8_t Flags = 0;

    // region VMPrimitiveOperand Members
    void Read(const uint8_t* bytes) override;
    void Write(uint8_t* bytes) override;
    // endregion

    bool UseLocalAsRef() const
    {
        return (Flags & 1) == 1;
    }
    void UseLocalAsRef(bool value)
    {
        if (value) Flags |= 1;
        else Flags &= (uint8_t)~1;
    }

    bool AllowIntersection() const
    {
        return (Flags & 2) == 2;
    }
    void AllowIntersection(bool value)
    {
        if (value) Flags |= 2;
        else Flags &= (uint8_t)~2;
    }

    bool UserEditableTilesOnly() const
    {
        return (Flags & 4) == 4;
    }
    void UserEditableTilesOnly(bool value)
    {
        if (value) Flags |= 4;
        else Flags &= (uint8_t)~4;
    }
};

}}}
