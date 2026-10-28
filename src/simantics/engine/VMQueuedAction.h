/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Engine.VMQueuedAction / VMQueuePriority / VMQueueMode
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include "simantics/_files_contract.h"
#include "content/ContentManager.h"
#include "simantics/engine/translator/VMRoutine.h"
#include "simantics/engine/VMStackFrame.h"
namespace FSO { namespace SimAntics {
class VMEntity;
namespace Engine {
class VMActionCallback;

enum class VMQueuePriority : int16_t {
    Maximum = 100, Autonomous = 2, UserDriven = 50, ParentIdle = 25, ParentExit = 24, Idle = 0
};

enum class VMQueueMode : uint8_t {
    Normal, ParentIdle,
    ParentExit, //hidden until active. DO NOT CANCEL OR SKIP!
    Idle
};

class VMQueuedAction {
public:
    VMQueuedAction() {}

    VMRoutine* ActionRoutine = nullptr;
    VMRoutine* CheckRoutine = nullptr;
    VMEntity* Callee = nullptr;
    VMEntity* StackObject = nullptr; //set to callee for interactions

    VMEntity* IconOwner() const { return (_IconOwner == nullptr) ? Callee : _IconOwner; } //defaults to callee
    void IconOwner(VMEntity* value) { _IconOwner = value; }

    FSO::Content::GameObject* CodeOwner = nullptr; //used to access local resources from BHAVs like strings
    std::string Name;
    std::vector<int16_t> Args; //WARNING - if you use this, the args array MUST have the same number of elements the routine is expecting! (empty == C# null)

    int32_t InteractionNumber = -1; //this interaction's number... This is needed for create object callbacks
                                    //for This Interaction but entry point functions don't have this...
                                    //suggests init and main don't use action queue.
    /** FreeSims name kept (PORTING.md). TS1(sims): carries FreeSO's NotifyIdle semantics — "should this queue
     ** item notify idle for input? (happens if priority is 0 or less than followups)" — set by VMThread's queue code. **/
    bool Cancelled = false;
    /// FIX(sims): queued by the player (pie menu). The TS1 'queue skip lower priority' rule in EnqueueAction leaves it.
    bool UserQueued = false;

    int16_t Priority = (int16_t)VMQueuePriority::Idle;
    VMQueueMode Mode = VMQueueMode::Normal;
    FSO::Files::Formats::IFF::Chunks::TTABFlags Flags = (FSO::Files::Formats::IFF::Chunks::TTABFlags)0;
    FSO::Files::Formats::IFF::Chunks::TSOFlags Flags2 = (FSO::Files::Formats::IFF::Chunks::TSOFlags)0x1f;

    uint16_t UID = 0; //a wraparound ID that is just here so that a specific interaction can be reliably "cancelled" by a client.

    std::shared_ptr<VMActionCallback> Callback;

    std::shared_ptr<VMStackFrame> ToStackFrame(VMEntity* caller);
private:
    VMEntity* _IconOwner = nullptr; //defaults to callee
};
}}}
