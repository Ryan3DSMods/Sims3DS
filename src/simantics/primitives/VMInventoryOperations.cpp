/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Primitives.VMInventoryOperations (primitives/VMInventoryOperations.cs)
#include "simantics/primitives/VMInventoryOperations.h"
#include <algorithm>
#include <memory>
#include <typeinfo>
#include "core/io.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMThread.h"
#include "simantics/engine/TSOGlobalLink/IVMTSOGlobalLink.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/VM.h"

namespace FSO { namespace SimAntics { namespace Primitives {
using namespace FSO::SimAntics::Engine;
using FSO::SimAntics::Model::TSOPlatform::VMAsyncState;

namespace {
// STUB(3ds): C# sent `new VMNetAsyncResponseCmd(id, state)` through vm.SendCommand (NetPlay). There is no net
// driver here, so the response command's Execute is applied directly: replace the thread's blocking state if it
// exists and is of exactly the same type.
void ApplyAsyncResponse(VM* vm, int16_t id, std::shared_ptr<VMAsyncState> state)
{
    //if ID is 0, there is no thread to unblock, and we just have to do the extra functionality below.
    if (id != 0)
    {
        VMEntity* obj = vm->GetObjectById(id);
        //we can only update an object's blocking state if it exists and is of the same type exactly.
        if (obj == nullptr || obj->Thread == nullptr || obj->Thread->BlockingState == nullptr
            || typeid(*obj->Thread->BlockingState) != typeid(*state)) return;
        obj->Thread->BlockingState = state;
    }
}

std::shared_ptr<VMInventoryOpState> MakeResponse(bool success, bool writeTemp0, int16_t temp0Value)
{
    auto s = std::make_shared<VMInventoryOpState>();
    s->Responded = true;
    s->Success = success;
    s->WriteTemp0 = writeTemp0;
    s->Temp0Value = temp0Value;
    return s;
}
}

VMPrimitiveExitCode VMInventoryOperations::Execute(VMStackFrame* context, VMPrimitiveOperand* args)
{
    auto operand = static_cast<VMInventoryOperationsOperand*>(args);

    //first of all... are we in an async wait state?
    if (context->Thread->BlockingState != nullptr && dynamic_cast<VMInventoryOpState*>(context->Thread->BlockingState.get()) != nullptr)
    {
        auto keepAlive = context->Thread->BlockingState; // state is read after clearing the blocking state
        auto state = static_cast<VMInventoryOpState*>(keepAlive.get()); //crash here if not intended state.
        if (state->Responded)
        {
            context->Thread->BlockingState = nullptr;
            if (state->WriteTemp0) context->Thread->TempRegisters[0] = state->Temp0Value;
            return (state->Success) ? VMPrimitiveExitCode::GOTO_TRUE : VMPrimitiveExitCode::GOTO_FALSE;
        }
        else return VMPrimitiveExitCode::CONTINUE_NEXT_TICK;
    }

    switch (operand->Mode)
    {
        case VMInventoryOpMode::GetRemainingCapacity:
            context->Thread->TempRegisters[0] = 255; //TODO: hard object limit imposed by db.
            break;
        case VMInventoryOpMode::AddStackObjToInventory:
            // vm initiated inventory transfer.
            // TODO: should this force owner? Crafting Bench uses separate command to change owner before doing this.
            context->Thread->BlockingState = std::make_shared<VMInventoryOpState>();
            if (context->VM()->GlobalLink() != nullptr)
            {
                auto vm = context->VM();
                auto id = context->Caller->ObjectID; //this thread's object id.
                auto mypid = context->Caller->PersistID;
                auto opid = context->StackObject->PersistID;
                auto oid = context->StackObject->ObjectID;
                vm->GlobalLink()->MoveToInventory(vm, context->StackObject->MultitileGroup, [vm, id, opid, oid, mypid](bool success, uint32_t pid)
                {
                    (void)oid; (void)mypid; (void)pid;
                    ApplyAsyncResponse(vm, id, MakeResponse(success, false, 0));
                    if (success)
                    {
                        if (opid == 0)
                        {
                            //we got a persist id when we got added to inventory. quickly register it before the send to inventory is run.
                            // STUB(3ds): vm.ForwardCommand(new VMNetUpdatePersistStateCmd { ObjectID = oid, PersistID = pid }) — NetPlay.
                        }
                        // STUB(3ds): vm.ForwardCommand(new VMNetSendToInventoryCmd { Verified = true, Success = true,
                        //            ObjectPID = pid, ActorUID = mypid }) — NetPlay.
                    }
                });
            }
            return VMPrimitiveExitCode::CONTINUE_NEXT_TICK;
        case VMInventoryOpMode::RemoveTemp0ObjOfTypeFromInventory:
            //needs a rather complex db operation.
            //must first select x objects. must then delete them all.
            //if deleted# != selected#, reverse transaction and declare it failed.
            //same for selected# being < x.
            context->Thread->BlockingState = std::make_shared<VMInventoryOpState>();
            if (context->VM()->GlobalLink() != nullptr)
            {
                //get id and vm now to avoid race conditions
                auto id = context->Caller->ObjectID; //this thread's object id.
                auto vm = context->VM();
                context->VM()->GlobalLink()->ConsumeInventory(context->VM(), context->Caller->PersistID, operand->GUID, 1, context->Thread->TempRegisters[0],
                    [vm, id](bool success, int32_t count)
                    {
                        (void)count;
                        ApplyAsyncResponse(vm, id, MakeResponse(success, false, 0));
                    });
            }
            return VMPrimitiveExitCode::CONTINUE_NEXT_TICK;
        case VMInventoryOpMode::CountObjectsOfType:
            context->Thread->BlockingState = std::make_shared<VMInventoryOpState>();
            if (context->VM()->GlobalLink() != nullptr)
            {
                //get id and vm now to avoid race conditions
                auto id = context->Caller->ObjectID; //this thread's object id.
                auto vm = context->VM();
                context->VM()->GlobalLink()->ConsumeInventory(context->VM(), context->Caller->PersistID, operand->GUID, 0, 0,
                    [vm, id](bool success, int32_t count)
                {
                    ApplyAsyncResponse(vm, id, MakeResponse(success, true, (int16_t)count));
                });
            }
            return VMPrimitiveExitCode::CONTINUE_NEXT_TICK;

        default:
            break;
    }
    return VMPrimitiveExitCode::GOTO_TRUE;
}

void VMInventoryOperationsOperand::Read(const uint8_t* bytes)
{
    st::Reader io(bytes, 8, st::Endian::Little);
    GUID = io.u32();
    Mode = (VMInventoryOpMode)io.u8();
}

void VMInventoryOperationsOperand::Write(uint8_t* bytes)
{
    st::Writer io(st::Endian::Little);
    io.WriteUInt32(GUID);
    io.WriteByte((uint8_t)Mode);
    auto& buf = io.ToArray();
    std::copy(buf.begin(), buf.begin() + std::min<size_t>(buf.size(), OPERAND_SIZE), bytes);
}

}}}
