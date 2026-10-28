/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims SimsVille/SimsAntics/primitives/VMChangeSuitOrAccessory.cs
#pragma once
#include <cstdint>
#include "simantics/_files_contract.h"   // FSO_FLAGS_ENUM
#include "simantics/engine/VMPrimitiveHandler.h"
#include "simantics/engine/scopes/VMSuitScope.h"

namespace FSO { namespace SimAntics { namespace Primitives {

// [Flags]
enum class VMChangeSuitOrAccessoryFlags : int32_t
{
    Remove = 1,
    Update = 4
};
FSO_FLAGS_ENUM(VMChangeSuitOrAccessoryFlags)

class VMChangeSuitOrAccessory : public Engine::VMPrimitiveHandler {
public:
    Engine::VMPrimitiveExitCode Execute(Engine::VMStackFrame* context, Engine::VMPrimitiveOperand* args) override;
};

class VMChangeSuitOrAccessoryOperand : public Engine::VMPrimitiveOperand {
public:
    uint8_t SuitData = 0;
    Engine::Scopes::VMSuitScope SuitScope = Engine::Scopes::VMSuitScope::Global;
    VMChangeSuitOrAccessoryFlags Flags = (VMChangeSuitOrAccessoryFlags)0;

    // region VMPrimitiveOperand Members
    void Read(const uint8_t* bytes) override;
    void Write(uint8_t* bytes) override;
    // endregion
};

}}}
