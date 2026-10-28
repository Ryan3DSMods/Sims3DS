/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims SimsVille/SimsAntics/primitives/VMTransferFunds.cs
#pragma once
#include <cstdint>
#include "simantics/_files_contract.h"
#include "simantics/engine/VMPrimitiveHandler.h"
#include "simantics/engine/VMPrimitiveOperand.h"
#include "simantics/engine/scopes/VMVariableScope.h"
#include "simantics/model/TSOPlatform/VMAsyncState.h"

namespace FSO { namespace SimAntics { namespace Primitives {

class VMTransferFunds : public Engine::VMPrimitiveHandler {
public:
    Engine::VMPrimitiveExitCode Execute(Engine::VMStackFrame* context, Engine::VMPrimitiveOperand* args) override;
};

enum class VMTransferFundsExpenseType : uint8_t
{ //these are probably used for logging or enforcing special behaviour
    NONE = 0,
    IncomeMisc = 1,
    IncomeFromPlayers = 2,
    IncomeFromObjects = 3,
    IncomeCheat = 4,
    ExpenseMisc = 5,
    ExpenseObjectRefillsAndMaintinance = 6,
    ExpenseFromObjects = 7,
    SimToSim = 8,

    //income from players and objects used for various purposes
    IncomePlayersSkill = 9,
    IncomeObjectsSkill = 10,
    IncomePlayersPizza = 11,
    IncomeObjectsPizza = 12,
    IncomePlayersPaperC = 13,
    IncomeObjectsPaperC = 14,
    IncomePlayersMaze = 15,
    IncomeObjectsMaze = 16,
    IncomePlayersRoulette = 17,
    IncomeObjectsRoulette = 18,
    IncomePlayersSlots = 19,
    IncomeObjectsSlots = 20,
    IncomePlayersBlackjack = 21,
    IncomeObjectsBlackjack = 22,
    IncomePlayersPoker = 23,
    IncomeObjectsPoker = 24,

    ExpenseNPC = 25,
    ExpenseNPCGardener = 26,
    ExpenseNPCMaid = 27,
    ExpenseNPCRepairman = 28,
    ExpenseNPCButler = 29, //???

    IncomeJob = 30,
    IncomeRobotJob = 31,
    IncomeRestaurantJob = 32,
    IncomeClubJob = 33,

    CSRRequestFunds = 34,
    CashInRequest = 35,
    CashIn = 36,
    CashOutRequest = 37,
    CashOut = 38,
    TransferToSharedAccount = 39,

    DoorCharge = 40,
    IncomeTypewriter = 41,
    IncomeEasel = 42,
    IncomeEaselPlayers = 43,
    IncomeChalkboard = 44,
    IncomeCanning = 45,
    IncomeChemistry = 46,
    IncomeGGWorkbench = 47,
    IncomePinata = 48,
    IncomePinataPlayers = 49,
    IncomeTelemarket = 50,
    IncomeTipJarPlayers = 51,
    ExpenseTipJar = 52,
    ExpenseBedHeart = 53,
    IncomeBedHeartPlayers = 54,
    ExpenseBuffet = 55,
    IncomeBuffetPlayers = 56,
    IncomeBuffetVacPlayers = 57,
    ExpenseViffetVac = 58,
    IncomeSlotPlayers = 59,
    ExpenseCheats = 60,
    IncomeFoodCounterPlayers = 61,
    UNKNOWN = 62,
    ExpenseFoodCounter = 63,
    ExpenseSnackMachine = 64,
    IncomeSnackMachinePlayers = 65,
    ExpenseShackMachineRefill = 66,
    ExpenseSodaMachine = 67,
    IncomeSodaMachinePlayers = 68,
    ExpenseSodaMachineRefill = 69,
    IncomePinballPlayers = 70,
    ExpensePinball = 71,
    ExpenseFridgeRefill = 72
};

enum class VMTransferFundsType : uint8_t {
    DEPRICATED_ADD = 0,
    DEPRICATED_SUBTRACT = 1,
    MeToMaxis = 2,
    DEPRICATED_ME_TO_STACK_OBJECT = 3,
    DEPRICATED_ME_TO_STACK_OBJECTS_OWNER = 4,
    MaxisToMe = 5,
    MaxisToStackObjectsOwner = 6,
    StackObjectsOwnerToMaxis = 7,
    DEPRICATED_OWNER_OF_TEMP_0_TO_OWNER_OF_TEMP_1 = 8,
    FromMeToStackObject = 9,
    FromStackObjectToMe = 10,
    FromMaxisToStackObject = 11,
    FromStackObjectToMaxis = 12,
    FromStackObjectToStackObjectOwner = 13,
    DEPRIACTED_FROM_OWNER_OF_OBJECT_TO_OBJECT = 14,
    DEPRICATED_FROM_TEMP_0_TO_OWNER_OF_TEMP_1 = 15,
    DEPRICATED_FROM_OWNER_OF_TEMP_0_TO_TEMP_1 = 16,
    PutStackObjectCashIntoTempXL0 = 17,
    FromStackObjectToLotRoommates = 18,
    FromLotOwnerToMaxis = 19,
    FromMaxisToLotOwner = 20,
    UNKNOWN = 21,
    CharacterToSharedAccount = 22,
    SharedAccountToCharacter = 23,
    MeToLotRoommates = 24
};

enum class VMTransferFundsOldOwner : uint8_t {
    Literal = 0,
    Parameters = 1,
    Local = 2,
    Normal = 3
};

// [Flags]
enum class VMTransferFundsFlags : uint8_t
{
    JustTest = 0x1,
    Subtract = 0x2,
    PayoutMultiplier = 0x4
};
FSO_FLAGS_ENUM(VMTransferFundsFlags)

class VMTransferFundsOperand : public Engine::VMPrimitiveOperand {
public:
    VMTransferFundsOldOwner OldAmountOwner = (VMTransferFundsOldOwner)0;
    Engine::Scopes::VMVariableScope AmountOwner = (Engine::Scopes::VMVariableScope)0;
    uint16_t AmountData = 0;
    VMTransferFundsFlags Flags = (VMTransferFundsFlags)0;
    VMTransferFundsExpenseType ExpenseType = (VMTransferFundsExpenseType)0;
    VMTransferFundsType TransferType = (VMTransferFundsType)0;

    bool JustTest() const { return (Flags & VMTransferFundsFlags::JustTest) > 0; }
    void JustTest(bool value) {
        if (value) Flags |= VMTransferFundsFlags::JustTest;
        else Flags &= ~VMTransferFundsFlags::JustTest;
    }

    // VMPrimitiveOperand Members
    void Read(const uint8_t* bytes) override;
    void Write(uint8_t* bytes) override;

    Engine::Scopes::VMVariableScope GetAmountOwner() const;
};

// Serialization (Deserialize/SerializeInto) dropped, like the core VMAsyncState.
class VMTransferFundsState : public Model::TSOPlatform::VMAsyncState {
public:
    bool Success = false;
    int32_t TransferAmount = 0;
    uint32_t UID1 = 0;
    uint32_t Budget1 = 0;
    uint32_t UID2 = 0;
    uint32_t Budget2 = 0;

    Model::TSOPlatform::VMAsyncStateType StateType() const override { return Model::TSOPlatform::VMAsyncStateType::TransferFunds; }
};

}}}
