/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Engine.VMSimanticsException
#pragma once
#include <stdexcept>
#include <string>
namespace FSO { namespace SimAntics { namespace Engine {
class VMStackFrame;
class VMSimanticsException : public std::runtime_error {
public:
    VMSimanticsException(const std::string& message, VMStackFrame* context);
    std::string ToString() const;
    std::string GetStackTrace() const;
private:
    std::string message;
    VMStackFrame* context;
};
}}}
