/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims SimsVille/SimsAntics/primitives/VMSleep.cs
#include "simantics/entities/VMEntity.h" // FIX(sims): Caller->Dead
#include "simantics/primitives/VMSleep.h"
#include <cstring>
#include <algorithm>
#include "core/io.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMThread.h"

namespace FSO { namespace SimAntics { namespace Primitives {

using namespace FSO::SimAntics::Engine;

VMPrimitiveExitCode VMSleep::Execute(VMStackFrame* context, VMPrimitiveOperand* args)
{
    auto operand = static_cast<VMSleepOperand*>(args);

    if (context->Thread->Interrupt)
    {
        context->Thread->Interrupt = false;
        return VMPrimitiveExitCode::GOTO_TRUE;
    }

    // C# short[] index throws when out of range (.at()); --short wraps like C#.
    auto& arg = context->Args.at(operand->StackVarToDec);
    // TS1(sims): the tick count is unsigned in The Sims: the expansion controllers sleep "-29536" (36000 ticks)
    // (Controller - Insanity). Read signed, that returned at once and the controller spun in its wait loop
    // until the 500k-primitive guard reset it. Same timing for N >= 0.
    // A tree that sleeps on a parameter it already counted down re-enters with -1, -2, ... and must return at once
    // (FreeSims/FreeSO signed behaviour; base game school/car portal trees rely on it), so only values far below
    // zero are read as large unsigned counts. During such a long count the value passes from -32768 to 32767 and
    // continues as an ordinary signed count.
    bool longWait = arg < -1000;
    if (!longWait) arg = (int16_t)(arg - 1);
    else arg = (int16_t)((uint16_t)arg - 1);
    if (!longWait && arg < 0)
        // FIX(sims): FreeSO — a dead caller (it removed itself) yields instead of looping on in this tick.
        return (context->Caller != nullptr && context->Caller->Dead) ? VMPrimitiveExitCode::GOTO_TRUE_NEXT_TICK : VMPrimitiveExitCode::GOTO_TRUE;
    else
        return VMPrimitiveExitCode::CONTINUE_NEXT_TICK;
}

// region VMPrimitiveOperand Members
void VMSleepOperand::Read(const uint8_t* bytes)
{
    st::Reader io(bytes, VMPrimitiveOperand::OPERAND_SIZE, st::Endian::Little);
    StackVarToDec = io.ReadInt16();
}

void VMSleepOperand::Write(uint8_t* bytes)
{
    st::Writer io(st::Endian::Little);
    io.WriteInt16(StackVarToDec);
    std::memcpy(bytes, io.ToArray().data(), std::min(io.ToArray().size(), VMPrimitiveOperand::OPERAND_SIZE));
}
// endregion

}}}
