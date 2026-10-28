// FSO.SimAntics.Primitives.VMTS1InventoryOperations (FreeSO tso.simantics/Primitives/VMTS1InventoryOperations.cs)
/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// TS1(sims): The Sims 1 primitive 51 "Manage Inventory" (Behavior.iff STR#139; Hot Date and later). FreeSims has
// no TS1 inventory; this is a translation of FreeSO's VMTS1InventoryOperations (MPL-2.0, FreeSims' successor), which
// keeps per-neighbour token lists in the neighbourhood's NGBH chunk: Hot Date gifts and date data, Vacation souvenirs,
// prize tokens and the disease, Unleashed pet show tokens/collars/seeds, Superstar awards and fame tokens, Makin'
// Magic ingredients, charms, MagiCoins and quest tokens, and the "sim data effects" (motives, clock) a family
// carries between lots.
#pragma once
#include <cstdint>
#include "simantics/engine/VMPrimitiveHandler.h"
#include "simantics/engine/VMPrimitiveOperand.h"

namespace FSO { namespace SimAntics { namespace Primitives {

class VMTS1InventoryOperations : public Engine::VMPrimitiveHandler {
public:
    Engine::VMPrimitiveExitCode Execute(Engine::VMStackFrame* context, Engine::VMPrimitiveOperand* args) override;
};

enum class VMTS1InventoryMode : uint8_t
{
    AddToken = 0, //add
    RemoveToken = 1,
    RemoveTokenAtIndex = 2,
    FindToken = 3, //count in temp0
    SetToNextTokenOfType = 4, //count in temp0. ignores guid.
    Temp0NeighborAsAutofollow = 5,
    Temp0NeighborAsFollowHome = 6,
    UnusedRemoveTimeData = 8
};

class VMTS1InventoryOperationsOperand : public Engine::VMPrimitiveOperand {
public:
    VMTS1InventoryMode Mode = VMTS1InventoryMode::AddToken;
    uint8_t TokenType = 0; //token type
    uint8_t Flags = 0; //flags
    //1 - unknown (regularly set for find token, ONLY this is set for add token and remove)
    //2 - count in temp
    //4-8 - temp[num] := count (set to next related)
    //16 - found index into temp (regularly set for find token)
    //32 - ??
    //64 - ??
    //128 - index in temp (set to next)
    uint8_t Flags2 = 0;
    //1-2 - temp[num] := index (1 regularly set, not in set to next? would be index in temp 0)
    //4 - (set in find token)
    //8 - (very regularly set)
    //16 - ??
    //32 - object in temp 4
    //64 - ??
    //128 - mode 8 remove time tokens?
    uint32_t GUID = 0;

    int32_t CountTemp() const { return (Flags >> 2) & 3; }
    int32_t IndexTemp() const { return Flags2 & 3; }
    bool NextIndexIntoTemp() const { return (Flags & 0x80) > 0; }
    bool FoundIndexIntoTemp() const { return (Flags & 0x10) > 0; }
    bool CountInTemp() const { return (Flags & 2) > 0; }
    void CountInTemp(bool value) { Flags &= (uint8_t)~2; if (value) Flags |= 2; }
    bool UseObjectInTemp4() const { return (Flags2 & 32) > 0; }
    void UseObjectInTemp4(bool value) { Flags2 &= (uint8_t)~32; if (value) Flags2 |= 32; }

    void Read(const uint8_t* bytes) override;
    void Write(uint8_t* bytes) override;
};

}}}
