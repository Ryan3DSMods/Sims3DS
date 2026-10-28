/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Engine.VMSlotParser
#include "simantics/engine/VMSlotParser.h"
#include "simantics/engine/VMRoutingFrame.h"
#include "simantics/engine/VMThread.h"
#include "simantics/model/VMSetToNextCache.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMGameObject.h"
#include "simantics/entities/VMMultitileGroup.h"
#include "simantics/VMContext.h"
#include "simantics/VMArchitecture.h"
#include "simantics/VM.h"
#include "common/utils/DirectionUtils.h"
#include <algorithm>
#include <cfloat>
#include <limits>
namespace FSO { namespace SimAntics { namespace Engine {
using namespace FSO::Files::Formats::IFF::Chunks;
using namespace FSO::SimAntics::Model;
using namespace FSO::SimAntics::Model::Routing;
using FSO::LotView::Model::LotTilePos;
using FSO::Common::Utils::DirectionUtils;
using st::Vector2;
using st::Vector3;

const VMRouteFailCode VMSlotParser::FailPrio[4] = {
    VMRouteFailCode::NoValidGoals,
    VMRouteFailCode::NoChair,
    VMRouteFailCode::DestTileOccupiedPerson,
    VMRouteFailCode::DestTileOccupied,
};

VMSlotParser::VMSlotParser(SLOTItem* slot) {
    Slot = slot;
    Flags = slot->Rsflags;
    MinProximity = slot->MinProximity;
    MaxProximity = slot->MaxProximity;
    DesiredProximity = slot->OptimalProximity;

    if (MaxProximity == 0) { MaxProximity = MinProximity; }
    if (DesiredProximity == 0) { DesiredProximity = MinProximity; }
}

std::vector<VMFindLocationResult> VMSlotParser::FindAvaliableLocations(VMEntity* obj, VMContext* context, VMEntity* caller) {
    /**
     * Start at min proximity and circle around the object to find the avaliable locations.
     * Then pick the one nearest to the optimal value
     */

    /**
     * ------ MAJOR TODO: ------
     * Avoid vector math at all costs! Small differences in hardware could cause desyncs.
     * This really goes for all areas of the SimAntics engine, but here it's particularly bad.
     */
    Vector2 center;
    if (OnlySit()) FailCode = VMRouteFailCode::NoChair;

    // if we need to use the average location of an object group, it needs to be calculated.
    if (((Flags & SLOTFlags::UseAverageObjectLocation) > 0) && (obj->MultitileGroup->MultiTile)) {
        center = Vector2(0, 0);
        auto& objs = obj->MultitileGroup->Objects;
        for (size_t i = 0; i < objs.size(); i++) {
            center += Vector2(objs[i]->Position().x / 16.0f, objs[i]->Position().y / 16.0f);
        }
        center /= (float)objs.size();
    } else center = Vector2(obj->Position().x / 16.0f, obj->Position().y / 16.0f);

    //add offset of slot if it exists. must be rotated to be relative to object
    auto rotOff = st::Transform(Slot->Offset, st::Matrix::CreateRotationZ(obj->RadianDirection()));
    auto circleCtr = Vector2(center.X + rotOff.X / 16, center.Y + rotOff.Y / 16);

    uint16_t room = context->VM->Context->GetRoomAt(obj->Position());
    Results.clear();

    if ((Flags & SLOTFlags::SnapToDirection) > 0) { //snap to the specified direction, on the specified point.
        // double baseRot;
        if ((int)Slot->Facing > (int)SLOTFacing::FaceAwayFromObject) {
            // bit of a legacy thing here. Facing field did not use to exist,
            // which is why SnapToDirection was hacked to use the directional flags.
            // now that it exists, it is used instead, to encode the same information...
            // just transform back into old format.
            Flags |= (SLOTFlags)(1 << (int)Slot->Facing);
        } else {
            if (((int)Flags & 255) == 0) Flags |= SLOTFlags::NORTH;
        }

        auto flagRot = DirectionUtils::PosMod(obj->RadianDirection() + FlagsAsRad(Flags), M_PI * 2);
        if (flagRot > M_PI) flagRot -= M_PI * 2;

        VerifyAndAddLocation(obj, circleCtr, center, Flags, DBL_MAX, context, caller, (float)flagRot);
        return Results;
    } else {
        if (((int)Flags & 255) == 0 || Slot->Offset != Vector3()) {
            //exact position
            //Flags |= (SLOTFlags)255;

            // special case, walk directly to point.
            VerifyAndAddLocation(obj, circleCtr, center, Flags, DBL_MAX, context, caller, std::numeric_limits<float>::quiet_NaN());
            return Results;
        }
        auto maxScore = std::max(DesiredProximity - MinProximity, MaxProximity - DesiredProximity) + (LotTilePos::Distance(obj->Position(), caller->Position()) + MaxProximity) / 3 + 2;
        auto ignoreRooms = (Flags & SLOTFlags::IgnoreRooms) > 0;

        auto resolutionBound = (MaxProximity / Slot->Resolution) * Slot->Resolution;

        for (int x = -resolutionBound; x <= resolutionBound; x += Slot->Resolution) {
            for (int y = -resolutionBound; y <= resolutionBound; y += Slot->Resolution) {
                auto pos = Vector2(circleCtr.X + x / 16.0f, circleCtr.Y + y / 16.0f);
                double distance = std::sqrt((double)(x * x + y * y));
                if (distance >= MinProximity - 0.01 && distance <= MaxProximity + 0.01 && (ignoreRooms || context->VM->Context->GetRoomAt(LotTilePos((int16_t)std::nearbyint(pos.X * 16), (int16_t)std::nearbyint(pos.Y * 16), obj->Position().Level)) == room)) //slot is within proximity
                {
                    auto routeEntryFlags = (SLOTFlags)(GetSearchDirection(circleCtr, pos, obj->RadianDirection()) & Flags); //the route needs to know what conditions it fulfilled
                    if (routeEntryFlags > 0) //within search location
                    {
                        double baseScore = ((maxScore - std::abs(DesiredProximity - distance)) + context->VM->Context->NextRandom(1024) / 1024.0f);
                        VerifyAndAddLocation(obj, pos, center, routeEntryFlags, baseScore, context, caller, std::numeric_limits<float>::quiet_NaN());
                    }
                }
            }
        }
    }
    /** Sort by how close they are to desired proximity **/

    if (Results.size() > 1) std::stable_sort(Results.begin(), Results.end(), [](const VMFindLocationResult& a, const VMFindLocationResult& b) { return -a.Score < -b.Score; }); //avoid sort because it acts incredibly unusually
    if (Results.size() > 0) FailCode = VMRouteFailCode::Success;
    return Results;
}

// C# Math.Round uses banker's rounding (MidpointRounding.ToEven); std::nearbyint does the same in the default FP mode.
static int16_t RoundEven(double v) { return (int16_t)std::nearbyint(v); }

void VMSlotParser::VerifyAndAddLocation(VMEntity* obj, Vector2 pos, Vector2 /*center*/, SLOTFlags entryFlags, double score, VMContext* context, VMEntity* caller, float facingDir) {
    //note: verification is not performed if snap target slot is enabled.
    auto tpos = LotTilePos(RoundEven(pos.X * 16), RoundEven(pos.Y * 16), obj->Position().Level);

    if (context->IsOutOfBounds(tpos)) return;

    score -= LotTilePos::Distance(tpos, caller->Position()) / 3.0;

    if (Slot->SnapTargetSlot < 0 && context->Architecture()->RaycastWall(st::Point((int)pos.X, (int)pos.Y), st::Point(obj->Position().TileX(), obj->Position().TileY()), obj->Position().Level)) {
        SetFail(VMRouteFailCode::WallInWay, nullptr);
        return;
    }

    bool faceAnywhere = false;
    if (std::isnan(facingDir)) {
        auto obj3P = obj->Position().ToVector3();
        auto objP = Vector2(obj3P.X, obj3P.Y);
        switch (Slot->Facing) {
            case SLOTFacing::FaceTowardsObject:
                facingDir = (float)GetDirectionTo(pos, objP); break;
            case SLOTFacing::FaceAwayFromObject:
                facingDir = (float)GetDirectionTo(objP, pos); break;
            case SLOTFacing::FaceAnywhere:
                faceAnywhere = true;
                facingDir = 0.0f; break;
            default: {
                int intDir = (int)std::nearbyint(std::log2((double)(int)obj->Direction()));
                auto rotatedF = ((int)Slot->Facing + intDir) % 8;
                facingDir = (float)(((int)rotatedF > 4) ? ((double)rotatedF * M_PI / 4.0) : (((double)rotatedF - 8.0) * M_PI / 4.0)); break;
            }
        }
    }

    VMEntity* chair = nullptr;
    bool avatarInWay = false; // TS1(sims): FreeSO
    if (Slot->SnapTargetSlot < 0) {
        auto solid = caller->PositionValid(tpos, FSO::LotView::Model::Direction::NORTH, context);
        if (solid.Status != VMPlacementError::Success) {
            if (solid.Object != nullptr && dynamic_cast<VMGameObject*>(solid.Object) != nullptr) {
                if (Slot->Sitting > 0 && solid.Object->EntryPoints[26].ActionFunction != 0) {
                    chair = solid.Object;
                } else {
                    SetFail(VMRouteFailCode::DestTileOccupied, solid.Object);
                    return;
                }
            } else if (solid.Object != nullptr) {
                avatarInWay = true; // TS1(sims): FreeSO
            }
        }

        // TS1(sims): FreeSO — "prefer not standing in front of a door". Without it, goals in door tiles (dancing
        // next to a stereo by the front door) left two Sims standing in the same doorway, each blocking the other's
        // only route for the rest of the run.
        {
            auto objsAt = context->SetToNextCache->GetObjectsAt(tpos);
            if (objsAt != nullptr) for (auto x : *objsAt)
                if (((int32_t)x->GetValue(VMStackObjectVariable::FlagField2) & (int32_t)VMEntityFlags2::ArchitectualDoor) > 0) { avatarInWay = true; break; }
        }

        if (chair != nullptr && (std::abs(DirectionUtils::Difference(chair->RadianDirection(), facingDir)) > M_PI / 4))
            return; //not a valid goal.
        if (chair == nullptr && OnlySit()) return;
    }

    VMFindLocationResult r;
    r.Position = LotTilePos(RoundEven(pos.X * 16), RoundEven(pos.Y * 16), obj->Position().Level);
    r.Score = score * ((chair != nullptr) ? Slot->Sitting : Slot->Standing); //todo: prefer closer?
    r.Chair = chair;
    if (Slot->SnapTargetSlot < 0) {
        //if an avatar is in or going to our destination positon, we this spot becomes low priority as getting into it will require a shoo.
        if (!avatarInWay) {
            for (auto avatar : context->SetToNextCache->Avatars) {
                if (avatar == caller || avatar->Thread == nullptr) continue;
                //search for routing frame. is its destination the same as ours?
                bool intersects = false;
                for (auto& f : avatar->Thread->Stack)
                    if (f->IsRoutingFrame() && ((VMRoutingFrame*)f.get())->IntersectsOurDestination(r)) { intersects = true; break; }
                if (intersects) { r.Score = r.Score / 100000; break; }
            }
        } else r.Score = r.Score / 100000;
    }
    r.RadianDirection = facingDir;
    r.Chair = chair;
    r.FaceAnywhere = faceAnywhere;
    r.RouteEntryFlags = entryFlags;
    Results.push_back(r);
}

void VMSlotParser::SetFail(VMRouteFailCode code, VMEntity* blocker) {
    auto indexOf = [](VMRouteFailCode c) { for (int i = 0; i < 4; i++) if (FailPrio[i] == c) return i; return -1; };
    if (indexOf(code) > indexOf(FailCode)) {
        FailCode = code;
        Blocker = blocker;
    }
}

double VMSlotParser::FlagsAsRad(SLOTFlags dir) {
    double value = std::nearbyint(std::log2((double)((int)dir & 255))) * M_PI / 4;
    //if (value > Math.PI) value -= 2*Math.PI;
    return value;
}

SLOTFlags VMSlotParser::GetSearchDirection(Vector2 pos1, Vector2 pos2, float rotShift) {
    double dir = std::atan2(std::floor(pos2.X) - std::floor(pos1.X), std::floor(pos1.Y) - std::floor(pos2.Y)) * (180.0 / M_PI);
    dir = DirectionUtils::NormalizeDegrees(dir - rotShift * (180.0 / M_PI));
    SLOTFlags result = (SLOTFlags)0;
    if (dir >= -45.0 - ANGLE_ERROR && dir <= 45.0 + ANGLE_ERROR) result |= SLOTFlags::NORTH;
    if (dir >= 0.0 - ANGLE_ERROR && dir <= 90.0 + ANGLE_ERROR) result |= SLOTFlags::NORTH_EAST;
    if (dir >= 45.0 - ANGLE_ERROR && dir <= 135.0 + ANGLE_ERROR) result |= SLOTFlags::EAST;
    if ((dir >= 90.0 - ANGLE_ERROR && dir <= 180.0 + ANGLE_ERROR) || (dir <= -180.0 + ANGLE_ERROR)) result |= SLOTFlags::SOUTH_EAST;
    if (dir >= 135.0 - ANGLE_ERROR || dir <= -135.0 + ANGLE_ERROR) result |= SLOTFlags::SOUTH;
    if ((dir >= -180.0 - ANGLE_ERROR && dir <= -135.0 + ANGLE_ERROR) || (dir >= 180.0 - ANGLE_ERROR)) result |= SLOTFlags::SOUTH_WEST;
    if (dir >= -135.0 - ANGLE_ERROR && dir <= -45.0 + ANGLE_ERROR) result |= SLOTFlags::WEST;
    if (dir >= -90.0 - ANGLE_ERROR && dir <= 0.0 + ANGLE_ERROR) result |= SLOTFlags::NORTH_WEST;
    return result;
}

SLOTFlags VMSlotParser::RadianToFlags(double rad) {
    int result = (int)(std::nearbyint((rad / (M_PI * 2)) * 8) + 80) % 8; //for best results, make sure rad is >-pi and <pi
    return (SLOTFlags)(1 << result);
}

double VMSlotParser::GetDirectionTo(Vector2 pos1, Vector2 pos2) {
    return std::atan2(pos2.X - pos1.X, -(pos2.Y - pos1.Y));
}
}}}
