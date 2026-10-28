// FSO.SimAntics.Primitives.VMGenericTS1Call (FreeSO tso.simantics/Primitives/VMGenericTS1Call.cs) +
// FSO.SimAntics.Model.VMGenericTS1CallMode (FreeSO Model/VMGenericTS1CallMode.cs)
/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// TS1(sims): The Sims 1 "Generic Sims Call" (primitive 1) modes, Behavior.iff STR#220 'generic sim calls'.
// FreeSims only has the TSO meanings (VMGenericTSOCall). FreeSO (FreeSims' successor, MPL-2.0) has a TS1 version;
// its implemented modes are translated here, the ones it leaves empty are implemented from the shipped expansion
// pack BHAVs (marked TS1(sims) in the .cpp). VMGenericTSOCall dispatches TS1 calls here, except the family
// modes 4/5/6/7/25/28/33 that the neighbourhood pass implemented there (see STUBS_neighbourhood.md).
#pragma once
#include <cstdint>
#include "simantics/engine/VMPrimitiveHandler.h"

namespace FSO { namespace SimAntics {
class VMAvatar;
namespace Model {
enum class VMGenericTS1CallMode : uint8_t
{
    HouseTutorialComplete = 0,
    SwapMyAndStackObjectsSlots = 1,
    SetActionIconToStackObject = 2,
    PullDownTaxiDialog = 3,
    AddToFamily = 4,
    CombineAssetsOfFamilyInTemp0 = 5,
    RemoveFromFamily = 6,
    MakeNewNeighbor = 7, //this one is "depracated"
    FamilyTutorialComplete = 8,
    ArchitectureTutorialComplete = 9,
    DisableBuildBuy = 10,
    EnableBuildBuy = 11,
    GetDistanceToCameraInTemp0 = 12,
    AbortInteractions = 13, //abort all interactions associated with the stack object
    HouseRadioStationEqualsTemp0 = 14,
    MyRoutingFootprintEqualsTemp0 = 15,
    ChangeNormalOutfit = 16, //changes the normal outfit of the sim to the next available suit
    ChangeToLotInTemp0 = 17,
    BuildTheDowntownSimAndPlaceObjIDInTemp0 = 18,
    SpawnDowntownDateOfPersonInTemp0 = 19, //temp0 is replaced with autofollow sim in temp0
    SpawnTakeBackHomeDataOfPersonInTemp0 = 20, //same side effect as above
    SpawnInventorySimDataEffects = 21,
    SelectDowntownLot = 22, //displays the selection screen and returns a lot in temp0 (simulation paused)
    GetDowntownTimeFromSOInventory = 23, //hours in temp0, minutes in temp1
    HotDateChangeSuitsPermanentlyCall = 24,
    SaveSimPersistentData = 25, //motives, relationships...
    BuildVacationFamilyPutFamilyNumInTemp0 = 26,
    ReturnNumberOfAvaiableVacationLotsInTemp0 = 27,
    ReturnZoningTypeOfLotInTemp0 = 28,
    SetStackObjectsSuit = 29, //suit type in temp0, suit index in temp1. Returns old index in temp1.
    GetStackObjectsSuit = 30, //suit type in temp0, suit index in temp1.
    CountStackObjectSuits = 31, //suit type in temp0, suit count returned in temp1.
    CreatePurchasedPetsNearOwner = 32,
    AddToFamilyInTemp0 = 33,
    PromoteFameIfNeeded = 34,
    TakeTaxiHook = 35,
    DemoteFameIfNeeded = 36,
    CancelPieMenu = 37,
    GetTokensFromString = 38,
    ChildToAdult = 39,
    PetToAdult = 40,
    HeadFlush = 41,
    MakeTemp0SelectedSim = 42,
    FamilySpellsIntoController = 43
};
}

namespace Primitives {

class VMGenericTS1Call {
public:
    /// Runs TS1 Generic Sims Call `call`. Called by VMGenericTSOCall::Execute when the VM runs TS1 content.
    static Engine::VMPrimitiveExitCode Execute(Engine::VMStackFrame* context, Model::VMGenericTS1CallMode call);

    // ---- TS1(sims): Superstar fame (GameData/fame.iff STR#4/5/6; person data 80 score, 81 star level) ----
    /// Star level the requirements allow from `level` upwards (0-10), for promotion.
    static int16_t FameLevelFor(FSO::SimAntics::VMAvatar* avatar, int16_t level, bool promote);
};

}}}
