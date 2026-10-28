/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Engine.Utils.VMMemory — variable scope access for SimAntics instructions.
#pragma once
#include <cstdint>
#include <list>
#include <string>
#include "simantics/_files_contract.h"
#include "content/ContentManager.h"
#include "simantics/engine/scopes/VMVariableScope.h"
#include "simantics/engine/scopes/VMAnimationScope.h"
#include "simantics/engine/scopes/VMSuitScope.h"
#include "simantics/engine/scopes/VMSlotScope.h"
#include "simantics/model/VMStackObjectDefinitionVariable.h"
#include "vitaboy/Animation.h"
#include "vitaboy/Appearance.h"

namespace FSO { namespace SimAntics {
class VMEntity;
namespace Engine {
class VMStackFrame;
namespace Utils {

class VMMemory {
public:
    /// Get a variable (16 bit). Throws VMSimanticsException for invalid/unimplemented scopes.
    static int16_t GetVariable(VMStackFrame* context, Scopes::VMVariableScope scope, int16_t data);
    /// TS1(sims): FreeSO GetTreeAd — check tree ad (type 0 min, 1 range, 2 personality) set by the check tree, or
    /// else the active interaction's TTAB motive entry.
    static int16_t GetTreeAd(VMStackFrame* context, int32_t type, uint16_t data);
    static std::list<int16_t>& GetList(VMStackFrame* context, Scopes::VMVariableScope scope);
    /// used by functions which can take 32 bit integers, such as VMExpression. (TempXL aware)
    static int32_t GetBigVariable(VMStackFrame* context, Scopes::VMVariableScope scope, int16_t data);
    static int16_t GetTuningVariable(VMEntity* entity, uint16_t data, VMStackFrame* context);
    static std::string GetTuningVariableLabel(FSO::Content::GameObject* obj, uint16_t data);
    //hilariously large switch case. there's got to be a better way
    static int16_t GetEntityDefinitionVar(FSO::Files::Formats::IFF::Chunks::OBJD* objd, Model::VMOBJDVariable var, VMStackFrame* context);
    /// Set a variable (16 bit).
    static bool SetVariable(VMStackFrame* context, Scopes::VMVariableScope scope, int16_t data, int16_t value);
    static bool SetBigVariable(VMStackFrame* context, Scopes::VMVariableScope scope, int16_t data, int32_t value);
    /// Get an animation (nullptr if not found)
    static FSO::Vitaboy::Animation* GetAnimation(VMStackFrame* context, Scopes::VMAnimationScope scope, uint16_t id);
    static FSO::Vitaboy::Appearance* GetSuit(VMStackFrame* context, Scopes::VMSuitScope scope, uint16_t id);
    static FSO::Files::Formats::IFF::Chunks::SLOTItem* GetSlot(VMStackFrame* context, Scopes::VMSlotScope scope, uint16_t data);
};
}}}}
