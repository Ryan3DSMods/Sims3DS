/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Engine.Utils.VMMemory
#include <cstdio>
#include "simantics/engine/VMMemory.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMThread.h"
#include "simantics/engine/VMSimanticsException.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/entities/VMMultitileGroup.h"
#include "simantics/model/VMMotive.h"
#include "simantics/model/VMPersonDataVariable.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"
#include "simantics/VMClock.h"
#include "simantics/engine/VMTS1Neighbourhood.h"
#include <algorithm>

namespace FSO { namespace SimAntics { namespace Engine { namespace Utils {
using namespace FSO::SimAntics::Engine::Scopes;
using namespace FSO::SimAntics::Model;
using namespace FSO::Files::Formats::IFF::Chunks;
using FSO::Files::Formats::OTF::OTFTable;

// C# arrays throw IndexOutOfRangeException; std::vector/array do not. Bounds-check every indexed access
// so the thread's catch handler resets the object exactly as in FreeSims.
template<class C> static auto& At(C& c, int idx, VMStackFrame* context) {
    if (idx < 0 || (size_t)idx >= c.size()) throw VMSimanticsException("Index was outside the bounds of the array.", context);
    return c[(size_t)idx];
}

static VMAvatar* AsAvatar(VMEntity* e, VMStackFrame* context) {
    // C# `(VMAvatar)x` throws InvalidCastException on non-avatars (and NullReference on null).
    auto a = dynamic_cast<VMAvatar*>(e);
    if (a == nullptr) throw VMSimanticsException("Specified cast is not valid.", context);
    return a;
}

static VMEntity* NotNull(VMEntity* e, VMStackFrame* context) {
    if (e == nullptr) throw VMSimanticsException("Object reference not set to an instance of an object.", context);
    return e;
}

// TS1(sims): FreeSO VMMemory.GetTreeAd.
int16_t VMMemory::GetTreeAd(VMStackFrame* context, int32_t type, uint16_t data) {
    // first - try to get the tree ad we've set
    auto& adDict = context->Thread->MotiveAdChanges;
    auto found = adDict.find((type << 16) | data);
    if (found != adDict.end()) return found->second;
    // otherwise find it in the currently active action
    // be prepared for some overly complex garbage
    auto action = context->Thread->ActiveAction();
    if (action == nullptr || action->Callee == nullptr) return 0;
    auto tree = action->Callee->TreeTable;
    if (tree == nullptr) return 0;
    auto it = tree->InteractionByIndex.find((uint32_t)action->InteractionNumber);
    if (it == tree->InteractionByIndex.end()) return 0;
    auto interaction = it->second;
    if (data >= interaction->MotiveEntries.size()) return 0;
    auto& entry = interaction->MotiveEntries[data];
    switch (type) {
        case 0: return entry.EffectRangeMinimum;
        case 1: return entry.EffectRangeMaximum; // FreeSO: EffectRangeDelta
        case 2: return (int16_t)entry.PersonalityModifier;
    }
    return 0;
}

int16_t VMMemory::GetVariable(VMStackFrame* context, VMVariableScope scope, int16_t data) {
    auto& temps = context->Thread->TempRegisters;
    switch (scope) {
        case VMVariableScope::MyObjectAttributes: //0
            return context->Caller->GetAttribute((uint16_t)data);

        case VMVariableScope::StackObjectAttributes: //1
            if (context->StackObject != nullptr)
                return context->StackObject->GetAttribute((uint16_t)data);
            else
                return 0;

        case VMVariableScope::TargetObjectAttributes: //2
            throw VMSimanticsException("Target Object is Deprecated!", context);

        case VMVariableScope::MyObject: //3
            return context->Caller->GetValue((VMStackObjectVariable)data);

        case VMVariableScope::StackObject: //4
            if (context->StackObject != nullptr) {
                return context->StackObject->GetValue((VMStackObjectVariable)data);
            }
            return 0;

        case VMVariableScope::TargetObject: //5
            throw VMSimanticsException("Target Object is Deprecated!", context);

        case VMVariableScope::Global: //6
            return context->VM()->GetGlobalValue((uint16_t)data);

        case VMVariableScope::Literal: //7
            return data;

        case VMVariableScope::Temps: //8
            return At(temps, data, context);

        case VMVariableScope::Parameters: //9
            return At(context->Args, data, context);

        case VMVariableScope::StackObjectID: //10
            if (context->StackObject != nullptr) {
                return context->StackObject->ObjectID;
            }
            // TS1(sims): the id may name a neighbour rather than an object (see VMStackFrame::StackObjectRawID).
            if (context->VM()->TS1) return context->StackObjectRawID;
            return 0; //no object = 0, ids have a base of 1

        case VMVariableScope::TempByTemp: //11
            return At(temps, At(temps, data, context), context);

        case VMVariableScope::TreeAdRange: //12
            if (context->VM()->TS1) return GetTreeAd(context, 1, (uint16_t)data); // TS1(sims): FreeSO GetTreeAd
            throw VMSimanticsException("Not implemented...", context);

        case VMVariableScope::StackObjectTemp: //13
            throw VMSimanticsException("Not implemented...", context); //accesses the stack object's thread and gets its temp...

        case VMVariableScope::MyMotives: //14
            // FIX(sims): FreeSims tested `context.StackObject is VMAvatar` here (copy of the scope 15 case), so
            // "my motives" read 0 whenever the stack object was an object: a Sim in the shower ('tweak shower
            // finished': my hygiene >= ...) or on the toilet never finished. Test the caller, as SetVariable does.
            if (dynamic_cast<VMAvatar*>(context->Caller) != nullptr) {
                return AsAvatar(context->Caller, context)->GetMotiveData((VMMotive)data);
            }
            return 0;
        case VMVariableScope::StackObjectMotives: //15
            if (context->StackObject != nullptr && dynamic_cast<VMAvatar*>(context->StackObject) != nullptr) {
                return ((VMAvatar*)context->StackObject)->GetMotiveData((VMMotive)data);
            }
            return 0;
        case VMVariableScope::StackObjectSlot: { //16
            auto slotObj = NotNull(context->StackObject, context)->GetSlot(data);
            return (slotObj == nullptr) ? (int16_t)0 : slotObj->ObjectID;
        }

        case VMVariableScope::StackObjectMotiveByTemp: //17
            if (context->StackObject != nullptr) {
                return AsAvatar(context->StackObject, context)->GetMotiveData((VMMotive)At(temps, data, context));
            }
            return 0;
        case VMVariableScope::MyPersonData: //18
            return AsAvatar(context->Caller, context)->GetPersonData((VMPersonDataVariable)data);

        case VMVariableScope::StackObjectPersonData: //19
            if (context->StackObject != nullptr && dynamic_cast<VMAvatar*>(context->StackObject) != nullptr) {
                return ((VMAvatar*)context->StackObject)->GetPersonData((VMPersonDataVariable)data);
            }
            return 0;
        case VMVariableScope::MySlot: { //20
            auto slotObj2 = context->Caller->GetSlot(data);
            return (slotObj2 == nullptr) ? (int16_t)0 : slotObj2->ObjectID;
        }

        case VMVariableScope::StackObjectDefinition: //21
            return GetEntityDefinitionVar(NotNull(context->StackObject, context)->Object->OBJ, (VMOBJDVariable)data, context);

        case VMVariableScope::StackObjectAttributeByParameter: //22
            return NotNull(context->StackObject, context)->GetAttribute((uint16_t)At(context->Args, data, context));

        case VMVariableScope::RoomByTemp0: { //23
            //returns information on the selected room. Right now we don't have a room system, so always return the same values. (everywhere is indoors, not a pool)
            auto& roomInfo = context->VM()->Context->RoomInfo;
            if (roomInfo.empty()) throw VMSimanticsException("Invalid room data!", context);
            auto roomID = std::max(0, std::min((int)roomInfo.size() - 1, (int)temps[0]));
            auto& room = roomInfo[roomID];

            if (data == 0) return 100; //ambient light 0-100
            else if (data == 1) return (int16_t)((room.Room.IsOutside) ? 1 : 0); //outside
            else if (data == 2) return 0; //level
            else if (data == 3) return (int16_t)room.Room.Area; //area (???)
            else if (data == 4) return (int16_t)(room.Room.IsPool ? 1 : 0); //is pool
            else throw VMSimanticsException("Invalid room data!", context);

            //throw new VMSimanticsException("Not implemented...");
        }

        case VMVariableScope::NeighborInStackObject: //24
            // TS1(sims): FreeSims returned 1. Neighbour id is in the stack object id.
            if (context->VM()->TS1) return VMTS1Neighbourhood::GetNeighborData(context, context->StackObjectID(), data);
            return 1;

        case VMVariableScope::Local: //25
            return (int16_t)At(context->Locals, data, context);

        case VMVariableScope::Tuning: //26
            return GetTuningVariable(context->Callee, (uint16_t)data, context);

        case VMVariableScope::DynSpriteFlagForTempOfStackObject: //27
            return NotNull(context->StackObject, context)->IsDynamicSpriteFlagSet((uint16_t)At(temps, data, context)) ? (int16_t)1 : (int16_t)0;

        case VMVariableScope::TreeAdPersonalityVar: //28
            if (context->VM()->TS1) return GetTreeAd(context, 2, (uint16_t)data); // TS1(sims): FreeSO GetTreeAd
            throw VMSimanticsException("Not implemented...", context);

        case VMVariableScope::TreeAdMin: //29
            if (context->VM()->TS1) return GetTreeAd(context, 0, (uint16_t)data); // TS1(sims): FreeSO GetTreeAd
            throw VMSimanticsException("Not implemented...", context);

        case VMVariableScope::MyPersonDataByTemp: //30
            return AsAvatar(context->Caller, context)->GetPersonData((VMPersonDataVariable)(At(temps, data, context)));

        case VMVariableScope::StackObjectPersonDataByTemp: //31
            return AsAvatar(context->StackObject, context)->GetPersonData((VMPersonDataVariable)(At(temps, data, context)));

        case VMVariableScope::NeighborPersonData: //32
            //Neighbour neighbour = Content.Content.Get().TS1Neighborhood.GetNeighborByID(context.Caller.ObjectID);
            //return (neighbour != null) ? neighbour.PersonData.ElementAt(data) : (short)0;
            // TS1(sims): the shipped BHAVs read the neighbour whose id is in the stack object id
            // (e.g. VisitGenerator 'do generate': set to next neighbor -> neighbor's person data 61/58).
            if (context->VM()->TS1) return VMTS1Neighbourhood::GetNeighborPersonData(context, context->StackObjectID(), data);
            return 1;

        case VMVariableScope::JobData: //33 jobdata(temp0, temp1), used a few times to test if a person is at work but that isn't relevant for tso...
            if (context->VM()->TS1) return VMTS1Neighbourhood::GetJobData(context, data); // TS1(sims)
            throw VMSimanticsException("Should not be used, but if this shows implement an empty shell to return ideal values.", context);

        case VMVariableScope::NeighborhoodData: //34
            if (context->VM()->TS1) return VMTS1Neighbourhood::GetNeighborhoodData(context, data); // TS1(sims)
            throw VMSimanticsException("Should not be used, but if this shows implement an empty shell to return ideal values.", context);

        case VMVariableScope::StackObjectFunction: //35
            return (int16_t)At(NotNull(context->StackObject, context)->EntryPoints, data, context).ActionFunction;

        case VMVariableScope::MyTypeAttr: //36
            if (context->VM()->TS1) return VMTS1Neighbourhood::GetTypeAttr(context, context->Caller, data); // TS1(sims)
            throw VMSimanticsException("Unused", context);

        case VMVariableScope::StackObjectTypeAttr: //37
            if (context->VM()->TS1) return VMTS1Neighbourhood::GetTypeAttr(context, context->StackObject, data); // TS1(sims)
            throw VMSimanticsException("Unused", context);

        case VMVariableScope::ThirtyEight: //38
            // TS1(sims): TS1 scope 38 is "Neighbor's Object Definition".
            if (context->VM()->TS1) return VMTS1Neighbourhood::GetNeighborDefinition(context, context->StackObjectID(), data);
            throw VMSimanticsException("Really", context);

        case VMVariableScope::LocalByTemp: //40
            return (int16_t)At(context->Locals, At(temps, data, context), context);

        case VMVariableScope::StackObjectAttributeByTemp: //41
            return NotNull(context->StackObject, context)->GetAttribute((uint16_t)At(temps, data, context));

        case VMVariableScope::TempXL: //42
            //this needs a really intricate special case for specific operations.
            throw VMSimanticsException("Caller function does not support TempXL!", context);

        case VMVariableScope::CityTime: //43
            //return GetCityTime(data)
            switch (data) {
                case 0: return (int16_t)context->VM()->Context->Clock->Seconds();
                case 1: return (int16_t)context->VM()->Context->Clock->Minutes;
                case 2: return (int16_t)context->VM()->Context->Clock->Hours;
                case 3: return (int16_t)context->VM()->Context->Clock->TimeOfDay();
            };
            break;
        case VMVariableScope::TSOStandardTime: //44
            //return GetTSOStandardTime(data)
            return 0;
            //throw new VMSimanticsException("Not implemented...", context);

        case VMVariableScope::GameTime: //45
            switch (data) {
                case 0: return (int16_t)context->VM()->Context->Clock->Seconds();
                case 1: return (int16_t)context->VM()->Context->Clock->Minutes;
                case 2: return (int16_t)context->VM()->Context->Clock->Hours;
                case 3: return (int16_t)context->VM()->Context->Clock->TimeOfDay();
            };
            break;

        case VMVariableScope::MyList: { //46 (man if only i knew what this meant)
            auto& list = context->Caller->MyList;
            switch (data) {
                case 0: if (list.empty()) throw VMSimanticsException("Object reference not set to an instance of an object.", context); return list.front(); //is this allowed?
                case 1: if (list.empty()) throw VMSimanticsException("Object reference not set to an instance of an object.", context); return list.back();
                case 2: return (int16_t)list.size();
                default: throw VMSimanticsException("Unknown List Accessor", context);
            }
        }
        case VMVariableScope::StackObjectList: { //47
            if (context->StackObject == nullptr) return 0;
            auto& list = context->StackObject->MyList;
            switch (data) {
                case 0: if (list.empty()) throw VMSimanticsException("Object reference not set to an instance of an object.", context); return list.front();
                case 1: if (list.empty()) throw VMSimanticsException("Object reference not set to an instance of an object.", context); return list.back();
                case 2: return (int16_t)list.size();
                default: throw VMSimanticsException("Unknown List Accessor", context);
            }
        }

        case VMVariableScope::MoneyOverHead32Bit: //48
            //we're poor... will need special case for this in expression like TempXL
            return 0;

        case VMVariableScope::MyLeadTileAttribute: //49
            return context->Caller->MultitileGroup->BaseObject()->GetAttribute((uint16_t)data);

        case VMVariableScope::StackObjectLeadTileAttribute: //50
            return NotNull(context->StackObject, context)->MultitileGroup->BaseObject()->GetAttribute((uint16_t)data);

        case VMVariableScope::MyLeadTile: //51
            throw VMSimanticsException("Not implemented...", context);

        case VMVariableScope::StackObjectLeadTile: //52
            throw VMSimanticsException("Not implemented...", context);

        case VMVariableScope::StackObjectMasterDef: { //53
            //gets definition of the master tile of a multi tile object in the stack object.
            auto masterDef = NotNull(context->StackObject, context)->MasterDefinition;
            return GetEntityDefinitionVar((masterDef == nullptr) ? context->StackObject->Object->OBJ : masterDef, (VMOBJDVariable)data, context);
        }

        case VMVariableScope::FeatureEnableLevel: //54
            return 1;
            //all of them are enabled, dont really care right now

        case VMVariableScope::MyAvatarID: //59
            return 0;

        default: break;
    }
    throw VMSimanticsException("Unknown get variable", context);
}

std::list<int16_t>& VMMemory::GetList(VMStackFrame* context, VMVariableScope scope) {
    switch (scope) {
        case VMVariableScope::MyList:
            return context->Caller->MyList;
        case VMVariableScope::StackObjectList:
            return NotNull(context->StackObject, context)->MyList;
        default:
            throw VMSimanticsException("Cannot get specified variable scope as a list.", context);
    }
}

int32_t VMMemory::GetBigVariable(VMStackFrame* context, VMVariableScope scope, int16_t data) {
    switch (scope) {
        case VMVariableScope::TempXL:
            return At(context->Thread->TempXL, data, context);
        default:
            return GetVariable(context, scope, data); //return a normal var
    }
}

int16_t VMMemory::GetTuningVariable(VMEntity* /*entity*/, uint16_t data, VMStackFrame* context) {
    auto tableID = (uint16_t)(data >> 7);
    auto keyID = (uint16_t)(data & 0x7F);

    int mode = 0;
    if (tableID < 64) mode = 0;
    else if (tableID < 128) { tableID = (uint16_t)((tableID - 64)); mode = 1; }
    else if (tableID < 192) { tableID = (uint16_t)((tableID - 128)); mode = 2; }

    BCON* bcon;
    OTFTable* tuning;

    /** This could be in a BCON or an OTF **/

    switch (mode) {
        case 0: //local
            bcon = context->ScopeResource()->Get<BCON>((uint16_t)(tableID + 4096));
            if (bcon != nullptr) return (int16_t)At(bcon->Constants, keyID, context);

            tuning = context->ScopeResource()->Get<OTFTable>((uint16_t)(tableID + 4096));
            if (tuning != nullptr) { auto k = tuning->GetKey(keyID); if (k == nullptr) throw VMSimanticsException("Object reference not set to an instance of an object.", context); return (int16_t)k->Value; }
            break;
        case 1: { //semi globals
            uint16_t testTab = (uint16_t)(tableID + 8192);
            bcon = context->ScopeResource()->Get<BCON>(testTab);
            if (bcon != nullptr && keyID < bcon->Constants.size()) return (int16_t)bcon->Constants[keyID];

            tuning = context->ScopeResource()->Get<OTFTable>(testTab);
            if (tuning != nullptr) { auto k = tuning->GetKey(keyID); if (k == nullptr) throw VMSimanticsException("Object reference not set to an instance of an object.", context); return (int16_t)k->Value; }

            if (context->ScopeResource()->SemiGlobal != nullptr) {
                bcon = context->ScopeResource()->SemiGlobal->Get<BCON>(testTab);
                if (bcon != nullptr && keyID < bcon->Constants.size()) return (int16_t)bcon->Constants[keyID];

                tuning = context->ScopeResource()->SemiGlobal->Get<OTFTable>(testTab);
                if (tuning != nullptr) { auto k = tuning->GetKey(keyID); if (k == nullptr) throw VMSimanticsException("Object reference not set to an instance of an object.", context); return (int16_t)k->Value; }
            }
            break;
        }
        case 2: //global
            bcon = context->Global()->Resource->Get<BCON>((uint16_t)(tableID + 256));
            if (bcon != nullptr && keyID < bcon->Constants.size()) return (int16_t)bcon->Constants[keyID];

            tuning = context->Global()->Resource->Get<OTFTable>((uint16_t)(tableID + 256));
            if (tuning != nullptr) { auto k = tuning->GetKey(keyID); if (k == nullptr) throw VMSimanticsException("Object reference not set to an instance of an object.", context); return (int16_t)k->Value; }
            break;
    }

    //throw new Exception("Could not find tuning constant!");
    return 0;
}

std::string VMMemory::GetTuningVariableLabel(FSO::Content::GameObject* obj, uint16_t data) {
    auto tableID = (uint16_t)(4096 + (data >> 7));
    auto keyID = (uint16_t)(data & 0x7F);

    /** This could be in a BCON or an OTF **/
    auto bcon = obj->Resource->Get<BCON>(tableID);
    if (bcon != nullptr) {
        return "bcon." + std::to_string(data);
    }

    auto tuning = obj->Resource->Get<OTFTable>(tableID);
    if (tuning != nullptr) {
        auto k = tuning->GetKey(keyID);
        if (k != nullptr) return k->Label;
    }

    return "unknown, probably global or semiglobal";
    //throw new VMSimanticsException("Could not find tuning constant!");
}

int16_t VMMemory::GetEntityDefinitionVar(OBJD* objd, VMOBJDVariable var, VMStackFrame* context) {
    // FIX(sims): FreeSO reads every other field straight from the OBJD's raw words (RawData[var - 2]). FreeSims' switch
    // below threw "Not Implemented!" for several fields (the Vacation cattails reset on every lot: Shrubs3 'idle'
    // reads one of them), returned CatalogID for RatingComfort / ServingSurfaceTreeID and GUID % 0xFFFF for GUID1.
    if (var != VMOBJDVariable::Version1 && var != VMOBJDVariable::Version2) {
        size_t idx = (size_t)((int)var - 2);
        if ((int)var >= 2 && idx < objd->RawData.size()) return (int16_t)objd->RawData[idx];
        // a field newer than the object's OBJD version (e.g. field 95 of a 95-field Vacation cattail, read by its
        // 'Create a Duck'): 0, the value an unset field has (FreeSO would index out of range and reset the object).
        if ((int)var >= 2 && !objd->RawData.empty()) return 0;
    }
    switch (var) {
        case VMOBJDVariable::Version1: return (int16_t)(objd->Version % 0xFFFF);
        case VMOBJDVariable::Version2: return (int16_t)(objd->Version >> 16);
        case VMOBJDVariable::InitialStackSize: return (int16_t)objd->StackSize;
        case VMOBJDVariable::BaseGraphic: return (int16_t)objd->BaseGraphicID;
        case VMOBJDVariable::NumGraphics: return (int16_t)objd->NumGraphics;
        case VMOBJDVariable::MainTreeID: return (int16_t)objd->BHAV_MainID; // should this use OBJf functions?
        case VMOBJDVariable::GardeningTreeID: return (int16_t)objd->BHAV_GardeningID;
        case VMOBJDVariable::TreeTableID: return (int16_t)objd->TreeTableID;
        case VMOBJDVariable::InteractionGroup: return (int16_t)objd->InteractionGroupID;
        case VMOBJDVariable::Type: return (int16_t)objd->ObjectType;
        case VMOBJDVariable::MasterID: return (int16_t)objd->MasterID;
        case VMOBJDVariable::SubIndex: return (int16_t)objd->SubIndex;
        case VMOBJDVariable::WashHandsTreeID: return (int16_t)objd->BHAV_WashHandsID;
        case VMOBJDVariable::AnimTableID: return (int16_t)objd->AnimationTableID;
        case VMOBJDVariable::GUID1: return (int16_t)(objd->GUID % 0xFFFF);
        case VMOBJDVariable::GUID2: return (int16_t)(objd->GUID >> 16);
        case VMOBJDVariable::Disabled: return (int16_t)objd->Disabled;
        case VMOBJDVariable::PortalTreeID: throw VMSimanticsException("Not Implemented!", context);
        case VMOBJDVariable::Price: return (int16_t)objd->Price;
        case VMOBJDVariable::BodyStringsID: return (int16_t)objd->BodyStringID;
        case VMOBJDVariable::SlotsID: return (int16_t)objd->SlotID;
        case VMOBJDVariable::AllowIntersectionTreeID: return (int16_t)objd->BHAV_AllowIntersectionID;
        case VMOBJDVariable::UsesFnTable: return (int16_t)objd->UsesFnTable;
        case VMOBJDVariable::Bitfield1: return (int16_t)objd->BitField1;
        case VMOBJDVariable::PrepareFoodTreeID: return (int16_t)objd->BHAV_PrepareFoodID;
        case VMOBJDVariable::CookFoodTreeID: return (int16_t)objd->BHAV_CookFoodID;
        case VMOBJDVariable::PlaceOnSurfaceTreeID: return (int16_t)objd->BHAV_PlaceSurfaceID;
        case VMOBJDVariable::DisposeTreeID: return (int16_t)objd->BHAV_DisposeID;
        case VMOBJDVariable::EatFoodTreeID: return (int16_t)objd->BHAV_EatID;
        case VMOBJDVariable::PickupFromSlotTreeID: return (int16_t)objd->BHAV_PickupFromSlotID; //uh
        case VMOBJDVariable::WashDishTreeID: return (int16_t)objd->BHAV_WashDishID;
        case VMOBJDVariable::EatingSurfaceTreeID: return (int16_t)objd->BHAV_EatSurfaceID;
        case VMOBJDVariable::SitTreeID: return (int16_t)objd->BHAV_SitID;
        case VMOBJDVariable::StandTreeID: return (int16_t)objd->BHAV_StandID;
        case VMOBJDVariable::SalePrice: return (int16_t)objd->SalePrice;
        case VMOBJDVariable::Unused35: throw VMSimanticsException("Not Implemented!", context);
        case VMOBJDVariable::Unused36: throw VMSimanticsException("Not Implemented!", context);
        case VMOBJDVariable::BrokenBaseGraphicOffset: throw VMSimanticsException("Not Implemented!", context);
        case VMOBJDVariable::Unused38: throw VMSimanticsException("Not Implemented!", context);
        case VMOBJDVariable::HasCriticalAttributes: throw VMSimanticsException("Not Implemented!", context);
        case VMOBJDVariable::BuyModeType: return (int16_t)objd->FunctionFlags;
        case VMOBJDVariable::CatalogStringsID: return (int16_t)objd->CatalogStringsID;
        case VMOBJDVariable::IsGlobalSimObject: return (int16_t)objd->Global;
        case VMOBJDVariable::InitTreeID: return (int16_t)objd->BHAV_Init;
        case VMOBJDVariable::PlaceTreeID: return (int16_t)objd->BHAV_Place;
        case VMOBJDVariable::UserPickupTreeID: return (int16_t)objd->BHAV_UserPickup;
        case VMOBJDVariable::WallStyle: return (int16_t)objd->WallStyle;
        case VMOBJDVariable::LoadTreeID: return (int16_t)objd->BHAV_Load;
        case VMOBJDVariable::UserPlaceTreeID: return (int16_t)objd->BHAV_UserPlace;
        case VMOBJDVariable::ObjectVersion: return (int16_t)objd->ObjectVersion;
        case VMOBJDVariable::RoomChangedTreeID: return (int16_t)objd->BHAV_RoomChange;
        case VMOBJDVariable::MotiveEffectsID: return (int16_t)objd->MotiveEffectsID;
        case VMOBJDVariable::CleanupTreeID: return (int16_t)objd->BHAV_Cleanup;
        case VMOBJDVariable::LevelInfoRequestTreeID: return (int16_t)objd->BHAV_LevelInfo;
        case VMOBJDVariable::CatalogPopupID: return (int16_t)objd->CatalogID;
        case VMOBJDVariable::ServingSurfaceTreeID: return (int16_t)objd->CatalogID; // NOTE: FreeSims returns CatalogID here (sic)
        case VMOBJDVariable::LevelOffset: return (int16_t)objd->LevelOffset;
        case VMOBJDVariable::Shadow: return (int16_t)objd->Shadow;
        case VMOBJDVariable::NumAttributes: return (int16_t)objd->NumAttributes;
        case VMOBJDVariable::CleanTreeID: return (int16_t)objd->BHAV_Clean;
        case VMOBJDVariable::QueueSkippedTreeID: return (int16_t)objd->BHAV_QueueSkipped;
        case VMOBJDVariable::FrontDirection: return (int16_t)objd->FrontDirection;
        case VMOBJDVariable::WallAdjacencyChangedTreeID: return (int16_t)objd->BHAV_WallAdjacencyChanged;
        case VMOBJDVariable::MyLeadObject: return (int16_t)objd->MyLeadObject;
        case VMOBJDVariable::DynamicSpritesBaseID: return (int16_t)objd->DynamicSpriteBaseId;
        case VMOBJDVariable::NumDynamicSprites: return (int16_t)objd->NumDynamicSprites;
        case VMOBJDVariable::ChairEntryFlags: return (int16_t)objd->ChairEntryFlags;
        case VMOBJDVariable::TileWidth: return (int16_t)objd->TileWidth;
        case VMOBJDVariable::LotCategories: return 0; //NOT IN OBJD RIGHT NOW!
        case VMOBJDVariable::BuildModeType: return (int16_t)objd->BuildModeType;
        case VMOBJDVariable::OriginalGUID1: return (int16_t)objd->OriginalGUID1;
        case VMOBJDVariable::OriginalGUID2: return (int16_t)objd->OriginalGUID2;
        case VMOBJDVariable::SuitGUID1: return (int16_t)objd->SuitGUID1;
        case VMOBJDVariable::SuitGUID2: return (int16_t)objd->SuitGUID2;
        case VMOBJDVariable::PickupTreeID: return (int16_t)objd->BHAV_Pickup;
        case VMOBJDVariable::ThumbnailGraphic: return (int16_t)objd->ThumbnailGraphic;
        case VMOBJDVariable::ShadowFlags: return (int16_t)objd->ShadowFlags;
        case VMOBJDVariable::FootprintMask: return (int16_t)objd->FootprintMask;
        case VMOBJDVariable::DynamicMultiTileUpdateTreeID: return (int16_t)objd->BHAV_DynamicMultiTileUpdate;
        case VMOBJDVariable::ShadowBrightness: return (int16_t)objd->ShadowBrightness;
        case VMOBJDVariable::RepairTreeID: return (int16_t)objd->BHAV_Repair;
        case VMOBJDVariable::WallStyleSpriteID: return (int16_t)objd->WallStyleSpriteID;
        case VMOBJDVariable::RatingHunger: return (int16_t)objd->RatingHunger;
        case VMOBJDVariable::RatingComfort: return (int16_t)objd->CatalogID; // NOTE: FreeSims returns CatalogID here (sic)
        case VMOBJDVariable::RatingHygiene: return (int16_t)objd->RatingHygiene;
        case VMOBJDVariable::RatingBladder: return (int16_t)objd->RatingBladder;
        case VMOBJDVariable::RatingEnergy: return (int16_t)objd->RatingEnergy;
        case VMOBJDVariable::RatingFun: return (int16_t)objd->RatingFun;
        case VMOBJDVariable::RatingRoom: return (int16_t)objd->RatingRoom;
        case VMOBJDVariable::RatingSkillFlags: return (int16_t)objd->RatingSkillFlags;
        case VMOBJDVariable::NumTypeAttributes: throw VMSimanticsException("Not Implemented!", context); //??
        case VMOBJDVariable::MiscFlags: throw VMSimanticsException("Not Implemented!", context); //??
        case VMOBJDVariable::TypeAttrGUID1: throw VMSimanticsException("Not Implemented!", context);
        case VMOBJDVariable::TypeAttrGUID2: throw VMSimanticsException("Not Implemented!", context);
        case VMOBJDVariable::InteractionResultStrings: throw VMSimanticsException("Not Implemented!", context);
        case VMOBJDVariable::ClientHouseJoinTreeID: throw VMSimanticsException("Not Implemented!", context);
        case VMOBJDVariable::PrepareForSaleTreeID: throw VMSimanticsException("Not Implemented!", context);
        default:
            throw VMSimanticsException("Unknown definition var", context);
    }
}

bool VMMemory::SetVariable(VMStackFrame* context, VMVariableScope scope, int16_t data, int16_t value) {
    auto& temps = context->Thread->TempRegisters;
    switch (scope) {
        case VMVariableScope::MyObjectAttributes: //0
            context->Caller->SetAttribute((uint16_t)data, value);
            return true;

        case VMVariableScope::StackObjectAttributes: //1
            if (context->StackObject != nullptr)
                context->StackObject->SetAttribute((uint16_t)data, value);
            return true;

        case VMVariableScope::TargetObjectAttributes: //2
            throw VMSimanticsException("Target Object is Deprecated!", context);

        case VMVariableScope::MyObject: //3
            return context->Caller->SetValue((VMStackObjectVariable)data, value);

        case VMVariableScope::StackObject: //4
            if (context->StackObject != nullptr)
                return context->StackObject->SetValue((VMStackObjectVariable)data, value);
            else
                return true;

        case VMVariableScope::TargetObject: //5
            throw VMSimanticsException("Target Object is Deprecated!", context);

        case VMVariableScope::Global: //6
            return context->VM()->SetGlobalValue((uint16_t)data, value);

        case VMVariableScope::Literal: //7
            /** Huh? **/
            return false;

        case VMVariableScope::Temps: //8
            At(temps, data, context) = value;
            return true;

        case VMVariableScope::Parameters: //9
            /** Not too sure if this is illegal **/
            At(context->Args, data, context) = value;
            return true;

        case VMVariableScope::StackObjectID: //10
            /** Change the stack object **/
            context->StackObject = context->VM()->GetObjectById(value);
            context->StackObjectRawID = value; // TS1(sims): keep neighbour ids (see VMStackFrame)
            return true;

        case VMVariableScope::TempByTemp: //11
            At(temps, At(temps, data, context), context) = value;
            return true;

        case VMVariableScope::TreeAdRange: //12
            if (context->VM()->TS1) { context->Thread->MotiveAdChanges[(1 << 16) | (uint16_t)data] = value; return true; } // TS1(sims): FreeSO SetTreeAd
            return false; //can't set this!

        case VMVariableScope::StackObjectTemp: //13
            throw VMSimanticsException("Not implemented...", context);

        case VMVariableScope::MyMotives: //14
            if (dynamic_cast<VMAvatar*>(context->Caller) != nullptr)
                return ((VMAvatar*)context->Caller)->SetMotiveData((VMMotive)data, value);
            return false;

        case VMVariableScope::StackObjectMotives: //15
            return AsAvatar(context->StackObject, context)->SetMotiveData((VMMotive)data, value);

        case VMVariableScope::StackObjectSlot: //16
            throw VMSimanticsException("Not implemented...", context);

        case VMVariableScope::StackObjectMotiveByTemp: //17
            return AsAvatar(context->StackObject, context)->SetMotiveData((VMMotive)At(temps, data, context), value);

        case VMVariableScope::MyPersonData: //18
            return AsAvatar(context->Caller, context)->SetPersonData((VMPersonDataVariable)data, value);

        case VMVariableScope::StackObjectPersonData: //19
            return AsAvatar(context->StackObject, context)->SetPersonData((VMPersonDataVariable)data, value);

        case VMVariableScope::MySlot: //20
            throw VMSimanticsException("Not implemented...", context);

        case VMVariableScope::StackObjectDefinition: //21
            return false; //you can't set this!

        case VMVariableScope::StackObjectAttributeByParameter: //22
            NotNull(context->StackObject, context)->SetAttribute((uint16_t)At(context->Args, data, context), value);
            return true;

        case VMVariableScope::RoomByTemp0: //23
            throw VMSimanticsException("Not implemented...", context);

        case VMVariableScope::NeighborInStackObject: //24
            throw VMSimanticsException("Not implemented...", context);

        case VMVariableScope::Local: //25
            At(context->Locals, data, context) = value;
            return true;

        case VMVariableScope::Tuning: //26
            return false; //you can't set this!

        case VMVariableScope::DynSpriteFlagForTempOfStackObject: //27
            NotNull(context->StackObject, context)->SetDynamicSpriteFlag((uint16_t)At(temps, data, context), value > 0);
            return true;

        case VMVariableScope::TreeAdPersonalityVar: //28
            if (context->VM()->TS1) { context->Thread->MotiveAdChanges[(2 << 16) | (uint16_t)data] = value; return true; } // TS1(sims): FreeSO SetTreeAd
            return false; //you can't set this!

        case VMVariableScope::TreeAdMin: //29
            if (context->VM()->TS1) { context->Thread->MotiveAdChanges[(0 << 16) | (uint16_t)data] = value; return true; } // TS1(sims): FreeSO SetTreeAd
            return false; //you can't set this!

        case VMVariableScope::MyPersonDataByTemp: //30
            return AsAvatar(context->Caller, context)->SetPersonData((VMPersonDataVariable)At(temps, data, context), value);

        case VMVariableScope::StackObjectPersonDataByTemp: //31
            return AsAvatar(context->StackObject, context)->SetPersonData((VMPersonDataVariable)At(temps, data, context), value);

        case VMVariableScope::NeighborPersonData: //32
            if (context->VM()->TS1) return VMTS1Neighbourhood::SetNeighborPersonData(context, context->StackObjectID(), data, value); // TS1(sims)
            throw VMSimanticsException("Not implemented...", context);

        case VMVariableScope::JobData: //33
            throw VMSimanticsException("Not implemented...", context);

        case VMVariableScope::NeighborhoodData: //34
            if (context->VM()->TS1) return VMTS1Neighbourhood::SetNeighborhoodData(context, data, value); // TS1(sims)
            throw VMSimanticsException("Not implemented...", context);

        case VMVariableScope::StackObjectFunction: //35
            return false; //you can't set this!

        case VMVariableScope::MyTypeAttr: //36
            if (context->VM()->TS1) return VMTS1Neighbourhood::SetTypeAttr(context, context->Caller, data, value); // TS1(sims)
            throw VMSimanticsException("Not implemented...", context);

        case VMVariableScope::StackObjectTypeAttr: //37
            if (context->VM()->TS1) return VMTS1Neighbourhood::SetTypeAttr(context, context->StackObject, data, value); // TS1(sims)
            throw VMSimanticsException("Not implemented...", context);

        case VMVariableScope::ThirtyEight: //38
            return false; //you can't set this!

        case VMVariableScope::LocalByTemp: //40
            At(context->Locals, At(temps, data, context), context) = value;
            return true;

        case VMVariableScope::StackObjectAttributeByTemp: //41
            NotNull(context->StackObject, context)->SetAttribute((uint16_t)At(temps, data, context), value);
            return true;

        case VMVariableScope::TempXL: //42
            throw VMSimanticsException("Not implemented...", context);
            //this will need a special case for the expression primitive

        case VMVariableScope::CityTime: //43
        case VMVariableScope::TSOStandardTime: //44
        case VMVariableScope::GameTime: //45
            return false; //you can't set this!

        case VMVariableScope::MyList: //46
            throw VMSimanticsException("Not implemented...", context);

        case VMVariableScope::StackObjectList: //47
            throw VMSimanticsException("Not implemented...", context);

        case VMVariableScope::MoneyOverHead32Bit: //48
            //throw new VMSimanticsException("Not implemented...", context);
            return true;
            //needs special case like TempXL.

        case VMVariableScope::MyLeadTileAttribute: //49
            context->Caller->MultitileGroup->BaseObject()->SetAttribute((uint16_t)data, value);
            return true;

        case VMVariableScope::StackObjectLeadTileAttribute: //50
            NotNull(context->StackObject, context)->MultitileGroup->BaseObject()->SetAttribute((uint16_t)data, value);
            return true;

        case VMVariableScope::MyLeadTile: //51
        case VMVariableScope::StackObjectLeadTile: //52
        case VMVariableScope::StackObjectMasterDef: //53
            return false;

        case VMVariableScope::FeatureEnableLevel: //54
            throw VMSimanticsException("Not implemented...", context);

        case VMVariableScope::MyAvatarID: //59
            return false; //you can't set this!

        default:
            throw VMSimanticsException("Unknown scope for set variable!", context);
    }
}

bool VMMemory::SetBigVariable(VMStackFrame* context, VMVariableScope scope, int16_t data, int32_t value) {
    switch (scope) {
        case VMVariableScope::TempXL:
            At(context->Thread->TempXL, data, context) = value;
            return true;
        case VMVariableScope::MoneyOverHead32Bit: // FreeSO: ((VMAvatar)context.Caller).ShowMoneyHeadline(value)
            if (auto av = dynamic_cast<VMAvatar*>(context->Caller)) av->ShowMoneyHeadline(value);
            return true;
        default:
            return SetVariable(context, scope, data, (int16_t)value); //truncate value and set the relevant 16 bit var to it.
    }
}

FSO::Vitaboy::Animation* VMMemory::GetAnimation(VMStackFrame* context, VMAnimationScope scope, uint16_t id) {
    STR* animTable = nullptr;
    // TS1(sims): FreeSO — a child (age < 18) uses the object's next animation table (STR#130 "c2o" after STR#129
    // "a2o"; the indices match) and the global MiscChild table (157). FreeSims only knew the adult tables, so children
    // played adult animations or none (e.g. the TV's 'C2O-tvsmall-turnon').
    auto ava = dynamic_cast<VMAvatar*>(context->Caller);
    bool child = context->VM()->TS1 && ava != nullptr && ava->GetPersonData(VMPersonDataVariable::PersonsAge) < 18;
    //a2o, c2o, a2c, c2a

    switch (scope) {
        case VMAnimationScope::Object: {
            auto obj = context->CodeOwner;
            auto anitableID = (uint16_t)(obj->OBJ->AnimationTableID + (child ? 1 : 0));
            animTable = obj->Resource->Get<STR>(anitableID);
            if (animTable == nullptr) animTable = obj->Resource->Get<STR>((uint16_t)(child ? 130 : 129));
            break;
        }
        case VMAnimationScope::Misc:
            animTable = context->Global()->Resource->Get<STR>((uint16_t)(child ? 157 : 156));
            break;
        case VMAnimationScope::PersonStock:
            animTable = context->Global()->Resource->Get<STR>(130);
            break;
        case VMAnimationScope::Global:
            animTable = context->Global()->Resource->Get<STR>(128);
            break;
    }

    if (animTable == nullptr) {
        return nullptr;
    }

    std::string animationName;
    if (animTable->TryGetString(id, FSO::Files::Formats::IFF::Chunks::STR::DefaultLangCode, animationName)) {
        // TS1(sims): the Global/PersonStock tables only name adult animations ("a2o-..."); a child plays the
        // "c2o-..." version when one exists (inference from the StdChild/ReachChild/MiscChild naming; FreeSO plays
        // the adult name, which TS1 children have no skeleton match for).
        if (child && scope != VMAnimationScope::Object && scope != VMAnimationScope::Misc
            && animationName.size() > 3 && (animationName[0] == 'a' || animationName[0] == 'A') && animationName[1] == '2') {
            auto childName = "c" + animationName.substr(1);
            auto childAnim = FSO::Content::Content::Get()->AvatarAnimations->Get(childName + ".anim");
            if (childAnim != nullptr) return childAnim;
        }
        return FSO::Content::Content::Get()->AvatarAnimations->Get(animationName + ".anim");
    }
    else return nullptr;
}

FSO::Vitaboy::Appearance* VMMemory::GetSuit(VMStackFrame* context, VMSuitScope scope, uint16_t id) {
    switch (scope) {
        case VMSuitScope::Object: {
            auto suitTable = context->Callee->Object->Resource->Get<STR>(304);
            if (suitTable != nullptr) {
                auto suitFile = suitTable->GetString(id) + ".apr";
                auto apr = FSO::Content::Content::Get()->AvatarAppearances->Get(suitFile);
                return apr;
            }
            return nullptr;
        }
        default:
            return nullptr;
            //throw new VMSimanticsException("I dont know about this suit scope", context);
    }
}

SLOTItem* VMMemory::GetSlot(VMStackFrame* context, VMSlotScope scope, uint16_t data) {
    switch (scope) {
        case VMSlotScope::Global: {
            auto slots = context->Global()->Resource->Get<SLOT>(100);
            if (slots != nullptr) {
                auto it = slots->Slots.find(3);
                if (it != slots->Slots.end() && data < it->second.size()) return &it->second[data];
            }
            return nullptr;
        }
        case VMSlotScope::Literal: {
            auto slots = NotNull(context->StackObject, context)->Slots;
            if (slots == nullptr) throw VMSimanticsException("Object reference not set to an instance of an object.", context);
            auto it = slots->Slots.find(3);
            if (it == slots->Slots.end()) throw VMSimanticsException("The given key was not present in the dictionary.", context);
            if (data >= it->second.size()) return nullptr; // TS1(sims): FreeSO — an index past the table is no slot
            return &it->second[data];
        }
        case VMSlotScope::StackVariable: {
            auto slots = NotNull(context->StackObject, context)->Slots;
            if (slots == nullptr) throw VMSimanticsException("Object reference not set to an instance of an object.", context);
            auto it = slots->Slots.find(3);
            if (it == slots->Slots.end()) throw VMSimanticsException("The given key was not present in the dictionary.", context);
            auto idx = At(context->Args, data, context);
            if (idx < 0 || (size_t)idx >= it->second.size()) return nullptr; // TS1(sims): FreeSO (see Literal)
            return &it->second[idx];
        }
    }
    return nullptr;
}
}}}}
