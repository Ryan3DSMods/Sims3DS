/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.VMSolidResult / VMPlacementResult (defined in VMContext.cs)
#pragma once
#include "simantics/model/VMPlacementError.h"
namespace FSO { namespace SimAntics {
class VMEntity;
struct VMSolidResult {
    bool Solid = false;
    VMEntity* Chair = nullptr;
};
struct VMPlacementResult {
    Model::VMPlacementError Status = Model::VMPlacementError::Success; //if true, cannot place anywhere.
    VMEntity* Object = nullptr; //Container if above is .Success, Obstacle if above is any failure code.
    VMPlacementResult() {}
    explicit VMPlacementResult(Model::VMPlacementError status) : Status(status), Object(nullptr) {}
    VMPlacementResult(Model::VMPlacementError status, VMEntity* obj) : Status(status), Object(obj) {}
};
}}
