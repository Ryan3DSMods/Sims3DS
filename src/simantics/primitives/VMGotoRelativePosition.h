/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims SimsVille/SimsAntics/primitives/VMGotoRelativePosition.cs
#pragma once
#include <cstdint>
#include "simantics/_files_contract.h"   // FSO_FLAGS_ENUM
#include "simantics/engine/VMPrimitiveHandler.h"

namespace FSO { namespace SimAntics { namespace Primitives {

enum class VMGotoRelativeDirection : int32_t
{
    Facing = -2,
    AnyDirection = -1,
    SameDirection = 0,
    FortyFiveDegreesRightOfSameDirection = 1,
    NinetyDegreesRightOfSameDirection = 2,
    FortyFiveDegreesLeftOfOpposingDirection = 3,
    OpposingDirection = 4,
    FortyFiveDegreesRightOfOpposingDirection = 5,
    NinetyDegreesRightOfOpposingDirection = 6,
    FortyFiveDegreesLeftOfSameDirection = 7
};

enum class VMGotoRelativeLocation : int32_t {
    OnTopOf = -2,
    AnywhereNear = -1,
    InFrontOf = 0,
    FrontAndToRightOf = 1,
    ToTheRightOf = 2,
    BehindAndToRightOf = 3,
    Behind = 4,
    BehindAndToTheLeftOf = 5,
    ToTheLeftOf = 6,
    InFrontAndToTheLeftOf = 7
};

// [Flags]
enum class VMGotoRelativeFlags : uint8_t
{
    AllowDiffAlt = 0x1,
    NoFailureTrees = 0x2
};
FSO_FLAGS_ENUM(VMGotoRelativeFlags)

class VMGotoRelativePosition : public Engine::VMPrimitiveHandler {
public:
    Engine::VMPrimitiveExitCode Execute(Engine::VMStackFrame* context, Engine::VMPrimitiveOperand* args) override;
};

class VMGotoRelativePositionOperand : public Engine::VMPrimitiveOperand {
public:
    /** How long to meander around objects **/
    uint16_t OldTrapCount = 0;
    VMGotoRelativeLocation Location = VMGotoRelativeLocation::InFrontOf;
    VMGotoRelativeDirection Direction = VMGotoRelativeDirection::SameDirection;
    uint16_t RouteCount = 0;
    VMGotoRelativeFlags Flags = (VMGotoRelativeFlags)0;

    bool NoFailureTrees() const
    {
        return (Flags & VMGotoRelativeFlags::NoFailureTrees) > 0;
    }
    void NoFailureTrees(bool value)
    {
        Flags = (VMGotoRelativeFlags)(Flags & ~VMGotoRelativeFlags::NoFailureTrees);
        if (value) Flags |= VMGotoRelativeFlags::NoFailureTrees;
    }

    bool AllowDiffAlt() const
    {
        return (Flags & VMGotoRelativeFlags::AllowDiffAlt) > 0;
    }
    void AllowDiffAlt(bool value)
    {
        Flags = (VMGotoRelativeFlags)(Flags & ~VMGotoRelativeFlags::AllowDiffAlt);
        if (value) Flags |= VMGotoRelativeFlags::AllowDiffAlt;
    }

    // region VMPrimitiveOperand Members
    void Read(const uint8_t* bytes) override;
    void Write(uint8_t* bytes) override;
    // endregion
};

}}}
