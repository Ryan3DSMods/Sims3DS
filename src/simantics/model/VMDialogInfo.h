/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Model.VMDialogInfo
#pragma once
#include <cstdint>
#include <string>
#include "simantics/engine/VMDialogOperand.h"
namespace FSO { namespace SimAntics {
class VMEntity;
namespace Model {
class VMDialogInfo {
public:
    bool Block = false;
    VMEntity* Caller = nullptr;
    VMEntity* Icon = nullptr;
    FSO::SimAntics::Primitives::VMDialogOperand Operand;
    std::string Message;
    std::string IconName;
    std::string Title;
    std::string Yes;    bool HasYes = false;    // C# null-able strings
    std::string No;     bool HasNo = false;
    std::string Cancel; bool HasCancel = false;
    uint64_t DialogID = 0; //what primitive this dialog belongs to. (GUID<<32) | (BHAVID<<16) | (pointer) informs ui of duplicates.
};
}}}
