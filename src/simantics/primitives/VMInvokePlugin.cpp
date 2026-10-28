/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Primitives.VMInvokePlugin (primitives/VMInvokePlugin.cs)
// STUB(3ds): VM.EODHost (the server-side EOD plugin host, FSO.SimAntics.NetPlay.EODs.VMEODHost) does not exist on 3DS;
// it is only present on a TSO server, so every `context.VM.EODHost != null` / `context.VM.IsServer` branch is dead here.
// The thread-side plugin state machine (VMEODPluginThreadState, events, temps) is kept.
#include "simantics/primitives/VMInvokePlugin.h"
#include <memory>
#include "core/io.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMThread.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"

namespace FSO { namespace SimAntics { namespace Primitives {

using namespace FSO::SimAntics::Engine;

const std::unordered_set<uint32_t> VMInvokePlugin::IdleEvents =
{
    0x2a6356a0,
    0x4a5be8ab,
    0xea47ae39
};

VMPrimitiveExitCode VMInvokePlugin::Execute(VMStackFrame* context, VMPrimitiveOperand* args)
{
    //this probably shouldn't use an AsyncWaitState. I can see this having its own state.
    auto operand = static_cast<VMInvokePluginOperand*>(args);

    auto localEvt = context->Locals[operand->EventLocal];
    if (localEvt < 0)
    {
        // STUB(3ds): if (context.VM.EODHost != null) context.VM.EODHost.ForceDisconnectObj(context.Caller);
        context->Thread->EODConnection = nullptr;
    }

    if (context->Thread->EODConnection != nullptr)
    {
        // STUB(3ds): if (context.VM.EODHost != null) context.VM.EODHost.SimanticsDeliver(localEvt, context.Caller);
        (void)localEvt;

        auto eodState = context->Thread->EODConnection;
        while (eodState->Events.size() > 0)
        {
            //pop off an event.
            auto evt = eodState->Events[0];
            eodState->Events.erase(eodState->Events.begin());
            if (evt.Code < 0) continue;
            context->Locals[operand->EventLocal] = evt.Code;
            for (size_t i = 0; i < evt.Data.size(); i++)
            {
                context->Thread->TempRegisters.at(i) = evt.Data[i];
            }
            return VMPrimitiveExitCode::GOTO_FALSE;
        }

        if (eodState->Ended)
        {
            context->Thread->EODConnection = nullptr;
            return VMPrimitiveExitCode::GOTO_TRUE;
        }
    } else if (context->Locals[operand->EventLocal] == -2) {
        //not connected. initiate connection.
        auto objID = context->Locals[operand->ObjectLocal];
        auto personID = context->Locals[operand->PersonLocal];

        if (operand->Joinable)
        {
            //does the target object exist? does it have a server already? make one.
            auto serverObj = context->VM()->GetObjectById(objID);
            if (serverObj == nullptr) return VMPrimitiveExitCode::GOTO_TRUE; //no server
            if (serverObj->Thread->EODConnection == nullptr)
            {
                //connect the server object

                auto serverState = std::make_shared<VMEODPluginThreadState>();
                serverState->ObjectID = objID;
                serverState->AvatarID = 0;
                serverState->Joinable = true;
                serverState->Ended = false;
                serverObj->Thread->EODConnection = serverState;
                if (context->VM()->IsServer())
                {
                    // STUB(3ds): context.VM.EODHost.Connect(operand.PluginID, serverObj, serverObj, null, true, context.VM);
                }
            }
        }

        auto state = std::make_shared<VMEODPluginThreadState>();
        state->ObjectID = objID;
        state->AvatarID = personID;
        state->Joinable = operand->Joinable;
        state->Ended = false;
        context->Thread->EODConnection = state;
        if (context->VM()->IsServer())
        {
            // STUB(3ds): context.VM.EODHost.Connect(operand.PluginID, context.Caller, context.VM.GetObjectById(objID),
            //     (VMAvatar)context.VM.GetObjectById(personID), operand.Joinable, context.VM);
        }

    } else
    {
        return VMPrimitiveExitCode::GOTO_TRUE;
    }

    context->Locals[operand->EventLocal] = 0;
    if (IdleEvents.count(operand->PluginID) > 0)
    {
        context->Thread->TempRegisters[0] = 0;
        return VMPrimitiveExitCode::GOTO_FALSE;
    }
    else return VMPrimitiveExitCode::CONTINUE_NEXT_TICK;
}

void VMInvokePluginOperand::Read(const uint8_t* bytes)
{
    st::Reader io(bytes, 8, st::Endian::Little);
    PersonLocal = io.ReadByte();
    ObjectLocal = io.ReadByte();
    EventLocal = io.ReadByte();
    Joinable = io.ReadByte() > 0;

    PluginID = io.ReadUInt32();
}

void VMInvokePluginOperand::Write(uint8_t* bytes) { (void)bytes; }

}}}
