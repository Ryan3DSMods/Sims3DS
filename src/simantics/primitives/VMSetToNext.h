/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Primitives.VMSetToNext (primitives/VMSetToNext.cs)
#pragma once
#include <cstdint>
#include <array>
#include "core/math.h"
#include "simantics/engine/VMPrimitiveHandler.h"
#include "simantics/engine/scopes/VMVariableScope.h"

namespace FSO { namespace SimAntics {
class VMEntity;
namespace Primitives {

enum class VMSetToNextSearchType : int32_t
{
    Object = 0,
    Person = 1,
    NonPerson = 2,
    PartOfAMultipartTile = 3,
    ObjectOfType = 4,
    NeighborId = 5,
    ObjectWithCategoryEqualToSP0 = 6,
    NeighborOfType = 7,
    ObjectOnSameTile = 8,
    ObjectAdjacentToObjectInLocal = 9,
    Career = 10,
    ClosestHouse = 11,
    FamilyMember = 12
};

class VMSetToNext : public Engine::VMPrimitiveHandler
{
public:
    Engine::VMPrimitiveExitCode Execute(Engine::VMStackFrame* context, Engine::VMPrimitiveOperand* args) override;

private:
    //position steps for object adjacent to object in local
    static const std::array<st::Point, 4> AdjStep;

    int getAdjDir(VMEntity* src, VMEntity* dest);
    int getAdjDir(int diffX, int diffY);
};

class VMSetToNextOperand : public Engine::VMPrimitiveOperand
{
public:
    uint32_t GUID = 0;
    uint8_t Flags = 0;
    Engine::Scopes::VMVariableScope TargetOwner = (Engine::Scopes::VMVariableScope)0;
    uint8_t Local = 0;
    uint8_t TargetData = 0;

    VMSetToNextSearchType SearchType() const { return (VMSetToNextSearchType)(Flags & 0x7F); }
    void SearchType(VMSetToNextSearchType value) { Flags = (uint8_t)(0x80 | ((uint8_t)value & 0x7F)); }

    void Read(const uint8_t* bytes) override;
    void Write(uint8_t* bytes) override;
};

}}}
