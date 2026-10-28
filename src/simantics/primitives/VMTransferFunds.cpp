/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims SimsVille/SimsAntics/primitives/VMTransferFunds.cs
#include "simantics/primitives/VMTransferFunds.h"
#include <climits>
#include <cstring>
#include <memory>
#include <stdexcept>
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMThread.h"
#include "simantics/engine/VMMemory.h"
#include "simantics/engine/VMSimanticsException.h"
#include "simantics/engine/TSOGlobalLink/IVMTSOGlobalLink.h"
#include "simantics/engine/TSOGlobalLink/VMTSOGlobalLinkStub.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMGameObject.h"
#include "simantics/model/TSOPlatform/VMTSOObjectState.h"
#include "simantics/model/TSOPlatform/VMTSOLotState.h"
#include "simantics/VM.h"
#include "core/io.h"

namespace FSO { namespace SimAntics { namespace Primitives {
using namespace FSO::SimAntics::Engine;
using namespace FSO::SimAntics::Engine::Utils;
using namespace FSO::SimAntics::Engine::Scopes;
using namespace FSO::SimAntics::Model::TSOPlatform;

namespace {
// C# dereferenced these without checks (NullReferenceException, caught by the thread); keep that behaviour.
template<class T> T* NotNull(T* ptr, VMStackFrame* context, const char* what) {
    if (ptr == nullptr) throw VMSimanticsException(std::string("Transfer Funds: null ") + what + ".", context);
    return ptr;
}

uint32_t StackObjectOwnerID(VMStackFrame* context) {
    // C#: ((VMTSOObjectState)context.StackObject.TSOState).OwnerID
    auto state = NotNull(context->StackObject, context, "stack object")->TSOState();
    return static_cast<VMTSOObjectState*>(NotNull(state, context, "stack object TSO state"))->OwnerID;
}

// STUB(3ds): C# sent `new VMNetAsyncResponseCmd(id, state)` through vm.SendCommand (NetPlay), which on execution
// replaced the thread's blocking state when it was of the same type. There is no net driver here, so the
// response command's Execute is applied directly. (Its "update visual budgets" step only runs when the link is
// not a VMTSOGlobalLinkStub; the only link here is the stub, whose budgets are already the reference ones.)
void ApplyAsyncResponse(VM* vm, int16_t id, std::shared_ptr<VMTransferFundsState> state) {
    //if ID is 0, there is no thread to unblock, and we just have to do the extra functionality below.
    if (id != 0) {
        VMEntity* obj = vm->GetObjectById(id);
        //we can only update an object's blocking state if it exists and is of the same type exactly.
        if (obj == nullptr || obj->Thread == nullptr || obj->Thread->BlockingState == nullptr
            || dynamic_cast<VMTransferFundsState*>(obj->Thread->BlockingState.get()) == nullptr) return;
        obj->Thread->BlockingState = state;
    }
}
}

VMPrimitiveExitCode VMTransferFunds::Execute(VMStackFrame* context, VMPrimitiveOperand* args) {
    auto operand = static_cast<VMTransferFundsOperand*>(args);

    //first of all... are we in an async wait state?
    if (context->Thread->BlockingState != nullptr && dynamic_cast<VMTransferFundsState*>(context->Thread->BlockingState.get()) != nullptr)
    {
        auto state = static_cast<VMTransferFundsState*>(context->Thread->BlockingState.get()); //crash here if not intended state.
        if (state->Responded)
        {
            auto keepAlive = context->Thread->BlockingState; // state is read after clearing the blocking state
            context->Thread->BlockingState = nullptr;
            if (operand->TransferType == VMTransferFundsType::PutStackObjectCashIntoTempXL0)
            {
                context->Thread->TempXL[0] = (int32_t)state->Budget1;
                return VMPrimitiveExitCode::GOTO_TRUE;
            }
            else
            {
                context->Thread->TempXL[0] = state->TransferAmount; //final transfer amount put into tempxl0
                return (state->Success) ? VMPrimitiveExitCode::GOTO_TRUE : VMPrimitiveExitCode::GOTO_FALSE;
            }
        }
        else return VMPrimitiveExitCode::CONTINUE_NEXT_TICK;
    }

    auto amount = VMMemory::GetBigVariable(context, operand->GetAmountOwner(), (int16_t)operand->AmountData);

    uint32_t source = UINT32_MAX;
    uint32_t target = UINT32_MAX;

    switch (operand->TransferType)
    {
        case VMTransferFundsType::MeToMaxis:
            source = context->Caller->PersistID; break;
        case VMTransferFundsType::MaxisToMe:
            target = context->Caller->PersistID; break;
        case VMTransferFundsType::MaxisToStackObjectsOwner:
            if (dynamic_cast<VMGameObject*>(context->StackObject) != nullptr) target = StackObjectOwnerID(context);
            break;
        case VMTransferFundsType::StackObjectsOwnerToMaxis:
            if (dynamic_cast<VMGameObject*>(context->StackObject) != nullptr) source = StackObjectOwnerID(context);
            break;
        case VMTransferFundsType::FromMeToStackObject:
            source = context->Caller->PersistID; target = NotNull(context->StackObject, context, "stack object")->PersistID; break;
        case VMTransferFundsType::FromStackObjectToMe:
            source = NotNull(context->StackObject, context, "stack object")->PersistID; target = context->Caller->PersistID; break;
        case VMTransferFundsType::FromMaxisToStackObject:
            target = NotNull(context->StackObject, context, "stack object")->PersistID; break;
        case VMTransferFundsType::FromStackObjectToMaxis:
            source = NotNull(context->StackObject, context, "stack object")->PersistID; break;
        case VMTransferFundsType::FromStackObjectToStackObjectOwner:
            source = NotNull(context->StackObject, context, "stack object")->PersistID;
            if (dynamic_cast<VMGameObject*>(context->StackObject) != nullptr) target = StackObjectOwnerID(context);
            break;
        case VMTransferFundsType::PutStackObjectCashIntoTempXL0:
            source = NotNull(context->StackObject, context, "stack object")->PersistID; amount = 0;
            break;
        case VMTransferFundsType::FromLotOwnerToMaxis:
            source = NotNull(context->VM()->TSOState(), context, "lot TSO state")->OwnerID;
            break;
        case VMTransferFundsType::FromMaxisToLotOwner:
            target = NotNull(context->VM()->TSOState(), context, "lot TSO state")->OwnerID;
            break;
        default:
            return VMPrimitiveExitCode::GOTO_TRUE;

    }
    //TODO: LotRoommates transactions, shared account (but that's unused...)

    //if we're in a check tree, transactions must be instant and test only.
    if (context->Thread->IsCheck)
    {
        if (operand->TransferType == VMTransferFundsType::PutStackObjectCashIntoTempXL0)
        {
            context->Thread->TempXL[0] = (int32_t)NotNull(context->StackObject->TSOState(), context, "stack object TSO state")->Budget.Value;
            return VMPrimitiveExitCode::GOTO_TRUE;
        }
        if (!operand->JustTest()) return VMPrimitiveExitCode::GOTO_FALSE;
        context->Thread->TempXL[0] = amount;
        if (context->VM()->CheckGlobalLink.PerformTransaction(context->VM(), true, source, target, amount))
            return VMPrimitiveExitCode::GOTO_TRUE;
        return VMPrimitiveExitCode::GOTO_FALSE;
    }
    else
    {
        //otherwise, we can wait... make an async call to the transaction handler to process our request.
        //the response will be dealt with on a later tick.

        context->Thread->BlockingState = std::make_shared<VMTransferFundsState>();

        // STUB(3ds): VM.GlobalLink (the TSO server link) is always null on 3DS. In FreeSims single player the
        // local server driver used a VMTSOGlobalLinkStub as its GlobalLink; fall back to the VM's stub so the
        // transaction still happens (otherwise this primitive would block forever).
        Engine::TSOTransaction::IVMTSOGlobalLink* link = context->VM()->GlobalLink();
        if (link == nullptr) link = &context->VM()->CheckGlobalLink;

        if (link != nullptr)
        {
            //get id and vm now to avoid race conditions
            auto id = context->Caller->ObjectID; //this thread's object id
            auto vm = context->VM();
            link->PerformTransaction(context->VM(), operand->JustTest(), source, target, amount,
                [id, vm](bool success, int32_t transferAmount, uint32_t uid1, uint32_t budget1, uint32_t uid2, uint32_t budget2)
                {
                    auto state = std::make_shared<VMTransferFundsState>();
                    state->Responded = true;
                    state->Success = success;
                    state->TransferAmount = transferAmount;
                    state->UID1 = uid1;
                    state->Budget1 = budget1;
                    state->UID2 = uid2;
                    state->Budget2 = budget2;
                    ApplyAsyncResponse(vm, id, state);
                });
        }
        return VMPrimitiveExitCode::CONTINUE_NEXT_TICK;
    }
}

void VMTransferFundsOperand::Read(const uint8_t* bytes) {
    st::Reader io(bytes, 8, st::Endian::Little);
    OldAmountOwner = (VMTransferFundsOldOwner)io.u8();
    AmountOwner = (VMVariableScope)io.u8();
    AmountData = io.u16();
    Flags = (VMTransferFundsFlags)io.u8();
    io.u8();
    ExpenseType = (VMTransferFundsExpenseType)io.u8();
    TransferType = (VMTransferFundsType)io.u8();
}

void VMTransferFundsOperand::Write(uint8_t* bytes) {
    // C#: BinaryWriter over new MemoryStream(bytes) (little endian, fixed 8 bytes).
    // Faithful to the C#: AmountData is not written and Flags is written as a uint (4 bytes).
    st::Writer io(st::Endian::Little);
    io.WriteByte((uint8_t)OldAmountOwner);
    io.WriteByte((uint8_t)AmountOwner);
    io.WriteUInt32((uint32_t)Flags);
    io.WriteByte((uint8_t)ExpenseType);
    io.WriteByte((uint8_t)TransferType);
    auto& buf = io.ToArray();
    if (buf.size() > 8) throw std::runtime_error("Memory stream is not expandable.");
    std::memcpy(bytes, buf.data(), buf.size());
}

VMVariableScope VMTransferFundsOperand::GetAmountOwner() const {
    switch (OldAmountOwner) {
        case VMTransferFundsOldOwner::Literal:
            return VMVariableScope::Literal;
        case VMTransferFundsOldOwner::Parameters:
            return VMVariableScope::Parameters;
        case VMTransferFundsOldOwner::Local:
            return VMVariableScope::Local;

        default:
        case VMTransferFundsOldOwner::Normal:
            return AmountOwner;
    }
}

}}}
