/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Model.VMTuningCache
#include "simantics/model/VMTuningCache.h"
#include "simantics/model/DynamicTuning.h"
#include "simantics/VM.h"
#include "simantics/model/TSOPlatform/VMTSOLotState.h"
namespace FSO { namespace SimAntics { namespace Model {
void VMTuningCache::UpdateTuning(VM* vm) {
    if (vm->TS1) return;
    if (vm->Tuning == nullptr) return;
    auto table = vm->Tuning->GetTable("overfill", vm->TSOState()->PropertyCategory);
    if (table != nullptr) MotiveOverfill = *table;
}
}}}
