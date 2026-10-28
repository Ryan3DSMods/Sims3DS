/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims SimsVille/SimsAntics/primitives/VMFindBestAction.cs
#pragma once
#include <cstdint>
#include "simantics/engine/VMPrimitiveHandler.h"
#include "simantics/engine/VMPrimitiveOperand.h"
#include "simantics/model/VMPersonDataVariable.h"
#include "simantics/model/VMMotive.h"
#include "simantics/entities/VMEntity.h"
#include <vector>
#include <functional>
#include <string>

namespace FSO { namespace SimAntics { namespace Primitives {

// Finds a suitable action and queues it onto this sim. Used for pet free will.

class VMFindBestAction : public Engine::VMPrimitiveHandler {
public:
    Engine::VMPrimitiveExitCode Execute(Engine::VMStackFrame* context, Engine::VMPrimitiveOperand* args) override;

    // TS1(sims): FreeSO VMFindBestAction (The Sims 1 opcode 3 "Find Best Interaction").
    static const Model::VMPersonDataVariable VaryByTypes[23];
    static const Model::VMMotive WeightMotives[9];
    static int MotiveToWeight(int motive);
    /// ADDED(sims): diagnostics — called for every auto interaction evaluated (checkPassed false = rejected by
    /// flags/check tree; score is only meaningful when it passed). Null by default.
    static std::function<void(VMEntity* caller, VMEntity* obj, const std::string& name, bool checkPassed, float score)> DebugCandidate;
private:
    Engine::VMPrimitiveExitCode ExecuteTS1(Engine::VMStackFrame* context);
    static std::vector<VMPieMenuInteraction> TakeTopActions(const std::vector<VMPieMenuInteraction>& list, int count);
};

class VMFindBestActionOperand : public Engine::VMPrimitiveOperand {
public:
    // VMPrimitiveOperand Members
    void Read(const uint8_t* bytes) override;
    void Write(uint8_t* bytes) override;
};

}}}
