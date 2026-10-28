/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims SimsVille/SimsAntics/primitives/VMExpression.cs
#include "simantics/primitives/VMExpression.h"
#include <cmath>
#include <cstring>
#include <list>
#include <algorithm>
#include "core/io.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMThread.h"
#include "simantics/engine/VMMemory.h"
#include "simantics/engine/VMSimanticsException.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/model/VMStackObjectVariable.h"

namespace FSO { namespace SimAntics { namespace Primitives {

using namespace FSO::SimAntics::Engine;
using FSO::SimAntics::Engine::Utils::VMMemory;
using FSO::SimAntics::Engine::Scopes::VMVariableScope;
using FSO::SimAntics::Model::VMStackObjectVariable;

// C# int arithmetic is unchecked (wraps); C++ signed overflow is UB, so wrap through uint32.
static inline int32_t WrapAdd(int32_t a, int32_t b) { return (int32_t)((uint32_t)a + (uint32_t)b); }
static inline int32_t WrapSub(int32_t a, int32_t b) { return (int32_t)((uint32_t)a - (uint32_t)b); }
static inline int32_t WrapMul(int32_t a, int32_t b) { return (int32_t)((uint32_t)a * (uint32_t)b); }
// C# `1 << n` on int masks the shift count to 5 bits.
static inline int32_t Bit(int32_t n) { return (int32_t)(1u << (n & 31)); }

std::string VMExpression::OperatorToString(VMExpressionOperator op)
{
    switch (op) {
        case VMExpressionOperator::AndEquals:
            return "&=";
        case VMExpressionOperator::Assign:
            return "=";
        case VMExpressionOperator::ClearFlag:
            return "clearFlag";
        case VMExpressionOperator::DivEquals:
            return "/=";
        case VMExpressionOperator::Equals:
            return "==";
        case VMExpressionOperator::GreaterThan:
            return ">";
        case VMExpressionOperator::GreaterThanOrEqualTo:
            return ">=";
        case VMExpressionOperator::IncAndLessThan:
            return "++ & <";
        case VMExpressionOperator::IsFlagSet:
            return "flagSet";
        case VMExpressionOperator::LessThan:
            return "<";
        case VMExpressionOperator::LessThanOrEqualTo:
            return "<=";
        case VMExpressionOperator::DecAndGreaterThan:
            return "-- & >";
        case VMExpressionOperator::MinusEquals:
            return "-=";
        case VMExpressionOperator::ModEquals:
            return "%=";
        case VMExpressionOperator::MulEquals:
            return "*=";
        case VMExpressionOperator::NotEqualTo:
            return "!=";
        case VMExpressionOperator::PlusEquals:
            return "+=";
        case VMExpressionOperator::Pop:
            return "pop";
        case VMExpressionOperator::Push:
            return "push";
        case VMExpressionOperator::SetFlag:
            return "setFlag";
        default:
            break;
    }
    return "unknown";
}

VMPrimitiveExitCode VMExpression::Execute(VMStackFrame* context, VMPrimitiveOperand* args)
{
    auto operand = static_cast<VMExpressionOperand*>(args);

    int32_t rhsValue = 0;
    int32_t lhsValue = 0;
    bool setResult = false;

    switch (operand->Operator) {
        /** Modifiers **/
        case VMExpressionOperator::Assign:
            rhsValue = VMMemory::GetBigVariable(context, operand->RhsOwner, operand->RhsData);
            setResult = VMMemory::SetBigVariable(context, operand->LhsOwner, operand->LhsData, rhsValue);

            if (setResult)
            {
                return VMPrimitiveExitCode::GOTO_TRUE;
            } else {
                return VMPrimitiveExitCode::GOTO_FALSE;
            }

        /** ++ and < **/
        case VMExpressionOperator::IncAndLessThan:
            lhsValue = VMMemory::GetBigVariable(context, operand->LhsOwner, operand->LhsData);
            lhsValue = WrapAdd(lhsValue, 1);
            VMMemory::SetBigVariable(context, operand->LhsOwner, operand->LhsData, lhsValue);
            rhsValue = VMMemory::GetBigVariable(context, operand->RhsOwner, operand->RhsData);

            if (lhsValue < rhsValue) {
                return VMPrimitiveExitCode::GOTO_TRUE;
            } else {
                return VMPrimitiveExitCode::GOTO_FALSE;
            }

        /** -- and > **/
        case VMExpressionOperator::DecAndGreaterThan:
            lhsValue = VMMemory::GetBigVariable(context, operand->LhsOwner, operand->LhsData);
            lhsValue = WrapSub(lhsValue, 1);
            VMMemory::SetBigVariable(context, operand->LhsOwner, operand->LhsData, lhsValue);
            rhsValue = VMMemory::GetBigVariable(context, operand->RhsOwner, operand->RhsData);

            if (lhsValue > rhsValue)
            {
                return VMPrimitiveExitCode::GOTO_TRUE;
            }
            else
            {
                return VMPrimitiveExitCode::GOTO_FALSE;
            }

        case VMExpressionOperator::SetFlag: {
            lhsValue = VMMemory::GetBigVariable(context, operand->LhsOwner, operand->LhsData);
            rhsValue = VMMemory::GetBigVariable(context, operand->RhsOwner, operand->RhsData);
            auto bitval = Bit(rhsValue - 1);
            lhsValue |= bitval;
            if (VMMemory::SetBigVariable(context, operand->LhsOwner, operand->LhsData, lhsValue))
            {
                return VMPrimitiveExitCode::GOTO_TRUE;
            } else {
                return VMPrimitiveExitCode::GOTO_FALSE;
            }
        }

        case VMExpressionOperator::ClearFlag: {
            lhsValue = VMMemory::GetBigVariable(context, operand->LhsOwner, operand->LhsData);
            rhsValue = VMMemory::GetBigVariable(context, operand->RhsOwner, operand->RhsData);
            auto clearBitval = ~Bit(rhsValue - 1);
            lhsValue &= (int16_t)clearBitval;
            if (VMMemory::SetBigVariable(context, operand->LhsOwner, operand->LhsData, lhsValue))
            {
                return VMPrimitiveExitCode::GOTO_TRUE;
            } else {
                return VMPrimitiveExitCode::GOTO_FALSE;
            }
        }

        /** %= **/
        case VMExpressionOperator::PlusEquals:
        case VMExpressionOperator::ModEquals:
        case VMExpressionOperator::MinusEquals:
        case VMExpressionOperator::DivEquals:
        case VMExpressionOperator::MulEquals:
        case VMExpressionOperator::AndEquals:
            lhsValue = VMMemory::GetBigVariable(context, operand->LhsOwner, operand->LhsData);
            rhsValue = VMMemory::GetBigVariable(context, operand->RhsOwner, operand->RhsData);
            switch (operand->Operator)
            {
                case VMExpressionOperator::ModEquals:
                    if (rhsValue == 0) break;
                    if (rhsValue == -1) { lhsValue = 0; break; } // C++: INT_MIN % -1 traps; result is always 0 anyway
                    lhsValue = (int32_t)(((int64_t)(lhsValue % rhsValue) + rhsValue) % rhsValue);
                    break;
                case VMExpressionOperator::PlusEquals:
                    lhsValue = WrapAdd(lhsValue, rhsValue);
                    break;
                case VMExpressionOperator::MinusEquals:
                    lhsValue = WrapSub(lhsValue, rhsValue);
                    break;
                case VMExpressionOperator::DivEquals:
                    if (rhsValue == 0)
                    {
                        lhsValue = -1;
                        break;
                    }
                    // C# throws OverflowException for int.MinValue / -1; C++ would trap.
                    if (rhsValue == -1 && lhsValue == INT32_MIN)
                        throw VMSimanticsException("Arithmetic operation resulted in an overflow.", context);
                    lhsValue /= rhsValue;
                    break;
                case VMExpressionOperator::MulEquals:
                    lhsValue = WrapMul(lhsValue, rhsValue);
                    break;
                case VMExpressionOperator::AndEquals:
                    lhsValue &= rhsValue;
                    break;
                default:
                    break;
            }
            VMMemory::SetBigVariable(context, operand->LhsOwner, operand->LhsData, lhsValue);
            return VMPrimitiveExitCode::GOTO_TRUE;

        /** == **/
        case VMExpressionOperator::Equals:
        case VMExpressionOperator::LessThan:
        case VMExpressionOperator::GreaterThan:
        case VMExpressionOperator::GreaterThanOrEqualTo:
        case VMExpressionOperator::NotEqualTo:
        case VMExpressionOperator::LessThanOrEqualTo:
        case VMExpressionOperator::IsFlagSet: {
            lhsValue = VMMemory::GetBigVariable(context, operand->LhsOwner, operand->LhsData);
            rhsValue = VMMemory::GetBigVariable(context, operand->RhsOwner, operand->RhsData);

            bool result = false;
            switch (operand->Operator) {
                case VMExpressionOperator::Equals:
                    result = lhsValue == rhsValue;
                    break;
                case VMExpressionOperator::LessThan:
                    if (rhsValue == 1024 && operand->LhsData == (int32_t)(VMStackObjectVariable::Room) && operand->LhsOwner == VMVariableScope::MyObject)
                        //HACK: see below. ts1 needs this one
                        result = (int32_t)context->Caller->Position().Level - ((int32_t)context->Callee->Position().Level - (int32_t)context->Callee->Object->OBJ->LevelOffset) <= 0;
                    else result = lhsValue < rhsValue;
                    break;
                case VMExpressionOperator::GreaterThan:
                    result = lhsValue > rhsValue;
                    break;
                case VMExpressionOperator::GreaterThanOrEqualTo:
                    if (rhsValue == 1024 && operand->LhsData == (int32_t)(VMStackObjectVariable::Room) && operand->LhsOwner == VMVariableScope::MyObject)
                        //HACK: rooms >= 1024 is "upstairs"... check only used in stairs. Hacked to work with >2 floors
                        result = (int32_t)context->Caller->Position().Level - ((int32_t)context->Callee->Position().Level - (int32_t)context->Callee->Object->OBJ->LevelOffset) > 0;
                    else result = lhsValue >= rhsValue;
                    break;
                case VMExpressionOperator::NotEqualTo:
                    result = lhsValue != rhsValue;
                    break;
                case VMExpressionOperator::LessThanOrEqualTo:
                    result = lhsValue <= rhsValue;
                    break;
                case VMExpressionOperator::IsFlagSet:
                    result = ((lhsValue & Bit(rhsValue - 1)) > 0);
                    break;
                default:
                    break;
            }

            if (result) {
                return VMPrimitiveExitCode::GOTO_TRUE;
            } else {
                return VMPrimitiveExitCode::GOTO_FALSE;
            }
        }

        case VMExpressionOperator::Push: {
            auto& lhsList = VMMemory::GetList(context, operand->LhsOwner);
            rhsValue = VMMemory::GetBigVariable(context, operand->RhsOwner, operand->RhsData);

            switch (operand->LhsData)
            {
                case 0: //front
                    lhsList.push_front((int16_t)rhsValue);
                    break;
                case 1: //back
                    lhsList.push_back((int16_t)rhsValue);
                    break;
                case 2:
                    throw VMSimanticsException("Unknown list push destination: " + std::to_string(operand->LhsData), context);

                default:
                    lhsValue = VMMemory::GetBigVariable(context, operand->LhsOwner, operand->LhsData);
                    rhsValue = VMMemory::GetBigVariable(context, operand->RhsOwner, operand->RhsData);
                    lhsValue |= rhsValue;
                    VMMemory::SetBigVariable(context, operand->LhsOwner, operand->LhsData, lhsValue);
                    break;
            }
            return VMPrimitiveExitCode::GOTO_TRUE;
        }

        case VMExpressionOperator::Pop: {
            auto& rhsList = VMMemory::GetList(context, operand->RhsOwner);
            if (rhsList.size() == 0) return VMPrimitiveExitCode::GOTO_FALSE;

            switch (operand->RhsData)
            {
                case 0: //front
                    lhsValue = rhsList.front();
                    rhsList.pop_front();
                    break;
                case 1: //back
                    lhsValue = rhsList.back();
                    rhsList.pop_back();
                    break;
                case 2:
                    throw VMSimanticsException("Unknown list pop source: " + std::to_string(operand->LhsData), context);

                default:
                    lhsValue = VMMemory::GetBigVariable(context, operand->LhsOwner, operand->LhsData);
                    rhsValue = VMMemory::GetBigVariable(context, operand->RhsOwner, operand->RhsData);
                    lhsValue ^= rhsValue;
                    VMMemory::SetBigVariable(context, operand->LhsOwner, operand->LhsData, lhsValue);
                    break;
            }

            VMMemory::SetBigVariable(context, operand->LhsOwner, operand->LhsData, lhsValue);
            return VMPrimitiveExitCode::GOTO_TRUE;
        }
        case VMExpressionOperator::TS1AssignSqrtRHS: {
            rhsValue = VMMemory::GetBigVariable(context, operand->RhsOwner, operand->RhsData);
            // C#: (short)Math.Sqrt(rhsValue) — NaN (negative input) converts to 0 here instead of being undefined.
            double root = std::sqrt((double)rhsValue);
            int16_t rootValue = std::isnan(root) ? (int16_t)0 : (int16_t)(int32_t)root;
            setResult = VMMemory::SetBigVariable(context, operand->LhsOwner, operand->LhsData, rootValue);
            (void)setResult;
            return VMPrimitiveExitCode::GOTO_TRUE;
        }

        default:
            throw VMSimanticsException("Unknown expression type", context);
    }
}

// region VMPrimitiveOperand Members
void VMExpressionOperand::Read(const uint8_t* bytes)
{
    st::Reader io(bytes, VMPrimitiveOperand::OPERAND_SIZE, st::Endian::Little);
    LhsData = io.ReadInt16();
    RhsData = io.ReadInt16();
    IsSigned = io.ReadByte();
    Operator = (VMExpressionOperator)io.ReadByte();
    LhsOwner = (VMVariableScope)io.ReadByte();
    RhsOwner = (VMVariableScope)io.ReadByte();
}

void VMExpressionOperand::Write(uint8_t* bytes)
{
    st::Writer io(st::Endian::Little); // BinaryWriter over MemoryStream(bytes): overwrites only the written prefix
    io.WriteInt16(LhsData);
    io.WriteInt16(RhsData);
    io.WriteByte(IsSigned);
    io.WriteByte((uint8_t)Operator);
    io.WriteByte((uint8_t)LhsOwner);
    io.WriteByte((uint8_t)RhsOwner);
    std::memcpy(bytes, io.ToArray().data(), std::min(io.ToArray().size(), VMPrimitiveOperand::OPERAND_SIZE));
}
// endregion

}}}
