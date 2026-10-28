/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Engine.VMPrimitiveRegistration
// C# `Type OperandModel` becomes a factory function; use VMPrimitiveRegistration::Model<T>().
#pragma once
#include <cstdint>
#include <string>
#include <memory>
#include <functional>
#include "simantics/engine/VMPrimitiveHandler.h"
namespace FSO { namespace SimAntics { namespace Engine {
class VMPrimitiveRegistration {
public:
    explicit VMPrimitiveRegistration(VMPrimitiveHandler* handler) : Handler(handler) {}

    std::function<VMPrimitiveOperand*()> OperandModel; // null => instruction.Operand stays null
    std::string Name;
    uint16_t Opcode = 0;

    VMPrimitiveHandler* GetHandler() { return Handler.get(); }

    template<class T> static std::function<VMPrimitiveOperand*()> Model() { return []() -> VMPrimitiveOperand* { return new T(); }; }
private:
    std::unique_ptr<VMPrimitiveHandler> Handler;
};
}}}
