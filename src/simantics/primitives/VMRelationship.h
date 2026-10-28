/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Primitives.VMRelationship / VMOldRelationshipOperand / VMRelationshipOperand (primitives/VMRelationship.cs)
#pragma once
#include <cstdint>
#include "simantics/engine/VMPrimitiveHandler.h"
#include "simantics/engine/VMPrimitiveOperand.h"
#include "simantics/engine/scopes/VMVariableScope.h"

namespace FSO { namespace SimAntics { namespace Primitives {

class VMRelationship : public Engine::VMPrimitiveHandler {
public:
    Engine::VMPrimitiveExitCode Execute(Engine::VMStackFrame* context, Engine::VMPrimitiveOperand* args) override;
};

class VMRelationshipOperand : public Engine::VMPrimitiveOperand {
public:
    uint8_t RelVar = 0;
    uint8_t Mode = 0;
    uint8_t Flags = 0;
    uint8_t Local = 0;
    Engine::Scopes::VMVariableScope VarScope = (Engine::Scopes::VMVariableScope)0;
    int16_t VarData = 0;

    // #region VMPrimitiveOperand Members
    void Read(const uint8_t* bytes) override;
    void Write(uint8_t* bytes) override;
    // #endregion

    virtual bool UseNeighbor() const
    {
        return (Flags & 8) == 8;
    }
    virtual void UseNeighbor(bool value)
    {
        if (value) Flags |= 8;
        else Flags &= (uint8_t)~8;
    }

    virtual bool FailIfTooSmall() const
    {
        return (Flags & 1) == 1;
    }
    virtual void FailIfTooSmall(bool value)
    {
        if (value) Flags |= 1;
        else Flags &= (uint8_t)~1;
    }

    virtual int32_t SetMode() const
    {
        return ((Flags >> 2) & 1) | ((Flags >> 4) & 2);
    }
    virtual void SetMode(int32_t value)
    {
        Flags &= (uint8_t)~36;
        Flags |= (uint8_t)((value & 1) << 1);
        Flags |= (uint8_t)((value & 2) << 4);
    }
};

class VMOldRelationshipOperand : public VMRelationshipOperand {
private:
    uint8_t GetSet = 0;
public:
    //clever tricks to avoid coding the same thing twice ;)
    void Read(const uint8_t* bytes) override;
    void Write(uint8_t* bytes) override;

    bool UseNeighbor() const override
    {
        return (Flags & 2) == 2;
    }
    void UseNeighbor(bool value) override
    {
        if (value) Flags |= 2;
        else Flags &= (uint8_t)~2;
    }

    bool FailIfTooSmall() const override
    {
        return (Flags & 1) == 1;
    }
    void FailIfTooSmall(bool value) override
    {
        if (value) Flags |= 1;
        else Flags &= (uint8_t)~1;
    }

    int32_t SetMode() const override
    {
        return GetSet;
    }
    void SetMode(int32_t value) override
    {
        GetSet = (uint8_t)value;
    }
};

}}}
