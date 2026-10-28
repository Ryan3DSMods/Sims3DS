/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Engine.VMSimanticsException
#include "simantics/engine/VMSimanticsException.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMRoutingFrame.h"
#include "simantics/engine/VMThread.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"
namespace FSO { namespace SimAntics { namespace Engine {

VMSimanticsException::VMSimanticsException(const std::string& msg, VMStackFrame* ctx)
    : std::runtime_error(msg), message(msg), context(ctx) {}

std::string VMSimanticsException::ToString() const {
    std::string output = message;
    output += "\n\n";
    output += GetStackTrace();
    return output;
}

static std::string TrimEndNull(const std::string& s) {
    size_t end = s.size();
    while (end > 0 && s[end - 1] == '\0') end--;
    return s.substr(0, end);
}

std::string VMSimanticsException::GetStackTrace() const {
    if (context == nullptr || context->Thread == nullptr) return "No Stack Info.";
    std::string output;
    auto& stack = context->Thread->Stack;
    std::string prevEE = "";
    std::string prevER = "";

    for (int i = (int)stack.size() - 1; i >= 0; i--) {
        auto frame = stack[i].get();
        //run in tree:76
        std::string callerStr = (frame->Caller == nullptr) ? "null" : frame->Caller->ToString();
        std::string calleeStr = (frame->Callee == nullptr) ? "null" : frame->Callee->ToString();

        if (callerStr != prevER || calleeStr != prevEE) {
            output += "(" + callerStr + ":" + calleeStr + ") \n";
            prevEE = calleeStr;
            prevER = callerStr;
        }
        output += " > ";
        if (frame->IsRoutingFrame()) {
            output += "VMRoutingFrame with state: ";
            output += std::to_string((int)((VMRoutingFrame*)frame)->State);
        } else {
            output += TrimEndNull(frame->Routine->Rti.Name);
            output += ":";
            output += std::to_string(frame->InstructionPointer);
            output += " (";
            auto opcode = frame->GetCurrentInstruction().Opcode;
            auto primitive = (opcode > 255) ? nullptr : context->VM()->Context->Primitives[opcode].get();
            output += (primitive == nullptr) ? std::to_string(opcode) : primitive->Name;
            output += ")";
        }
        output += "\n";
    }
    return output;
}
}}}
