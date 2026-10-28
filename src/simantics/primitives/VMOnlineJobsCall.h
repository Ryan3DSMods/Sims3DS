/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Primitives.VMOnlineJobsCall / VMOnlineJobsCallOperand / VMOnlineJobsCallMode (primitives/VMOnlineJobsCall.cs)
#pragma once
#include <cstdint>
#include "simantics/engine/VMPrimitiveHandler.h"
#include "simantics/engine/VMPrimitiveOperand.h"

namespace FSO { namespace SimAntics { namespace Primitives {

enum class VMOnlineJobsCallMode : uint8_t
{
    GotoJobLot = 0,
    SetControllerID = 1,
    AttemptToValidateWorker = 2,
    Unused1 = 3,
    Unused2 = 4,
    AddStatusMessage = 5,
    RemoveStatusMessage = 6,
    SetTimeRemaining = 7,
    GetRandomJob = 8,
    IsJobAvailable = 9,
    SignalJobTypeChange = 10,
    SignalJobGradeChange = 11,
    SetWorkMode = 12
};

class VMOnlineJobsCall : public Engine::VMPrimitiveHandler {
public:
    Engine::VMPrimitiveExitCode Execute(Engine::VMStackFrame* context, Engine::VMPrimitiveOperand* args) override;
};

class VMOnlineJobsCallOperand : public Engine::VMPrimitiveOperand {
public:
    VMOnlineJobsCallMode Call = VMOnlineJobsCallMode::GotoJobLot;

    // #region VMPrimitiveOperand Members
    void Read(const uint8_t* bytes) override;
    void Write(uint8_t* bytes) override;
    // #endregion
};

}}}
