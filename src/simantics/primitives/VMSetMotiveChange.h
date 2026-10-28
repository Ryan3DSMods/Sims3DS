/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Primitives.VMSetMotiveChange (primitives/VMSetMotiveChange.cs)
#pragma once
#include <cstdint>
#include "simantics/_files_contract.h"
#include "simantics/engine/VMPrimitiveHandler.h"
#include "simantics/engine/scopes/VMVariableScope.h"
#include "simantics/model/VMMotive.h"

namespace FSO { namespace SimAntics { namespace Primitives {

enum class VMSetMotiveChangeFlags : int32_t {
    ClearAll = 1,
    Once = 2,
};
FSO_FLAGS_ENUM(VMSetMotiveChangeFlags)

class VMSetMotiveChange : public Engine::VMPrimitiveHandler
{
public:
    Engine::VMPrimitiveExitCode Execute(Engine::VMStackFrame* context, Engine::VMPrimitiveOperand* args) override;
};

class VMSetMotiveChangeOperand : public Engine::VMPrimitiveOperand {
public:
    Engine::Scopes::VMVariableScope DeltaOwner = (Engine::Scopes::VMVariableScope)0;
    int16_t DeltaData = 0;

    Engine::Scopes::VMVariableScope MaxOwner = (Engine::Scopes::VMVariableScope)0;
    int16_t MaxData = 0;

    VMSetMotiveChangeFlags Flags = (VMSetMotiveChangeFlags)0;
    Model::VMMotive Motive = (Model::VMMotive)0;

    bool ClearAll() const
    {
        return (Flags & VMSetMotiveChangeFlags::ClearAll) > 0;
    }
    void ClearAll(bool value)
    {
        if (value) Flags |= VMSetMotiveChangeFlags::ClearAll;
        else Flags &= ~VMSetMotiveChangeFlags::ClearAll;
    }

    bool Once() const
    {
        return (Flags & VMSetMotiveChangeFlags::Once) > 0;
    }
    void Once(bool value)
    {
        if (value) Flags |= VMSetMotiveChangeFlags::Once;
        else Flags &= ~VMSetMotiveChangeFlags::Once;
    }

    void Read(const uint8_t* bytes) override;
    void Write(uint8_t* bytes) override;
};

}}}
