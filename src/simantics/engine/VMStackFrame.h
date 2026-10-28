/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Engine.VMStackFrame
#pragma once
#include <cstdint>
#include <vector>
#include "simantics/_files_contract.h"
#include "content/ContentManager.h"
#include "simantics/engine/translator/VMRoutine.h"

namespace FSO { namespace SimAntics {
class VM;
class VMEntity;
class VMContext;
namespace Engine {
class VMThread;

// TS1(sims): FreeSO VMSpecialResult (replaces FreeSims' bool DiscardResult). Interaction marks the frame an
// interaction was pushed with (Allow Push / Gosub Found Action / run immediately); popping it ends that interaction.
enum class VMSpecialResult : uint8_t {
    Normal,
    Interaction,
    Retry
};

/// Holds information about the execution of a routine
class VMStackFrame {
public:
    VMStackFrame() {}
    virtual ~VMStackFrame() {}

    /** Thread executing this routine **/
    VMThread* Thread = nullptr;
    /** Routine that this context relates to **/
    VMRoutine* Routine = nullptr;
    /** Current instruction **/
    uint16_t InstructionPointer = 0;
    /** The object who executed this behavior **/
    VMEntity* Caller = nullptr;
    /** The object the code is running on **/
    VMEntity* Callee = nullptr;
    /** An object selected by the code to perform operations on. **/
    VMEntity* StackObject = nullptr;
    /** TS1(sims): raw value last written to the "Stack Object ID" variable. In The Sims 1 the stack object
     ** id also carries ids that are not objects on the lot: "Set to Next neighbor" stores a *neighbour id* there and
     ** "neighbor in stack object" / "neighbor's person data" / neighbour relationships read it back. FreeSims kept
     ** only the entity pointer, so the id was lost (read back as 0) and neighbour loops never terminated.
     ** Only consulted while StackObject is null (see StackObjectID()). Copied into subroutine frames. **/
    int16_t StackObjectRawID = 0;
    /** TS1(sims): C# (FreeSO) `context.StackObjectID`. */
    int16_t StackObjectID() const;
    /** TS1(sims): FreeSO SpecialResult (FreeSims: bool DiscardResult, "not a subroutine, return with a continue"). **/
    VMSpecialResult SpecialResult = VMSpecialResult::Normal;
    /** Indicates that the current stack frame is part of an action tree.
     ** Set by "idle for input, allow push", when an interaction is selected.
     ** Used to stop recursive interactions, is only false when within "main". **/
    bool ActionTree = false;

    /** Used to get strings and other resources (for primitives) from the code owner, as it may not be the callee but instead a semiglobal or global. **/
    FSO::Content::GameIffResource* ScopeResource() const { return CodeOwner->Resource; }

    FSO::Content::GameObject* CodeOwner = nullptr;
    /** Routine locals **/
    std::vector<int16_t> Locals;
    /** Arguments **/
    std::vector<int16_t> Args;

    FSO::Content::GameObjectResource* CallerPrivate() const;
    FSO::Content::GameObjectResource* CalleePrivate() const;
    FSO::Content::GameObjectResource* StackObjPrivate() const;
    FSO::Content::GameGlobal* Global() const;
    FSO::SimAntics::VM* VM() const;

    /** Utilities **/
    VMInstruction& GetCurrentInstruction() { return Routine->Instructions[InstructionPointer]; }
    template<class T> T* GetCurrentOperand() { return static_cast<T*>(GetCurrentInstruction().Operand.get()); }

    virtual bool IsRoutingFrame() const { return false; } // C# `frame is VMRoutingFrame`
};
}}}
