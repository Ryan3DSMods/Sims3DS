/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Primitives.VMTS1InventoryOperations — see VMTS1InventoryOperations.h
// TS1(sims): translated from FreeSO (tso.simantics/Primitives/VMTS1InventoryOperations.cs), MPL-2.0.
#include "simantics/primitives/VMTS1InventoryOperations.h"
#include <algorithm>
#include <cstring>
#include <numeric>
#include "core/io.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMThread.h"
#include "simantics/engine/VMSimanticsException.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/entities/VMGameObject.h"
#include "simantics/model/VMPersonDataVariable.h"
#include "simantics/VM.h"
#include "content/ContentManager.h"
#include "content/TS1NeighbourProvider.h"
#include "files/formats/iff/chunks/OBJD.h"

namespace FSO { namespace SimAntics { namespace Primitives {
using namespace FSO::SimAntics::Engine;
using FSO::Files::Formats::IFF::Chunks::InventoryItem;

VMPrimitiveExitCode VMTS1InventoryOperations::Execute(VMStackFrame* context, VMPrimitiveOperand* args)
{
    //todo: check condition
    auto operand = static_cast<VMTS1InventoryOperationsOperand*>(args);

    //tragic clown magic growth qualify: hasToken, stack object
    //6 17 45

    //magic person a - check inventory (caller??)
    //7 17 45

    //teleport here check: hasToken
    //6 17 13
    //has wand check:
    //7 17 13

    //get 35 coins:
    //7 3 9
    //get wand, toadsweat:
    //7 1 9

    //spawn purchases test (mode 4)
    //0 180 4
    //5 180 4
    //4 180 4

    //remove token caller get in wagon:
    //1 132 8
    //mode 4 something:
    //1 180 4
    //3 180 4
    //
    //weird remove request with wrong type (0 instead of 8):
    //0x82, 0x09

    //NOTES:
    // - has token and remove token currently ignore the type, and only look for entries with the given GUID.
    //   I haven't seen inventory items appear under multiple types, so this shouldn't affect anything right now
    //   ...but there is probably a flag for it!

    //$FamilyAssets $NameAttrib $NeighborLocal $FameTitleLocal

    //=== from exe ===
    //1. | of count | and index to Temp | . | Returning count to Temp | 0. | starting at index  | stored in Temp | from Stack Object's GUID | with object |
    // (Using Inventory of object in Temp 4)
    //types:
    // SKILL   SOUVENIR    PURCHASE    SIMDATA     DATE    INGREDIENT  MAGIC   GIFT

    auto neighbourhood = FSO::Content::Content::Get()->Neighborhood.get();
    if (neighbourhood == nullptr) return VMPrimitiveExitCode::GOTO_FALSE; // TS1(sims): no neighbourhood loaded
    auto inTarget = (operand->UseObjectInTemp4()) ? context->VM()->GetObjectById(context->Thread->TempRegisters[4]) : context->Caller;
    // C#: `var target = (VMAvatar)(inTarget);` — a game object is an InvalidCastException, null a NullReference below.
    if (inTarget == nullptr) throw VMSimanticsException("NullReferenceException", context);
    auto target = dynamic_cast<VMAvatar*>(inTarget);
    if (target == nullptr) throw VMSimanticsException("InvalidCastException", context);
    auto neighbour = target->GetPersonData(Model::VMPersonDataVariable::NeighborId);
    auto inventory = neighbourhood->GetInventoryByNID(neighbour);
    int32_t count = (operand->CountInTemp()) ? context->Thread->TempRegisters[0] : 1;
    int32_t type = operand->TokenType; //type 0 on find of type indicates "any type".
    int32_t index = context->Thread->TempRegisters[operand->IndexTemp()];

    //type 4: magic town purchasables
    //type 6: vacation purchasables
    uint32_t guid = operand->GUID;
    if (guid == 0) {
        if (context->StackObject == nullptr) throw VMSimanticsException("NullReferenceException", context);
        guid = context->StackObject->Object->OBJ->GUID;
    }
    //var guid = operand.GUID;

    auto findItem = [&](bool anyType) -> InventoryItem* {
        if (inventory == nullptr) return nullptr;
        for (auto& x : *inventory) if (x.GUID == guid && ((anyType && type == 0) || x.Type == type)) return &x;
        return nullptr;
    };
    auto removeItem = [&](InventoryItem* item) {
        for (size_t i = 0; i < inventory->size(); i++) if (&(*inventory)[i] == item) { inventory->erase(inventory->begin() + i); return; }
    };

    switch (operand->Mode)
    {
        case VMTS1InventoryMode::AddToken: {
            inventory = &neighbourhood->InitInventoryForNID(neighbour);
            //if we have an existing item add to that
            auto aitem = findItem(false);

            if (aitem == nullptr) {
                InventoryItem n; n.Count = (uint16_t)count; n.GUID = guid; n.Type = type;
                inventory->push_back(n);
            } else
                aitem->Count += (uint16_t)count;

            return VMPrimitiveExitCode::GOTO_TRUE;
        }
        case VMTS1InventoryMode::RemoveToken: {
            if (inventory == nullptr) return VMPrimitiveExitCode::GOTO_FALSE; //can't remove a token that isn't there
            auto ritem = findItem(true);
            if (ritem == nullptr || (int32_t)ritem->Count < count) return VMPrimitiveExitCode::GOTO_FALSE; //can't remove a token that isn't there
            if (count == -1) count = ritem->Count; //count of -1 means remove all
            ritem->Count -= (uint16_t)count;
            if (ritem->Count == 0) removeItem(ritem);
            //todo: does this write the index?
            return VMPrimitiveExitCode::GOTO_TRUE;
        }
        case VMTS1InventoryMode::RemoveTokenAtIndex: {
            //
            if (inventory == nullptr || index < 0 || index >= (int32_t)inventory->size())
                return VMPrimitiveExitCode::GOTO_FALSE; //can't remove a token that isn't there

            auto ritem = &(*inventory)[(size_t)index];
            if (count == -1) count = ritem->Count; //count of -1 means remove all
            ritem->Count -= (uint16_t)count;
            if (ritem->Count == 0) removeItem(ritem);
            if (operand->NextIndexIntoTemp())
                context->Thread->TempRegisters[operand->IndexTemp()] = (int16_t)(index - 1);
            return VMPrimitiveExitCode::GOTO_TRUE;
        }
        case VMTS1InventoryMode::FindToken: {
            if (inventory == nullptr) return VMPrimitiveExitCode::GOTO_FALSE;
            int32_t foundindex = -1;
            for (size_t i = 0; i < inventory->size(); i++) {
                auto& x = (*inventory)[i];
                if (x.GUID == guid && (type == 0 || x.Type == type)) { foundindex = (int32_t)i; break; }
            }
            if (foundindex == -1) return VMPrimitiveExitCode::GOTO_FALSE;
            auto& items = (*inventory)[(size_t)foundindex];
            auto itemcount = (int16_t)(items.Count);
            context->Thread->TempRegisters[operand->CountTemp()] = itemcount;
            if (operand->FoundIndexIntoTemp()) context->Thread->TempRegisters[operand->IndexTemp()] = (int16_t)foundindex;
            return (itemcount > 0) ? VMPrimitiveExitCode::GOTO_TRUE : VMPrimitiveExitCode::GOTO_FALSE;
            //return (itemcount >= count)? VMPrimitiveExitCode.GOTO_TRUE : VMPrimitiveExitCode.GOTO_FALSE;
        }
        case VMTS1InventoryMode::SetToNextTokenOfType: { //ignores guid
            if (inventory == nullptr) return VMPrimitiveExitCode::GOTO_FALSE;
            int32_t sum = 0;
            int32_t foundindex = -1;
            for (size_t i = 0; i < inventory->size(); i++) {
                auto& x = (*inventory)[i];
                if (x.Type != type) continue;
                sum += x.Count;
                if (foundindex == -1 && (int32_t)i > index) foundindex = (int32_t)i;
            }
            context->Thread->TempRegisters[operand->CountTemp()] = (int16_t)sum;
            if (foundindex == -1) return VMPrimitiveExitCode::GOTO_FALSE;
            if (operand->NextIndexIntoTemp()) context->Thread->TempRegisters[operand->IndexTemp()] = (int16_t)foundindex;
            return VMPrimitiveExitCode::GOTO_TRUE;
        }
        case VMTS1InventoryMode::Temp0NeighborAsAutofollow:
        case VMTS1InventoryMode::Temp0NeighborAsFollowHome: {
            // FIX(sims): FreeSO looked the existing entry up with GUID 0/1 but adds it as GUID 10/11 ("sim data"
            // slots read back by Generic Sims Call 19/20), and replaced it with `count` instead of temp 0. Here the
            // same slot is found and replaced with the neighbour id in temp 0.
            uint32_t slot = (operand->Mode == VMTS1InventoryMode::Temp0NeighborAsAutofollow) ? 10u : 11u;
            inventory = &neighbourhood->InitInventoryForNID(neighbour);
            //if we have an existing item replace it
            InventoryItem* aitem = nullptr;
            for (auto& x : *inventory) if (x.GUID == slot && x.Type == 2) { aitem = &x; break; }

            if (aitem == nullptr) {
                InventoryItem n; n.Count = (uint16_t)context->Thread->TempRegisters[0]; n.GUID = slot; n.Type = 2;
                inventory->push_back(n);
            } else
                aitem->Count = (uint16_t)context->Thread->TempRegisters[0];
            return VMPrimitiveExitCode::GOTO_TRUE;
        }
        default:
            return VMPrimitiveExitCode::GOTO_TRUE;
    }
}

void VMTS1InventoryOperationsOperand::Read(const uint8_t* bytes)
{
    st::Reader io(bytes, OPERAND_SIZE, st::Endian::Little);
    Mode = (VMTS1InventoryMode)io.u8();
    TokenType = io.u8();
    Flags = io.u8();
    Flags2 = io.u8();
    GUID = io.u32();
}

void VMTS1InventoryOperationsOperand::Write(uint8_t* bytes)
{
    bytes[0] = (uint8_t)Mode;
    bytes[1] = TokenType;
    bytes[2] = Flags;
    bytes[3] = Flags2;
    std::memcpy(bytes + 4, &GUID, 4);
}

}}}
