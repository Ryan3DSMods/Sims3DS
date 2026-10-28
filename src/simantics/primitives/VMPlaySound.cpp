/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims SimsVille/SimsAntics/primitives/VMPlaySound.cs
#include "simantics/primitives/VMPlaySound.h"
#include <cstring>
#include <stdexcept>
#include <string>
#include "simantics/engine/VMStackFrame.h"
#include "simantics/engine/VMSimanticsException.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMAvatar.h"
#include "simantics/model/Sound/VMSoundEntry.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"
#include "content/ContentManager.h"
#include "files/formats/iff/chunks/FWAV.h"
#include "core/io.h"
#include "files/HIT/HITConstants.h"
#include "hit/HITThread.h"
#include "hit/HITTVOn.h"
#include "hit/HITVM.h"

namespace FSO { namespace SimAntics { namespace Primitives {
using namespace FSO::SimAntics::Engine;
using namespace FSO::SimAntics::Model::Sound;
using FSO::Files::Formats::IFF::Chunks::FWAV;
using FSO::HIT::HITVM;
using FSO::HIT::HITThread;
using FSO::Files::HIT::HITEvents;

VMPrimitiveExitCode VMPlaySound::Execute(VMStackFrame* context, VMPrimitiveOperand* args) {
    if (!VM::UseWorld()) return VMPrimitiveExitCode::GOTO_TRUE;

    auto operand = static_cast<VMPlaySoundOperand*>(args);
    FWAV* fwav = context->ScopeResource()->Get<FWAV>(operand->EventID);
    if (fwav == nullptr) fwav = context->VM()->Context->Globals->Resource->Get<FWAV>(operand->EventID);

    auto owner = (operand->StackObjAsSource()) ? context->StackObject : context->Caller;
    // FIX(sims): FreeSO returns true for a missing owner (FreeSims threw a NullReferenceException). The Grim Reaper's
    // 'play loop sound' runs with no stack object after a death: it reset ~4,000 times per game day before.
    if (owner == nullptr) return VMPrimitiveExitCode::GOTO_TRUE;

    VMSoundEntry* lastThread = nullptr; // C#: owner.SoundThreads.FirstOrDefault(x => x?.Name == fwav?.Name)
    if (fwav != nullptr) {
        for (auto& x : owner->SoundThreads) {
            if (x.Name == fwav->Name) { lastThread = &x; break; }
        }
    }

    if (lastThread != nullptr)
    {
        auto lt = std::dynamic_pointer_cast<HITThread>(lastThread->Sound);
        if (lt != nullptr && lt->Interruptable() == true) lastThread = nullptr;
    }

    // TS1(sims): events that act on the calling object's own sounds instead of starting one:
    // tv_turn_off / stereo_turnoff (kKillInstance), radio_next_song (kSndobCancelNote), kSndobStop / kSndobKill.
    if (fwav != nullptr && HITVM::Get() != nullptr)
    {
        auto reg = HITVM::Get()->GetEvent(fwav->Name);
        if (reg != nullptr && (reg->EventType == HITEvents::kKillInstance || reg->EventType == HITEvents::kSoundobStop
            || reg->EventType == HITEvents::kSoundobKill || reg->EventType == HITEvents::kTurnOffTV))
        {
            // the station (TV channel / radio) of this object; other sounds (the switch click) keep playing
            bool all = (reg->EventType == HITEvents::kSoundobStop || reg->EventType == HITEvents::kSoundobKill);
            auto& threads = owner->SoundThreads;
            for (size_t i = 0; i < threads.size(); i++)
            {
                if (threads[i].Sound == nullptr) continue;
                if (!all && std::dynamic_pointer_cast<FSO::HIT::HITTVOn>(threads[i].Sound) == nullptr) continue;
                threads[i].Sound->RemoveOwner(owner->ObjectID);
                threads.erase(threads.begin() + i); i--;
            }
            if (HITVM::Get()->EventLog) HITVM::Get()->EventLog(fwav->Name, "kill instance " + std::to_string(owner->ObjectID));
            return VMPrimitiveExitCode::GOTO_TRUE;
        }
        if (reg != nullptr && reg->EventType == HITEvents::kSoundobCancelNote)
        {
            for (auto& t : owner->SoundThreads)
            {
                auto tv = std::dynamic_pointer_cast<FSO::HIT::HITTVOn>(t.Sound);
                if (tv != nullptr) tv->PlayNext(); // next song / next clip
            }
            return VMPrimitiveExitCode::GOTO_TRUE;
        }
    }

    if (fwav != nullptr && lastThread == nullptr && HITVM::Get() != nullptr) {
        auto thread = HITVM::Get()->PlaySoundEvent(fwav->Name);
        if (thread != nullptr) {
            if (owner == nullptr) return VMPrimitiveExitCode::GOTO_TRUE;
            if (!thread->AlreadyOwns(owner->ObjectID)) thread->AddOwner(owner->ObjectID);

            auto ava = dynamic_cast<VMAvatar*>(owner);
            auto hthread = std::dynamic_pointer_cast<HITThread>(thread);
            if (ava != nullptr && hthread != nullptr) ava->SubmitHITVars(hthread.get());
            // TS1(sims): every source passes its instance (arg1) for seqgroup_kill / getsrcdatafield
            else if (hthread != nullptr) VMAvatar::SubmitSourceVars(hthread.get(), owner->ObjectID);

            VMSoundEntry entry;
            entry.Sound = thread;
            entry.Pan = !operand->NoPan();
            entry.Zoom = !operand->NoZoom();
            entry.Loop = operand->Loop();
            entry.Name = fwav->Name;
            owner->SoundThreads.push_back(entry);
            if (owner->Thread != nullptr) owner->TickSounds();
        }
    }

    return VMPrimitiveExitCode::GOTO_TRUE;
}

void VMPlaySoundOperand::Read(const uint8_t* bytes) {
    st::Reader io(bytes, 8, st::Endian::Little);
    EventID = io.u16();
    Pad = io.u16();
    Flags = io.u8();
    Volume = io.u8();
}

void VMPlaySoundOperand::Write(uint8_t* bytes) {
    // C#: BinaryWriter over new MemoryStream(bytes) (little endian, fixed 8 bytes)
    st::Writer io(st::Endian::Little);
    io.WriteUInt16(EventID);
    io.WriteUInt16(Pad);
    io.WriteByte(Flags);
    io.WriteByte(Volume);
    auto& buf = io.ToArray();
    if (buf.size() > 8) throw std::runtime_error("Memory stream is not expandable.");
    std::memcpy(bytes, buf.data(), buf.size());
}

}}}
