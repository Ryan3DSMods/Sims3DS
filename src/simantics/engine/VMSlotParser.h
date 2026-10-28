/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Engine.VMSlotParser / VMFindLocationResult — resolves a SLOT into candidate routing goals.
#pragma once
#include <cstdint>
#include <vector>
#include <cmath>
#include "core/math.h"
#include "simantics/_files_contract.h"
#include "world/model/LotTilePos.h"
#include "simantics/model/Routing/VMRouteFailCode.h"
namespace FSO { namespace SimAntics {
class VMEntity; class VMContext;
namespace Engine {

class VMFindLocationResult {
public:
    float RadianDirection = 0;
    FSO::LotView::Model::LotTilePos Position;
    double Score = 0;
    bool FaceAnywhere = false;
    VMEntity* Chair = nullptr;
    FSO::Files::Formats::IFF::Chunks::SLOTFlags RouteEntryFlags = FSO::Files::Formats::IFF::Chunks::SLOTFlags::NORTH;
    VMFindLocationResult() {}
};

class VMSlotParser {
public:
    static constexpr double ANGLE_ERROR = -0.1f;
    std::vector<VMFindLocationResult> Results;
    Model::Routing::VMRouteFailCode FailCode = Model::Routing::VMRouteFailCode::NoValidGoals;
    VMEntity* Blocker = nullptr;
    FSO::Files::Formats::IFF::Chunks::SLOTItem* Slot;

    explicit VMSlotParser(FSO::Files::Formats::IFF::Chunks::SLOTItem* slot);

    // TODO: float values may desync if devices are not both x86 or using a different C# library.
    // Might need to replace with fixed point library for position and rotation
    /// This method will find all the avaliable locations within the criteria ordered by proximity to the optimal proximity
    /// External functions can then decide which is most desirable. E.g. the nearest slot to the object may be the longest route if
    /// its in another room.
    std::vector<VMFindLocationResult> FindAvaliableLocations(VMEntity* obj, VMContext* context, VMEntity* caller);

    /// Returns which search direction (n/s/e/w/ne/nw/se/sw) a target is in relation to an object. (absolute)
    static FSO::Files::Formats::IFF::Chunks::SLOTFlags GetSearchDirection(st::Vector2 pos1, st::Vector2 pos2, float rotShift);
    static FSO::Files::Formats::IFF::Chunks::SLOTFlags RadianToFlags(double rad);
    static double GetDirectionTo(st::Vector2 pos1, st::Vector2 pos2);

private:
    static const Model::Routing::VMRouteFailCode FailPrio[4];
    FSO::Files::Formats::IFF::Chunks::SLOTFlags Flags;
    int32_t MinProximity;
    int32_t MaxProximity;
    int32_t DesiredProximity;
    bool OnlySit() const { return (Slot->Sitting > 0 && Slot->Standing == 0); }
    void VerifyAndAddLocation(VMEntity* obj, st::Vector2 pos, st::Vector2 center, FSO::Files::Formats::IFF::Chunks::SLOTFlags entryFlags, double score, VMContext* context, VMEntity* caller, float facingDir);
    void SetFail(Model::Routing::VMRouteFailCode code, VMEntity* blocker);
    static double FlagsAsRad(FSO::Files::Formats::IFF::Chunks::SLOTFlags dir);
};
}}}
