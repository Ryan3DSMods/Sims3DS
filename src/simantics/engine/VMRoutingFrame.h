/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Engine.VMRoutingFrame
//
// Determines the path to a set destination from an avatar and provides a system to walk them there. First finds the
// set of room portals to get to the target's room (calls their portal functions in order), then pathfinds to the relevant
// position once in the same room.
//
// If in the same room or a flag is set to "ignore rooms", the room step is ignored and the character is directly routed
// to the specified position. Pathfinders are normally pushed to the stack, but behave completely differently from
// stack frames. You can imagine an example case, where the avatar is currently routing to a door, but the final destination
// is the piano.
//
// STACK:
// Private: Interaction - Play (piano)
// Private: route to piano and sit (piano)
// VMPathFinder: Go on top of, opposing direction. (this is passed as a position and direction.)
// Semi-global: Portal function (door)
// Semi-global: Goto door and gace through (door)
// VMPathFinder: Go on top of, same direction. (passed as position direction, but this time destination is in same room, so we call no portal functions)
//
// Once the first portal function returns true, we will pop back to the initial VMPathFinder to route to the second or the object.
// This can repeat many times.
//
// However, if a portal function returns false, we will need to re-evaluate the portal route to the
// destination, discounting the route from the previous portal to the portal we failed to route to. (nodes are no longer connected)
// The Sims 1 does not do this, but routing problems annoy me as much as everyone else. :)
#pragma once
#include <cstdint>
#include <vector>
#include <list>
#include <memory>
#include <unordered_set>
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMPrimitiveExitCode.h"
#include "simantics/engine/VMSlotParser.h"
#include "simantics/model/VMRoomInfo.h"
#include "simantics/model/VMAnimationState.h"
#include "simantics/model/Routing/VMRouteFailCode.h"

namespace FSO { namespace SimAntics {
class VMAvatar;
namespace Engine {

enum class VMRoutingFrameState : uint8_t {
    INITIAL,

    BEGIN_TURN,
    WALKING,
    TURN_ONLY,
    END_TURN,

    SHOOED, //recalculate route once the stack gets back here.
    ROOM_PORTAL,
    STAND_FUNC,
    FAILED
};

class VMRoutingFrame : public VMStackFrame {
public:
    /// TS1(sims): FreeSO IntersectsOurDestination.
    bool IntersectsOurDestination(const VMFindLocationResult& target) const;
    std::shared_ptr<std::list<st::Point>> WalkTo; // null == C# null
    VMRoutingFrameState State = VMRoutingFrameState::INITIAL;
    int32_t PortalTurns = 0;
    int32_t WaitTime = 0;
    bool CallFailureTrees = false;
    // TS1(sims): a route takes at least one tick (see VMThread::NextInstruction): the tick it was pushed, and the
    // result held back for the next tick when it ended in that same tick.
    int64_t PushTick = -1;
    int8_t HeldResult = -1;

    VMRoutingFrame() {}
    bool IsRoutingFrame() const override { return true; }

    // ADDED(primitives): keeps alive a SLOTItem a primitive built on the fly (C# `new SLOTItem{...}` passed to InitRoutes
    // stayed alive through GC because `Slot` referenced it). Set it before InitRoutes(OwnedSlot.get(), ...).
    std::unique_ptr<FSO::Files::Formats::IFF::Chunks::SLOTItem> OwnedSlot;

    bool InitRoutes(FSO::Files::Formats::IFF::Chunks::SLOTItem* slot, VMEntity* target);
    bool InitRoutes(const std::vector<VMFindLocationResult>& choices); //returns false if we can't find a single route
    void InvalidateRoomRoute();
    void SoftFail(Model::Routing::VMRouteFailCode code, VMEntity* blocker);
    VMPrimitiveExitCode Tick();
    void WalkInterrupt(int32_t waitTime);

private:
    static uint16_t ROUTE_FAIL_TREE;
    static const int16_t FAILED_ROUTE_LOCKOUT; // TS1(sims)

    static uint32_t GOTO_GUID;
    static int16_t SHOO_INTERACTION;
    static uint16_t SHOO_TREE;

    //each within-room route gets these allowances separately.
    static int32_t WAIT_TIMEOUT; //10 seconds
    static int32_t MAX_RETRIES;

    std::vector<Model::VMRoomPortal> Rooms; // C# Stack<VMRoomPortal>: top == back()
    Model::VMRoomPortal CurrentPortal;

    double WalkDirection = 0;
    double TargetDirection = 0;
    bool IgnoreRooms = false;

    int32_t Timeout = WAIT_TIMEOUT;
    int32_t Retries = MAX_RETRIES;

    bool AttemptedChair = false;
    float TurnTweak = 0;
    int32_t TurnFrames = 0;

    int32_t MoveTotalFrames = 0;
    int32_t MoveFrames = 0;
    int32_t Velocity = 0;
    VMRoutingFrame* ParentRoute = nullptr;

    int16_t WalkStyle();
    bool InPool();

    std::unordered_set<Model::VMRoomPortal> IgnoredRooms;
    std::unordered_set<VMAvatar*> AvatarsToConsider;

    FSO::LotView::Model::LotTilePos PreviousPosition;
    FSO::LotView::Model::LotTilePos CurrentWaypoint = FSO::LotView::Model::LotTilePos::OUT_OF_WORLD;

    bool RoomRouteInvalid = false;
    FSO::Files::Formats::IFF::Chunks::SLOTItem* Slot = nullptr;
    VMEntity* Target = nullptr;
    std::unique_ptr<std::vector<VMFindLocationResult>> Choices; // null == C# null
    std::shared_ptr<VMFindLocationResult> CurRoute;             // null == C# null

    void Init();
    void HardFail(Model::Routing::VMRouteFailCode code, VMEntity* blocker);
    bool DoRoomRoute(VMFindLocationResult* route);
    Model::Routing::VMRouteFailCode AttemptRoute(std::shared_ptr<VMFindLocationResult> route);
    /// Pathfinds to the destination position from the current. The room pathfind should get us to the same room before we do this.
    bool AttemptWalk();
    double GetDist(const FSO::LotView::Model::LotTilePos& pos1, const FSO::LotView::Model::LotTilePos& pos2);
    bool PushEntryPoint(int32_t entryPoint, VMEntity* ent);
    bool CanShooAvatar(VMAvatar* avatar);
    VMRoutingFrame* GetParentFrame();
    bool CanPortalTurn();
    void BeginWalk();
    bool EndWalk(); //returns true if we should exit immediately.
    bool Turn(double directionDiff);
    void StartWalkAnimation();
    Model::VMAnimationState* PlayAnim(const std::string& name, VMAvatar* avatar);
    bool AdvanceWaypoint();
};
}}}
