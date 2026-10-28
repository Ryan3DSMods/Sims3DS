/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims SimsVille/SimsAntics/primitives/VMExpression.cs
// (C# namespace FSO.SimAntics.Engine.Primitives; placed in FSO::SimAntics::Primitives like the other primitives.)
#pragma once
#include <cstdint>
#include <string>
#include "simantics/engine/VMPrimitiveHandler.h"
#include "simantics/engine/scopes/VMVariableScope.h"

namespace FSO { namespace SimAntics { namespace Primitives {

enum class VMExpressionOperator : uint8_t {
    GreaterThan = 0,
    LessThan = 1,
    Equals = 2,
    PlusEquals = 3,
    MinusEquals = 4,
    Assign = 5,
    MulEquals = 6,
    DivEquals = 7,
    IsFlagSet = 8,
    SetFlag = 9,
    ClearFlag = 10,
    IncAndLessThan = 11,
    ModEquals = 12,
    AndEquals = 13,
    GreaterThanOrEqualTo = 14,
    LessThanOrEqualTo = 15,
    NotEqualTo = 16,
    DecAndGreaterThan = 17,
    Push = 18,
    Pop = 19,
    TS1AssignSqrtRHS = 20
};

class VMExpression : public Engine::VMPrimitiveHandler {
private:
    std::string OperatorToString(VMExpressionOperator op);
public:
    Engine::VMPrimitiveExitCode Execute(Engine::VMStackFrame* context, Engine::VMPrimitiveOperand* args) override;
    // C#: public override string ToString() { return base.ToString(); } (type name)
    std::string ToString() { return "FSO.SimAntics.Engine.Primitives.VMExpression"; }
};

class VMExpressionOperand : public Engine::VMPrimitiveOperand {
public:
    int16_t LhsData = 0;
    int16_t RhsData = 0;
    uint8_t IsSigned = 0;
    VMExpressionOperator Operator = VMExpressionOperator::GreaterThan;
    Engine::Scopes::VMVariableScope LhsOwner = (Engine::Scopes::VMVariableScope)0;
    Engine::Scopes::VMVariableScope RhsOwner = (Engine::Scopes::VMVariableScope)0;

    // region VMPrimitiveOperand Members
    void Read(const uint8_t* bytes) override;
    void Write(uint8_t* bytes) override;
    // endregion
};

}}}
