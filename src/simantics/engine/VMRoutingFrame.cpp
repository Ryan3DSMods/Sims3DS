/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Engine.VMRoutingFrame (see header for an overview)
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include "simantics/engine/VMRoutingFrame.h"
#include "simantics/engine/VMThread.h"
#include "simantics/engine/VMSimanticsException.h"
#include "simantics/engine/Routing/VMRectRouter.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/entities/VMGameObject.h"
#include "simantics/entities/VMMultitileGroup.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"
#include "common/utils/DirectionUtils.h"
#include "content/ContentManager.h"
#include <unordered_map>
#include <algorithm>
#include <cmath>

namespace FSO { namespace SimAntics { namespace Engine {
using namespace FSO::Files::Formats::IFF::Chunks;
using namespace FSO::SimAntics::Model;
using namespace FSO::SimAntics::Model::Routing;
using FSO::LotView::Model::LotTilePos;
using FSO::Common::Utils::DirectionUtils;
using st::Point;

uint16_t VMRoutingFrame::ROUTE_FAIL_TREE = 398;
const int16_t VMRoutingFrame::FAILED_ROUTE_LOCKOUT = 200; // TS1(sims): ticks, as global 'fail food chain object'
uint32_t VMRoutingFrame::GOTO_GUID = 0x000007C4;
int16_t VMRoutingFrame::SHOO_INTERACTION = 3;
uint16_t VMRoutingFrame::SHOO_TREE = 4107;
int32_t VMRoutingFrame::WAIT_TIMEOUT = 10 * 30; //10 seconds
int32_t VMRoutingFrame::MAX_RETRIES = 10;

static VMEntity* NonNull(VMEntity* e) {
    if (e == nullptr) throw VMSimanticsException("Object reference not set to an instance of an object.", nullptr);
    return e;
}

int16_t VMRoutingFrame::WalkStyle() {
    return (InPool()) ? (int16_t)0 : Caller->GetValue(VMStackObjectVariable::WalkStyle);
}

bool VMRoutingFrame::InPool() {
    auto& ri = VM()->Context->RoomInfo;
    auto room = VM()->Context->GetRoomAt(Caller->Position());
    return room < ri.size() && ri[room].Room.IsPool;
}

void VMRoutingFrame::Init() {
    ParentRoute = GetParentFrame();
    if (ParentRoute != nullptr) {
        AvatarsToConsider = ParentRoute->AvatarsToConsider;
    } else {
        for (auto obj : VM()->Entities) {
            auto colAvatar = dynamic_cast<VMAvatar*>(obj);
            if (colAvatar != nullptr && colAvatar->Thread != nullptr && colAvatar->Thread->Stack.size() > 0) {
                auto colTopFrame = colAvatar->Thread->Stack.back().get();
                if (colTopFrame != nullptr && colTopFrame->IsRoutingFrame()) {
                    auto colRoute = (VMRoutingFrame*)colTopFrame;
                    if (colRoute->WaitTime > 0) AvatarsToConsider.insert(colAvatar);
                }
            }
        }
    }
}

bool VMRoutingFrame::InitRoutes(SLOTItem* slot, VMEntity* target) {
    Init();

    Slot = slot;
    Target = target;
    auto found = AttemptRoute(nullptr);

    if (found == VMRouteFailCode::Success) return true;
    else HardFail(found, nullptr);
    return false;
}

bool VMRoutingFrame::InitRoutes(const std::vector<VMFindLocationResult>& choices) { //returns false if we can't find a single route
    Init();

    Choices.reset(new std::vector<VMFindLocationResult>(choices)); //should be ordered by most preferred first, with a little bit of random shuffling to keep things interesting for "wander"
    //style movements. Also includes flags dictating if this route goes through walls etc.
    auto found = VMRouteFailCode::NoValidGoals;
    while (found != VMRouteFailCode::Success && Choices->size() > 0) {
        found = AttemptRoute(std::make_shared<VMFindLocationResult>((*Choices)[0]));
        Choices->erase(Choices->begin());
    }

    if (found == VMRouteFailCode::Success) return true;
    else HardFail(found, nullptr);
    return false;
}

void VMRoutingFrame::InvalidateRoomRoute() {
    RoomRouteInvalid = true;
}

void VMRoutingFrame::SoftFail(VMRouteFailCode code, VMEntity* blocker) {
    auto found = VMRouteFailCode::NoValidGoals;
    while (found != VMRouteFailCode::Success && Choices != nullptr && Choices->size() > 0) {
        found = AttemptRoute(std::make_shared<VMFindLocationResult>((*Choices)[0]));
        Choices->erase(Choices->begin());
    }

    if (found != VMRouteFailCode::Success) HardFail(code, blocker);
}

// TS1(sims): FreeSO VMRoutingFrame.IntersectsOurDestination.
bool VMRoutingFrame::IntersectsOurDestination(const VMFindLocationResult& target) const {
    if (CurRoute == nullptr) return false;
    if (target.Chair != nullptr && target.Chair == CurRoute->Chair) return true;
    auto pos = CurRoute->Position;
    auto tpos = target.Position;
    return tpos.x >= pos.x - 6 && tpos.x <= pos.x + 6 && tpos.y >= pos.y - 6 && tpos.y <= pos.y + 6; // C# VMObstacle.Contains
}

void VMRoutingFrame::HardFail(VMRouteFailCode code, VMEntity* blocker) {
    State = VMRoutingFrameState::FAILED;
    auto avatar = (VMAvatar*)Caller;
    if (CallFailureTrees && ParentRoute == nullptr) {
        auto bhav = Global()->Resource->Get<BHAV>(ROUTE_FAIL_TREE);
        Thread->ExecuteSubRoutine(this, bhav, CodeOwner, Primitives::VMSubRoutineOperand((int16_t)code, (blocker == nullptr) ? (int16_t)0 : blocker->ObjectID, 0, 0));
    }
    avatar->SetPersonData(VMPersonDataVariable::RouteResult, (int16_t)code);
    // ADDED(sims): diagnostics for simlife (ROUTE_DEBUG=1).
    static const bool routeDebug = getenv("ROUTE_DEBUG") != nullptr;
    if (routeDebug) printf("   [route] %s failed code %d target %s blocker %s\n", Caller->ToString().c_str(), (int)code,
        Target ? Target->ToString().c_str() : "-", blocker ? blocker->ToString().c_str() : "-");

    // TS1(sims): memory of failed autonomous routes. For autonomous interactions the TS1 route failure tree
    // (global 398 'route failure feedback') returns at once (priority <= 25: no balloon) and nothing marks the target,
    // so Find Best Interaction re-chose the unreachable object every tick (hundreds of identical picks per game hour,
    // e.g. two Sims blocking each other in a doorway). The game's own idiom for "don't pick this object again for a
    // while" is the object's lockout count (global 'fail food chain object' sets 200 ticks); we apply it to the target
    // of the failed autonomous interaction. Inference - see STUBS_autonomy.md.
    if (Thread != nullptr && Thread->Context->VM->TS1 && code != VMRouteFailCode::Interrupted && ActionTree) {
        auto active = Thread->ActiveAction();
        if (active != nullptr && active->Callee != nullptr && active->Callee != Caller
            && active->Priority <= (int16_t)VMQueuePriority::Autonomous
            && active->Callee->GetValue(VMStackObjectVariable::LockoutCount) < FAILED_ROUTE_LOCKOUT)
            active->Callee->SetValue(VMStackObjectVariable::LockoutCount, FAILED_ROUTE_LOCKOUT);
    }
}

bool VMRoutingFrame::DoRoomRoute(VMFindLocationResult* route) {
    Rooms.clear();

    LotTilePos dest;
    if (Slot != nullptr && Target != nullptr) {
        //take destination pos from object. Estimate room closeness using distance to object, not destination.
        dest = Target->Position();
    } else {
        if (route != nullptr) dest = route->Position;
        else return false; //???
    }

    auto DestRoom = VM()->Context->GetRoomAt(dest);
    auto MyRoom = VM()->Context->GetRoomAt(Caller->Position());

    IgnoreRooms = (Slot != nullptr && (Slot->Rsflags & SLOTFlags::IgnoreRooms) > 0);

    if (DestRoom == MyRoom || IgnoreRooms) return true; //we don't have to do any room finding for this
    else {
        //find shortest room traversal to destination. Simple A* pathfind.
        //Portals are considered nodes to allow multiple portals between rooms to be considered.

        std::vector<VMRoomPortal> openSet; //we use this like a queue, but we need certain functions for sorted queue that are only provided by list.
        std::unordered_set<VMRoomPortal> closedSet;

        std::unordered_map<VMRoomPortal, double> gScore;
        std::unordered_map<VMRoomPortal, double> fScore;
        std::unordered_map<VMRoomPortal, VMRoomPortal> parents;

        auto StartPortal = VMRoomPortal(Caller->ObjectID, MyRoom); //consider the sim as a portal to this room (as a starting point)
        openSet.push_back(StartPortal);
        gScore[StartPortal] = 0;
        fScore[StartPortal] = GetDist(Caller->Position(), dest);

        auto sortedInsert = [&](const VMRoomPortal& portal) {
            auto myScore = fScore[portal];
            for (size_t i = 0; i < openSet.size(); i++) {
                if (myScore < fScore[openSet[i]]) {
                    openSet.insert(openSet.begin() + i, portal);
                    return;
                }
            }
            openSet.push_back(portal);
        };

        while (openSet.size() != 0) {
            auto current = openSet[0];
            openSet.erase(openSet.begin());

            if (current.TargetRoom == DestRoom) {
                //this portal gets us to the room.
                std::vector<VMRoomPortal> path;
                while (current != StartPortal) //push previous portals till we get to our first "portal", the sim in its current room (we have already "traversed" this portal)
                {
                    Rooms.push_back(current);
                    current = parents[current];
                }
                return true;
            }

            closedSet.insert(current);

            if (current.TargetRoom >= VM()->Context->RoomInfo.size()) continue;
            auto portals = VM()->Context->RoomInfo[current.TargetRoom].Portals;

            for (auto& portal : portals) { //evaluate all neighbor portals
                if (IgnoredRooms.count(portal) || closedSet.count(portal)) continue; //already evaluated, or couldn't get to the portal.

                auto pos = NonNull(VM()->GetObjectById(portal.ObjectID))->Position();
                auto gFromCurrent = gScore[current] + GetDist(NonNull(VM()->GetObjectById(current.ObjectID))->Position(), pos);
                auto newcomer = std::find(openSet.begin(), openSet.end(), portal) == openSet.end();

                if (newcomer || gFromCurrent < gScore[portal]) {
                    parents[portal] = current; //best parent for now
                    gScore[portal] = gFromCurrent;
                    fScore[portal] = gFromCurrent + GetDist(pos, dest);
                    if (newcomer) { //add and move to relevant position
                        sortedInsert(portal);
                    } else { //remove and reinsert to refresh sort
                        openSet.erase(std::find(openSet.begin(), openSet.end(), portal));
                        sortedInsert(portal);
                    }
                }
            }
        }

        return false;
    }
}

VMRouteFailCode VMRoutingFrame::AttemptRoute(std::shared_ptr<VMFindLocationResult> route) { //returns false if there is no room portal route to the destination room.
    //if route is not null, we are on a DIRECT route, where either the SLOT has been resolved or a route has already been passed to us.
    //resets some variables either way, so that the route can start again.

    CurRoute = route;

    WalkTo = nullptr; //reset routing state
    AttemptedChair = false;
    TurnTweak = 0;

    return (DoRoomRoute(route.get())) ? VMRouteFailCode::Success : VMRouteFailCode::NoRoomRoute;
}

bool VMRoutingFrame::AttemptWalk() {
    //find shortest path to destination tile. Simple A* pathfind.
    //portals are used to traverse floors, so we do not care about the floor each point is on.
    //when evaluating possible adjacent tiles we use the Caller's current floor.

    LotTilePos startPos = Caller->Position();
    CurrentWaypoint = LotTilePos::OUT_OF_WORLD;
    auto myRoom = VM()->Context->GetRoomAt(startPos);
    if (myRoom >= VM()->Context->RoomInfo.size()) return false;

    auto& roomInfo = VM()->Context->RoomInfo[myRoom];
    std::vector<VMObstacle> obstacles;

    int bx = (roomInfo.Room.Bounds.X - 1) << 4;
    int by = (roomInfo.Room.Bounds.Y - 1) << 4;
    int width = (roomInfo.Room.Bounds.Width + 2) << 4;
    int height = (roomInfo.Room.Bounds.Height + 2) << 4;
    obstacles.push_back(VMObstacle(bx - 16, by - 16, bx + width + 16, by));
    obstacles.push_back(VMObstacle(bx - 16, by + height, bx + width + 16, by + height + 16));

    obstacles.push_back(VMObstacle(bx - 16, by - 16, bx, by + height + 16));
    obstacles.push_back(VMObstacle(bx + width, by - 16, bx + width + 16, by + height + 16));

    auto ents = roomInfo.Entities; //copy: entry points may change room membership
    for (auto obj : ents) {
        auto ft = obj->Footprint.get();

        auto flags = (VMEntityFlags)obj->GetValue(VMStackObjectVariable::Flags);
        std::vector<int16_t> a1 = { obj->ObjectID, 0, 0, 0 };
        std::vector<int16_t> a2 = { Caller->ObjectID, 0, 0, 0 };
        if (obj != Caller && ft != nullptr &&
            (dynamic_cast<VMGameObject*>(obj) != nullptr || AvatarsToConsider.count(dynamic_cast<VMAvatar*>(obj))) &&
            ((flags & VMEntityFlags::DisallowPersonIntersection) > 0 || (flags & VMEntityFlags::AllowPersonIntersection) == 0)
            && (!(Caller->ExecuteEntryPoint(5, VM()->Context, true, obj, &a1)
                || obj->ExecuteEntryPoint(5, VM()->Context, true, Caller, &a2))))
            obstacles.push_back(VMObstacle(ft->x1 - 3, ft->y1 - 3, ft->x2 + 3, ft->y2 + 3));
    }

    obstacles.insert(obstacles.end(), roomInfo.Room.WallObs.begin(), roomInfo.Room.WallObs.end());
    if (!IgnoreRooms) obstacles.insert(obstacles.end(), roomInfo.Room.RoomObs.begin(), roomInfo.Room.RoomObs.end());

    auto startPoint = Point((int)startPos.x, (int)startPos.y);
    auto endPoint = Point((int)CurRoute->Position.x, (int)CurRoute->Position.y);

    // FIX(sims) QA2: two Sims meeting in a doorway stop on the wall line (shooed mid-portal), each a few units inside
    // the wall / other room's obstacle of its own room. Every later route then failed at once ("start inside an
    // obstacle") and both stood there until they starved (CAS family, house 1: Alice/Cid 2 game days, all motives
    // -100). A start point less than 3/4 tile inside an obstacle is nudged out to the nearest free point of the same
    // room (at most 12 units, invisible) before routing; deeper overlaps still fail as before.
    for (int pass = 0; pass < 2; pass++) {
        const VMObstacle* inside = nullptr;
        for (auto& rect : obstacles) if (rect.HardContains(startPoint)) { inside = &rect; break; }
        if (inside == nullptr) break;
        bool moved = false;
        if (pass == 0) {
            const Point cands[4] = { Point(inside->x1 - 1, startPoint.Y), Point(inside->x2 + 1, startPoint.Y),
                                     Point(startPoint.X, inside->y1 - 1), Point(startPoint.X, inside->y2 + 1) };
            int best = -1, bestD = 13;
            for (int c = 0; c < 4; c++) {
                int d = std::abs(cands[c].X - startPoint.X) + std::abs(cands[c].Y - startPoint.Y);
                if (d >= bestD) continue;
                LotTilePos np((int16_t)cands[c].X, (int16_t)cands[c].Y, startPos.Level);
                if (VM()->Context->GetRoomAt(np) != myRoom) continue;
                bool free = true;
                for (auto& r2 : obstacles) if (r2.HardContains(cands[c])) { free = false; break; }
                if (!free) continue;
                best = c; bestD = d;
            }
            if (best >= 0) {
                auto storedDir = ((VMAvatar*)Caller)->RadianDirection();
                LotTilePos np((int16_t)cands[best].X, (int16_t)cands[best].Y, startPos.Level);
                auto res = Caller->SetPosition(np, FSO::LotView::Model::Direction::NORTH, VM()->Context);
                ((VMAvatar*)Caller)->RadianDirection(storedDir);
                if (res.Status == VMPlacementError::Success) { startPos = np; startPoint = cands[best]; moved = true; }
            }
        }
        if (!moved) return false;
    }

    std::vector<VMObstacle*> map;
    for (auto& o : obstacles) map.push_back(&o);
    Routing::VMRectRouter router(map);

    if (startPoint == endPoint) {
        State = VMRoutingFrameState::TURN_ONLY;
        return true;
    }

    WalkTo = router.Route(startPoint, endPoint);
    if (WalkTo != nullptr) {
        if (WalkTo->front() != endPoint) WalkTo->pop_front();
        AdvanceWaypoint();
    }
    return (WalkTo != nullptr);
}

double VMRoutingFrame::GetDist(const LotTilePos& pos1, const LotTilePos& pos2) {
    return std::sqrt(std::pow(pos1.x - pos2.x, 2) + std::pow(pos1.y - pos2.y, 2)) / 16.0 + std::abs(pos1.Level - pos2.Level) * 10;
}

bool VMRoutingFrame::PushEntryPoint(int32_t entryPoint, VMEntity* ent) {
    if (ent == nullptr) return false;
    if (ent->EntryPoints[entryPoint].ActionFunction != 0) {
        bool Execute;
        if (ent->EntryPoints[entryPoint].ConditionFunction != 0) //check if we can definitely execute this...
        {
            auto Behavior = ent->GetBHAVWithOwner(ent->EntryPoints[entryPoint].ConditionFunction, VM()->Context);
            if (Behavior != nullptr) {
                auto frame = std::make_shared<VMStackFrame>();
                frame->Caller = Caller;
                frame->Callee = ent;
                frame->CodeOwner = Behavior->owner;
                frame->StackObject = ent;
                frame->Routine = VM()->Assemble(Behavior->bhav);
                frame->Args.assign(4, 0);
                Execute = (VMThread::EvaluateCheck(VM()->Context, Caller, frame) == VMPrimitiveExitCode::RETURN_TRUE);
            }
            else Execute = true;
        } else {
            Execute = true;
        }

        if (Execute) {
            //push it onto our stack, except now the object owns our soul! when we are returned to we can evaluate the result and determine if the action failed.
            auto Behavior = ent->GetBHAVWithOwner(ent->EntryPoints[entryPoint].ActionFunction, VM()->Context);
            if (Behavior == nullptr) return false; //invalid id
            auto routine = VM()->Assemble(Behavior->bhav);
            auto childFrame = std::make_shared<VMStackFrame>();
            childFrame->Routine = routine;
            childFrame->Caller = Caller;
            childFrame->Callee = ent;
            childFrame->CodeOwner = Behavior->owner;
            childFrame->StackObject = ent;
            childFrame->ActionTree = ActionTree;
            childFrame->Args.assign(routine->Arguments, 0);
            Thread->Push(childFrame);
            return true;
        } else {
            return false; //could not execute portal function. todo: re-evaluate room route
        }
    } else {
        return false;
    }
}

VMPrimitiveExitCode VMRoutingFrame::Tick() {
    auto avatar = (VMAvatar*)Caller;

    if (State != VMRoutingFrameState::FAILED && avatar->GetFlag(VMEntityFlags::InteractionCanceled) && avatar->GetPersonData(VMPersonDataVariable::NonInterruptable) == 0) {
        HardFail(VMRouteFailCode::Interrupted, nullptr);
        return VMPrimitiveExitCode::CONTINUE;
    }

    if (WaitTime > 0) {
        if (Velocity > 0) Velocity--;

        if (avatar->Animations.size() < 3) StartWalkAnimation();
        avatar->Animations[0]->Weight = (8 - Velocity) / 8.0f;
        avatar->Animations[1]->Weight = (Velocity / 8.0f) * 0.66f;
        avatar->Animations[2]->Weight = (Velocity / 8.0f) * 0.33f;

        WaitTime--;
        Timeout--;
        if (Timeout <= 0) {
            //try again. not sure if we should reset timeout for the new route
            SoftFail(VMRouteFailCode::NoPath, nullptr);
            if (State != VMRoutingFrameState::FAILED) {
                Velocity = 0;
                State = VMRoutingFrameState::WALKING;
            }
        } else return VMPrimitiveExitCode::CONTINUE_NEXT_TICK;
    }

    if (RoomRouteInvalid && State != VMRoutingFrameState::BEGIN_TURN && State != VMRoutingFrameState::END_TURN) {
        RoomRouteInvalid = false;
        IgnoredRooms.clear();

        WalkTo = nullptr; //reset routing state
        if (!DoRoomRoute(CurRoute.get())) {
            if (CurRoute != nullptr) SoftFail(VMRouteFailCode::NoRoomRoute, nullptr);
            else HardFail(VMRouteFailCode::NoRoomRoute, nullptr);
        } else if (Rooms.size() > 0) {
            State = VMRoutingFrameState::INITIAL;
        }
    }

    switch (State) {
        case VMRoutingFrameState::STAND_FUNC:
            if (Thread->LastStackExitCode == VMPrimitiveExitCode::RETURN_TRUE) {
                State = VMRoutingFrameState::INITIAL;
                if (avatar->GetPersonData(VMPersonDataVariable::Posture) == 1) avatar->SetPersonData(VMPersonDataVariable::Posture, 0);
            } else {
                auto resultID = avatar->GetValue(VMStackObjectVariable::PrimitiveResultID);
                SoftFail(VMRouteFailCode::CantStand, (resultID == 0) ? nullptr : VM()->GetObjectById(resultID));
            }
            return VMPrimitiveExitCode::CONTINUE;
        case VMRoutingFrameState::INITIAL:
        case VMRoutingFrameState::ROOM_PORTAL: {
            //check if the room portal that just finished succeeded.
            if (State == VMRoutingFrameState::ROOM_PORTAL) {
                if (Thread->LastStackExitCode != VMPrimitiveExitCode::RETURN_TRUE) {
                    IgnoredRooms.insert(CurrentPortal);
                    State = VMRoutingFrameState::INITIAL;
                    if (!DoRoomRoute(CurRoute.get())) {
                        SoftFail(VMRouteFailCode::NoRoomRoute, nullptr); //todo: reattempt room route with portal we tried removed.
                        return VMPrimitiveExitCode::CONTINUE;
                    }
                }
            }

            if (Rooms.size() > 0) { //push portal function of next portal
                CurrentPortal = Rooms.back();
                Rooms.pop_back();
                auto ent = VM()->GetObjectById(CurrentPortal.ObjectID);
                State = VMRoutingFrameState::ROOM_PORTAL;
                if (!PushEntryPoint(15, ent)) //15 is portal function
                    SoftFail(VMRouteFailCode::NoRoomRoute, nullptr); //could not execute portal function
                return VMPrimitiveExitCode::CONTINUE;
            }

            //if we're here, room route is OK. start routing to a destination.
            if (Choices == nullptr) {
                //perform slot parse.
                if (Slot == nullptr) {
                    HardFail(VMRouteFailCode::Unknown, nullptr);
                    return VMPrimitiveExitCode::CONTINUE; //this should never happen. If it does, someone has used the routing system incorrectly.
                }

                VMSlotParser parser(Slot);

                Choices.reset(new std::vector<VMFindLocationResult>(parser.FindAvaliableLocations(Target, VM()->Context, avatar)));
                if (Choices->size() == 0) {
                    HardFail(parser.FailCode, parser.Blocker);
                    return VMPrimitiveExitCode::CONTINUE;
                } else {
                    CurRoute = std::make_shared<VMFindLocationResult>((*Choices)[0]);
                    Choices->erase(Choices->begin());
                }
            }

            if (CurRoute == nullptr) {
                HardFail(VMRouteFailCode::NoValidGoals, nullptr);
                return VMPrimitiveExitCode::CONTINUE;
            }

            //do we need to sit in a seat? it should take over.
            if (CurRoute->Chair != nullptr) {
                if (!AttemptedChair) {
                    AttemptedChair = true;
                    if (PushEntryPoint(26, CurRoute->Chair)) return VMPrimitiveExitCode::CONTINUE;
                    else {
                        SoftFail(VMRouteFailCode::CantSit, nullptr);
                        return VMPrimitiveExitCode::CONTINUE;
                    }
                } else {
                    if (Thread->LastStackExitCode == VMPrimitiveExitCode::RETURN_TRUE) return VMPrimitiveExitCode::RETURN_TRUE;
                    else {
                        SoftFail(VMRouteFailCode::CantSit, nullptr);
                        return VMPrimitiveExitCode::CONTINUE;
                    }
                }
            }

            //If we are sitting, and the target is not this seat we need to call the stand function on the object we are contained within.
            if (avatar->GetPersonData(VMPersonDataVariable::Posture) == 1) {
                State = VMRoutingFrameState::STAND_FUNC;
                //push it onto our stack, except now the portal owns our soul! when we are returned to we can evaluate the result and determine if the route failed.
                auto chair = Caller->Container;

                if (chair == nullptr) avatar->SetPersonData(VMPersonDataVariable::Posture, 0); //we're sitting, but are not bound to a chair. Just instantly get up..
                else {
                    if (!PushEntryPoint(27, chair)) //27 is stand. TODO: set up an enum for these
                        HardFail(VMRouteFailCode::CantStand, nullptr);
                    return VMPrimitiveExitCode::CONTINUE;
                }
            }

            //no chair, we just need to walk to the spot. Start the within-room routing.
            if (WalkTo == nullptr) {
                if (!AttemptWalk()) {
                    SoftFail(VMRouteFailCode::NoPath, nullptr);
                    return VMPrimitiveExitCode::CONTINUE;
                }
            }
            if (State != VMRoutingFrameState::TURN_ONLY) BeginWalk();

            return VMPrimitiveExitCode::CONTINUE;
        }
        case VMRoutingFrameState::FAILED:
            return VMPrimitiveExitCode::RETURN_FALSE;
        case VMRoutingFrameState::TURN_ONLY:
            return (EndWalk()) ? VMPrimitiveExitCode::RETURN_TRUE : VMPrimitiveExitCode::CONTINUE;
        case VMRoutingFrameState::SHOOED:
            StartWalkAnimation();
            State = VMRoutingFrameState::WALKING;
            return VMPrimitiveExitCode::CONTINUE;
        case VMRoutingFrameState::BEGIN_TURN:
        case VMRoutingFrameState::END_TURN:
            if (avatar->CurrentAnimationState() == nullptr || avatar->CurrentAnimationState()->EndReached) {
                if (State == VMRoutingFrameState::BEGIN_TURN) {
                    State = VMRoutingFrameState::WALKING;
                    WalkDirection = TargetDirection;
                    avatar->RadianDirection((float)TargetDirection);
                    StartWalkAnimation();
                    return VMPrimitiveExitCode::CONTINUE;
                } else {
                    if (!CurRoute->FaceAnywhere) avatar->RadianDirection(CurRoute->RadianDirection);

                    //reset animation, so that we're facing the correct direction afterwards.
                    avatar->Animations.clear();

                    auto& anims = InPool() ? avatar->SwimAnimations : avatar->WalkAnimations;

                    auto animation = FSO::Content::Content::Get()->AvatarAnimations->Get(anims[3] + ".anim");
                    auto state = std::make_shared<VMAnimationState>(animation, false);
                    state->Loop = true;
                    avatar->Animations.push_back(state);

                    return VMPrimitiveExitCode::RETURN_TRUE; //we are here!
                }
            } else {
                avatar->RadianDirection(avatar->RadianDirection() + TurnTweak); //while we're turning, adjust our direction
                return VMPrimitiveExitCode::CONTINUE_NEXT_TICK;
            }
        case VMRoutingFrameState::WALKING: {
            if (WalkTo == nullptr) {
                if (!AttemptWalk()) {
                    SoftFail(VMRouteFailCode::NoPath, nullptr);
                }
                return VMPrimitiveExitCode::CONTINUE;
            }

            if (WalkTo->size() == 0 && MoveTotalFrames - MoveFrames <= 28 && CanPortalTurn()) //7+6+5+4...
            {
                //tail off
                if (Velocity <= 0) Velocity = 1;
                if (Velocity > 1) Velocity--;
            } else {
                //get started
                if (Velocity < 8) Velocity++;
            }

            if (avatar->Animations.size() < 3) StartWalkAnimation();
            avatar->Animations[0]->Weight = (8 - Velocity) / 8.0f;
            avatar->Animations[1]->Weight = (Velocity / 8.0f) * 0.66f;
            avatar->Animations[2]->Weight = (Velocity / 8.0f) * 0.33f;

            MoveFrames += Velocity;
            if (MoveFrames >= MoveTotalFrames) {
                MoveFrames = MoveTotalFrames;
                //move us to the final spot, then attempt an advance.
            }
            //normal sims can move 0.05 units in a frame.

            if (TurnFrames > 0) {
                avatar->RadianDirection((float)(TargetDirection + DirectionUtils::Difference(TargetDirection, WalkDirection) * (TurnFrames / 10.0)));
                TurnFrames--;
            }
            else avatar->RadianDirection((float)TargetDirection);

            auto diff = CurrentWaypoint - PreviousPosition;
            if (MoveTotalFrames != 0) {
                diff.x = (int16_t)((diff.x * MoveFrames) / MoveTotalFrames);
                diff.y = (int16_t)((diff.y * MoveFrames) / MoveTotalFrames);
            }

            auto storedDir = avatar->RadianDirection();
            auto result = Caller->SetPosition(PreviousPosition + diff, FSO::LotView::Model::Direction::NORTH, VM()->Context);
            avatar->RadianDirection(storedDir);
            if (result.Status != VMPlacementError::Success && result.Status != VMPlacementError::CantBeThroughWall) {
                //route failure, either something changed or we hit an avatar
                //on both cases try again, but if we hit an avatar then detect if they have a routing frame and stop us for a bit.
                //we stop the first collider because in most cases they're walking across our walk direction and the problem will go away after a second once they're past.
                //
                //if we need to route around a stopped avatar we add them to our "avatars to consider" set.

                bool routeAround = true;
                VMRoutingFrame* colRoute = nullptr;

                auto colAvatar = dynamic_cast<VMAvatar*>(result.Object);
                if (colAvatar != nullptr) {
                    VMStackFrame* colTopFrame = (colAvatar->Thread != nullptr && colAvatar->Thread->Stack.size() > 0) ? colAvatar->Thread->Stack.back().get() : nullptr;

                    //we already attempted to move around this avatar... if this happens too much give up.
                    if (AvatarsToConsider.count(colAvatar) && --Retries <= 0) {
                        SoftFail(VMRouteFailCode::NoPath, avatar);
                        return VMPrimitiveExitCode::CONTINUE;
                    }

                    if (colTopFrame != nullptr && colTopFrame->IsRoutingFrame()) {
                        colRoute = (VMRoutingFrame*)colTopFrame;
                        routeAround = (colRoute->WaitTime > 0);
                    }
                    else {
                        // FIX(sims) QA3: FreeSO counts a retry when the blocker is not walking (standing in the doorway,
                        // using something there). Without it a Sim blocked by a standing Sim shooed and waited forever.
                        --Retries;
                    }
                    if (routeAround) AvatarsToConsider.insert(colAvatar);
                }

                if (result.Object == nullptr || dynamic_cast<VMGameObject*>(result.Object) != nullptr) {
                    //this should not happen often. An object or other feature has blocked our path due to some change in its position.
                    //repeated occurances indicate that we are stuck in something.
                    //todo: is this safe for the robot lot?
                    if (--Retries <= 0) {
                        SoftFail(VMRouteFailCode::NoPath, avatar);
                        return VMPrimitiveExitCode::CONTINUE;
                    }
                }

                if (routeAround) {
                    auto oldWalk = std::make_shared<std::list<Point>>(*WalkTo);
                    if (AttemptWalk()) return VMPrimitiveExitCode::CONTINUE;
                    else {
                        //failed to walk around the object
                        if (colAvatar != nullptr) {
                            WalkTo = oldWalk;
                            //if they're a person, shoo them away.
                            //if they're in a routing frame we can push the tree directly onto their stack
                            //otherwise we push an interaction and just hope they move (todo: does tso do this?)

                            //DO NOT push tree if:
                            // - sim is already being shooed. (the parent of the top routing frame is in state "SHOOED" or shoo interaction is present)
                            // - we are being shooed.
                            // - sim is waiting on someone they just shooed. (presumably can move out of our way once they finish waiting)
                            //instead just wait a small duration and try again later.

                            //cases where we cannot continue moving increase the retry count. if this is greater than RETRY_COUNT then we fail.
                            //not sure how the original game works.

                            if (CanShooAvatar(colAvatar)) {
                                AvatarsToConsider.erase(colAvatar);
                                auto group = VM()->Context->CreateObjectInstance(GOTO_GUID, LotTilePos(result.Object->Position()), FSO::LotView::Model::Direction::NORTH, false);
                                VMEntity* callee = (group == nullptr || group->Objects.empty()) ? nullptr : group->Objects[0];
                                if (callee == nullptr) {
                                    WalkInterrupt(60);
                                } else if (colRoute != nullptr) {
                                    colRoute->State = VMRoutingFrameState::SHOOED;
                                    colRoute->WalkTo = nullptr;
                                    colRoute->AvatarsToConsider.insert(avatar); //just to make sure they don't try route through us.

                                    auto tree = callee->GetBHAVWithOwner(SHOO_TREE, VM()->Context);
                                    if (tree != nullptr) {
                                        auto blockerThread = result.Object->Thread.get();
                                        auto before = blockerThread->Stack.size();
                                        blockerThread->ExecuteSubRoutine(colRoute, tree->bhav, tree->owner, Primitives::VMSubRoutineOperand());
                                        // FIX(sims): FreeSO — the shoo tree runs on the Destination object. FreeSims left the
                                        // blocked route's callee/stack object in the frame, so 'Move out of way' read the routing slot
                                        // of whatever the blocker was walking to (null ref / missing key -> the blocker was reset).
                                        if (blockerThread->Stack.size() > before) {
                                            auto frame = blockerThread->Stack.back();
                                            frame->StackObject = callee;
                                            frame->Callee = callee;
                                        }
                                    }

                                    WalkInterrupt(60);
                                } else {
                                    callee->PushUserInteraction(SHOO_INTERACTION, result.Object, VM()->Context);
                                    WalkInterrupt(60);
                                }
                            } else {
                                WalkInterrupt(60); //wait for a little while, they're moving out of the way.
                            }
                            return VMPrimitiveExitCode::CONTINUE;
                        }
                        SoftFail(VMRouteFailCode::DestTileOccupied, result.Object);
                        return VMPrimitiveExitCode::CONTINUE;
                    }
                } else {
                    WalkInterrupt(30);
                    return VMPrimitiveExitCode::CONTINUE;
                }
            }
            float t = (MoveTotalFrames == 0) ? 1.0f : MoveFrames / (float)MoveTotalFrames;
            Caller->VisualPosition(st::Vector3::Lerp(PreviousPosition.ToVector3(), CurrentWaypoint.ToVector3(), t));

            float tv = (MoveTotalFrames == 0) ? 0.0f : Velocity / (float)MoveTotalFrames;
            auto velocity = st::Vector3::Lerp(PreviousPosition.ToVector3(), CurrentWaypoint.ToVector3(), tv) - PreviousPosition.ToVector3();
            velocity.Z = 0;
            avatar->Velocity = velocity;

            if (MoveTotalFrames == MoveFrames) {
                MoveTotalFrames = 0;
                while (MoveTotalFrames == 0) {
                    auto remains = AdvanceWaypoint();
                    if (!remains) {
                        MoveTotalFrames = -1;
                        if (EndWalk()) return VMPrimitiveExitCode::RETURN_TRUE;
                    }
                }
            }
            return VMPrimitiveExitCode::CONTINUE_NEXT_TICK;
        }
    }
    return VMPrimitiveExitCode::GOTO_FALSE; //???
}

bool VMRoutingFrame::CanShooAvatar(VMAvatar* avatar) {
    VMRoutingFrame* topRoute = nullptr;
    //look for top frame
    if (avatar->Thread != nullptr) {
        for (int i = (int)avatar->Thread->Stack.size() - 1; i >= 0; i--) {
            auto frame = avatar->Thread->Stack[i].get();
            if (frame->IsRoutingFrame()) {
                topRoute = (VMRoutingFrame*)frame;
            }
        }
    }

    //check both the top route frame its parent for ones postponed by shooing,
    //as the top frame will most likely be walking as part of the shoo, and the postponed frame will be the parent.
    //check the parent too just in case they got shooed this frame by another avatar
    if (topRoute != nullptr && (topRoute->State == VMRoutingFrameState::SHOOED || (topRoute->ParentRoute != nullptr && topRoute->ParentRoute->State == VMRoutingFrameState::SHOOED)))
        return false;

    //check if the shoo interaction is present on their action queue. callee GUID (destination obj) and
    //interaction id must match.
    if (avatar->Thread != nullptr)
        for (auto& x : avatar->Thread->Queue)
            if (x->Callee != nullptr && x->Callee->Object->GUID == GOTO_GUID && x->InteractionNumber == SHOO_INTERACTION) return false;

    return true;
}

void VMRoutingFrame::WalkInterrupt(int32_t waitTime) {
    if (State == VMRoutingFrameState::WALKING) {
        //only wait if we're walking
        WaitTime = std::max(waitTime, WaitTime);
    }
}

VMRoutingFrame* VMRoutingFrame::GetParentFrame() {
    //look for parent frame
    for (int i = (int)Thread->Stack.size() - 2; i >= 0; i--) {
        auto frame = Thread->Stack[i].get();
        if (frame->IsRoutingFrame()) {
            return (VMRoutingFrame*)frame;
        }
    }
    return nullptr;
}

bool VMRoutingFrame::CanPortalTurn() {
    auto rf = ParentRoute;
    if (rf == nullptr) rf = this;
    return (rf->State != VMRoutingFrameState::ROOM_PORTAL || rf->PortalTurns++ == 0);
}

void VMRoutingFrame::BeginWalk() { //faces the avatar towards the initial walk direction and begins walking.
    WalkDirection = Caller->RadianDirection();
    auto directionDiff = DirectionUtils::Difference(Caller->RadianDirection(), TargetDirection);

    bool hardStart = CanPortalTurn();
    Velocity = (hardStart) ? 0 : 8;

    if (hardStart && Turn(directionDiff)) {
        State = VMRoutingFrameState::BEGIN_TURN;
    } else {
        State = VMRoutingFrameState::WALKING;
        Caller->RadianDirection((float)TargetDirection);
        StartWalkAnimation();
    }
}

bool VMRoutingFrame::EndWalk() { //returns true if we should exit immediately.
    auto avatar = (VMAvatar*)Caller;
    WalkDirection = Caller->RadianDirection();
    TargetDirection = CurRoute->RadianDirection;

    avatar->SetPersonData(VMPersonDataVariable::RouteEntryFlags, (int16_t)CurRoute->RouteEntryFlags);

    auto directionDiff = DirectionUtils::Difference(Caller->RadianDirection(), TargetDirection);
    if (!CurRoute->FaceAnywhere && CanPortalTurn() && Turn(directionDiff)) {
        State = VMRoutingFrameState::END_TURN;
    } else {
        if (!CurRoute->FaceAnywhere) avatar->RadianDirection(CurRoute->RadianDirection);
        return true; //we are here!
    }
    return false;
}

bool VMRoutingFrame::Turn(double directionDiff) {
    auto obj = (VMAvatar*)Caller;
    int off = (directionDiff > 0) ? 0 : 1;
    auto absDiff = std::abs(directionDiff);
    auto& anims = (InPool()) ? obj->SwimAnimations : obj->WalkAnimations;

    std::string animName;
    if (absDiff >= M_PI - 0.01) //full 180 turn
    {
        animName = anims[4 + off];
        TurnTweak = 0;
    } else if (absDiff >= (M_PI / 2) - 0.01) //>=90 degree turn
    {
        animName = anims[6 + off];
        TurnTweak = (float)(absDiff - (M_PI / 2));
    } else if (absDiff >= (M_PI / 4) - 0.01) //>=45 degree turn
    {
        animName = anims[8 + off];
        TurnTweak = (float)(absDiff - (M_PI / 4));
    } else {
        //turning animation not needed for small rotations!
        return false;
    }

    obj->Animations.clear();
    auto anim = PlayAnim(animName, obj);
    if (WalkStyle() == 1) anim->Speed = 2.0f;
    auto frames = anim->Anim->NumFrames();
    TurnTweak /= (frames == 0) ? 1.0f : frames / anim->Speed;
    if (off == 1) TurnTweak = -TurnTweak;
    return true;
}

void VMRoutingFrame::StartWalkAnimation() {
    auto obj = (VMAvatar*)Caller;
    auto pool = InPool();
    auto& anims = (pool) ? obj->SwimAnimations : obj->WalkAnimations;

    if (obj->Animations.size() == 3 &&
        obj->Animations[0]->Anim->Name == anims[3] &&
        obj->Animations[1]->Anim->Name == anims[(WalkStyle() == 1) ? 21 : 20]) return; //hacky check to test if we're already doing a walking animation.

    //we set up a very specific collection of animations.
    //The original game gets its walk animation by confusingly combining two of the walk animations and running them at 1.5x speed.
    //We also want to store the standing pose in the first animation slot so that we can blend into and out of it with velocity.

    obj->Animations.clear();
    auto anim = PlayAnim(anims[3], obj); //stand animation (TODO: what about swimming?)
    anim->Weight = 0.0f;
    anim->Loop = true;
    anim = PlayAnim(anims[(WalkStyle() == 1) ? 21 : 20], obj); //Run full:Walk Full
    anim->Weight = 0.66f;
    anim->Speed = 1.5f;
    anim->Loop = true;
    anim = PlayAnim(anims[(WalkStyle() == 1 || pool) ? 21 : (obj->IsPet() ? 20 : 25)], obj); //Run full:Walk Half
    anim->Weight = 0.33f;
    anim->Speed = 1.5f;
    anim->Loop = true;
}

VMAnimationState* VMRoutingFrame::PlayAnim(const std::string& name, VMAvatar* avatar) {
    auto animation = FSO::Content::Content::Get()->AvatarAnimations->Get(name + ".anim");
    // C#: new VMAnimationState(null) threw a NullReferenceException (caught by the thread -> reset); don't crash.
    if (animation == nullptr) throw std::runtime_error("Object reference not set to an instance of an object. (missing animation " + name + ")");
    auto state = std::make_shared<VMAnimationState>(animation, false);
    avatar->Animations.push_back(state);
    return state.get();
}

bool VMRoutingFrame::AdvanceWaypoint() {
    if (WalkTo->size() == 0) return false;

    auto point = WalkTo->front();
    WalkTo->pop_front();
    if (WalkTo->size() > 0) {
        CurrentWaypoint = LotTilePos((int16_t)point.X, (int16_t)point.Y, Caller->Position().Level);
    }
    else CurrentWaypoint = CurRoute->Position; //go directly to position at last
    PreviousPosition = Caller->Position();
    MoveFrames = 0;

    MoveTotalFrames = ((LotTilePos::Distance(CurrentWaypoint, Caller->Position()) * 20) / 2);
    if (((VMAvatar*)Caller)->IsPet() || InPool()) MoveTotalFrames *= 2;
    MoveTotalFrames = std::max<int32_t>(1, MoveTotalFrames / ((WalkStyle() == 1) ? 3 : 1));

    WalkDirection = Caller->RadianDirection();
    TargetDirection = std::atan2((double)(CurrentWaypoint.x - Caller->Position().x), (double)(Caller->Position().y - CurrentWaypoint.y)); //y+ as north. x+ is -90 degrees.
    TurnFrames = 10;
    return true;
}

}}}
