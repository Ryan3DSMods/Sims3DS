/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims SimsVille/SimsAntics/primitives/VMStopAllSounds.cs
#include "simantics/primitives/VMStopAllSounds.h"
#include "simantics/engine/VMStackFrame.h"
#include "simantics/entities/VMEntity.h"
#include "core/io.h"

namespace FSO { namespace SimAntics { namespace Primitives {

Engine::VMPrimitiveExitCode VMStopAllSounds::Execute(Engine::VMStackFrame* context, Engine::VMPrimitiveOperand* args) {
    auto operand = static_cast<VMStopAllSoundsOperand*>(args);

    auto owner = (operand->Flags == 1) ? context->StackObject : context->Caller;
    if (owner == nullptr) return Engine::VMPrimitiveExitCode::GOTO_TRUE;
    auto& threads = owner->SoundThreads;

    for (size_t i = 0; i < threads.size(); i++) {
        if (threads[i].Sound != nullptr) threads[i].Sound->RemoveOwner(owner->ObjectID);
    }
    threads.clear();

    return Engine::VMPrimitiveExitCode::GOTO_TRUE;
}

void VMStopAllSoundsOperand::Read(const uint8_t* bytes) {
    st::Reader io(bytes, 8, st::Endian::Little);
    Flags = io.u8();
}

void VMStopAllSoundsOperand::Write(uint8_t* bytes) {
    // C#: BinaryWriter over new MemoryStream(bytes) (little endian, fixed 8 bytes)
    bytes[0] = Flags;
}

}}}
