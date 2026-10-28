/*This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at
http://mozilla.org/MPL/2.0/.

The Original Code is the SimsLib.

The Initial Developer of the Original Code is
Rhys Simpson. All Rights Reserved.

Contributor(s): Mats 'Afr0' Vederhus
*/
// Translated from FreeSims SimsVille/HIT/HITThread.cs
#include "hit/HITThread.h"
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include "audio/AudioMixer.h"
#include "content/Audio.h"
#include "hit/HITInterpreter.h"
#include "hit/HITVM.h"

namespace FSO { namespace HIT {

using namespace FSO::Files::HIT;

float HITSound::GetVolFactor()
{
    if (VM != nullptr)
        return VM->GetMasterVolume(VolGroup);

    return 1.0f;
}

void HITThread::Interrupt(HITThread* waiter)
{
    Interrupted = true;
    InterruptWaiter = std::static_pointer_cast<HITThread>(waiter->shared_from_this());
    waiter->InterruptBlocker = std::static_pointer_cast<HITThread>(shared_from_this());
    for (size_t i = 0; i < Notes.size(); i++)
    {
        auto& note = Notes[i];
        if (note.EndTick == -1)
        {
            // C#: Duration is in seconds and TickN counts 60 Hz ticks; the C# mixed the two units. The note is
            // allowed to finish its current pass, rounded up to whole durations.
            float tickDuration = note.Duration * 60.0f; // TS1(sims): seconds -> ticks
            if (tickDuration <= 0) tickDuration = 1;
            note.EndTick = note.StartTick + (int)(std::ceil((TickN - note.StartTick) / tickDuration) * tickDuration);
        }
    }
}

void HITThread::Unblock()
{
    InterruptBlocker.reset();
}

bool HITThread::Tick() //true if continue, false if kill
{
    if (Paused) return true;
    TickN++;

    if (EverHadOwners && Owners.size() == 0)
    {
        KillVocals();
        Dead = true;
        return false;
    }

    auto mixer = VM != nullptr ? VM->Mixer : nullptr;
    if (VolumeSet && mixer != nullptr)
    {
        for (size_t i = 0; i < Notes.size(); i++)
        {
            auto& note = Notes[i];
            if (note.EndTick != -1 && TickN > note.EndTick) mixer->Stop(note.instance);
            if (!mixer->IsPlaying(note.instance)) continue; // C#: dispose finished instances
            mixer->SetVolumePan(note.instance, NoteVolume(), Positional() ? Pan : 0.0f);
        }
    }

    VolumeSet = false;

    if (SimpleMode)
    {
        if (PlaySimple)
        {
            NoteOn();
            PlaySimple = false;
        }
        if (NoteActive(LastNote())) return true;
        else
        {
            Dead = true;
            return false;
        }
    }
    else
    {
        try
        {
            // TS1(sims): a budget of instructions per tick guards against a loop that never halts
            // (the C# would hang the game).
            for (int budget = 0; budget < 4096; budget++)
            {
                if (Src == nullptr || PC >= Src->Data.size()) throw std::out_of_range("HIT: PC out of range");
                auto opcode = Src->Data[PC++];
                if (opcode >= HITInterpreter::InstructionCount) opcode = 0;
                auto result = HITInterpreter::Instructions[opcode](this);
                if (result == HITResult::HALT) return true;
                else if (result == HITResult::KILL)
                {
                    Dead = true;
                    return false;
                }
            }
            return true;
        }
        catch (std::exception&)
        {
            // C#: return false without marking the thread dead; dead here so its owners stop referring to it
            // (and loop sounds are restarted by VMEntity.TickSounds).
            Dead = true;
            return false;
        }
    }
}

void HITThread::KillVocals()
{
    auto mixer = VM != nullptr ? VM->Mixer : nullptr;
    if (mixer == nullptr) return;
    for (size_t i = 0; i < Notes.size(); i++)
    {
        if (NoteActive((int)i))
        {
            mixer->Stop(Notes[i].instance);
        }
    }
}

HITThread::HITThread()
{
}

HITThread::HITThread(std::shared_ptr<HITFile> Src, HITVM* VM)
{
    this->Src = Src;
    this->VM = VM;
    Registers.assign(16, 0);
    Registers[1] = 12;
    LocalVar.assign(54, 0);
    ObjectVar.assign(29, 0);

    Notes.clear();
    Owners.clear();

    Stack.clear();
    audContent = FSO::Content::Audio::Get();
}

HITThread::HITThread(uint32_t TrackID)
{
    Owners.clear();
    Notes.clear();
    VM = HITVM::Get(); // TS1(sims): the C# left VM null here (master volume fell back to 1)

    audContent = FSO::Content::Audio::Get();
    SetTrack(TrackID);

    Patch = (ActiveTrack != nullptr) ? ActiveTrack->SoundID : 0;
    SimpleMode = true;
    PlaySimple = true; //play next frame, so we have time to set volumes.
}

HITThread::~HITThread()
{
    Dispose();
}

void HITThread::SetVolume(float volume, float pan)
{
    if (VolumeSet)
    {
        if (volume > Volume)
        {
            Volume = volume;
            PreviousVolume = Volume;
            Pan = pan;
        }
    }
    else
    {
        Volume = volume;
        PreviousVolume = Volume;
        Pan = pan;
    }

    VolumeSet = true;
}

void HITThread::LoadHitlist(uint32_t id)
{
    Hitlist = (audContent != nullptr) ? audContent->GetHitlist(id, ResGroup) : nullptr;
}

uint32_t HITThread::HitlistChoose() //returns a random id from the hitlist
{
    if (Hitlist != nullptr && !Hitlist->IDs.empty()) return Hitlist->IDs[(size_t)HITVM::NextRandom((int)Hitlist->IDs.size())];
    else return 0;
}

uint8_t HITThread::ReadByte()
{
    if (Src == nullptr || PC >= Src->Data.size()) throw std::out_of_range("HIT: read past end");
    return Src->Data[PC++];
}

uint32_t HITThread::ReadUInt32()
{
    uint32_t result = 0;
    result |= ReadByte();
    result |= ((uint32_t)ReadByte() << 8);
    result |= ((uint32_t)ReadByte() << 16);
    result |= ((uint32_t)ReadByte() << 24);
    return result;
}

int32_t HITThread::ReadInt32()
{
    return (int32_t)ReadUInt32();
}

uint32_t HITThread::PeekUInt32() const
{
    if (Src == nullptr || PC + 4 > Src->Data.size()) return 0xFFFFFFFFu;
    auto& d = Src->Data;
    return (uint32_t)d[PC] | ((uint32_t)d[PC + 1] << 8) | ((uint32_t)d[PC + 2] << 16) | ((uint32_t)d[PC + 3] << 24);
}

void HITThread::SetTrack(uint32_t value)
{
    HITResourceGroup* owner = nullptr;
    auto trk = (audContent != nullptr) ? audContent->GetTrack(value, ResGroup, &owner) : nullptr;
    if (trk != nullptr)
    {
        ActiveTrack = trk;
        Patch = ActiveTrack->SoundID;
        if (ResGroup == nullptr) ResGroup = owner;
        // TS1(sims): the track's control group selects the volume slider (C# only set it for TV/music)
        switch (ActiveTrack->ControlGroup)
        {
        case HITControlGroups::kGroupMusic: VolGroup = HITVolumeGroup::MUSIC; break;
        case HITControlGroups::kGroupVox: VolGroup = HITVolumeGroup::VOX; break;
        default: break;
        }
    }
    else
    {
        if (VM != nullptr && VM->Verbose) std::printf("HITThread: Couldn't find track: %u\n", value);
    }
}

void HITThread::Pause()
{
    Paused = true;
    auto mixer = VM != nullptr ? VM->Mixer : nullptr;
    if (mixer == nullptr) return;
    for (auto& note : Notes)
    {
        if (mixer->IsPlaying(note.instance)) mixer->SetPaused(note.instance, true);
    }
}

void HITThread::Resume()
{
    Paused = false;
    auto mixer = VM != nullptr ? VM->Mixer : nullptr;
    if (mixer == nullptr) return;
    for (auto& note : Notes)
    {
        if (mixer->IsPlaying(note.instance)) mixer->SetPaused(note.instance, false);
    }
}

uint32_t HITThread::LoadTrack(int value)
{
    if (Hitlist == nullptr || value < 0 || (size_t)value >= Hitlist->IDs.size()) return 0;
    SetTrack(Hitlist->IDs[(size_t)value]);
    return Hitlist->IDs[(size_t)value];
}

// TS1(sims): positional volume (VMEntity.TickSounds) only applies to tracks that take volume/pan arguments
// (kArgsVolPan / kArgsIdVolPan / kArgsXYZ); stings and UI sounds (kArgsNormal) always play centred at full volume.
bool HITThread::Positional() const
{
    if (ActiveTrack == nullptr) return true;
    return ActiveTrack->ArgType != HITArgs::kArgsNormal;
}

// TS1(sims): volume of a note = positional volume x track volume (0..1024, e.g. TVs 750) x the "vol"
// register the HIT code may set (0x12, 1024 = unity) x ducking. The C# only applied the positional volume.
float HITThread::NoteVolume() const
{
    float v = Positional() ? Volume : 1.0f;
    if (ActiveTrack != nullptr && ActiveTrack->Volume > 0) v *= ActiveTrack->Volume / 1024.0f;
    if (!LocalVar.empty() && LocalVar[0x12 - 0x10] > 0) v *= std::min(1.0f, LocalVar[0x12 - 0x10] / 1024.0f);
    if (VM != nullptr) v *= VM->DuckFactor(this);
    return v;
}

int HITThread::PlayPatch(bool loop)
{
    auto mixer = VM != nullptr ? VM->Mixer : nullptr;
    if (mixer == nullptr || audContent == nullptr) return -1;
    auto sound = audContent->GetSFX(Patch, ResGroup);
    int instance = 0;
    float duration = 0;
    int group = (int)VolGroup;
    if (sound != nullptr)
    {
        instance = mixer->Play(sound, NoteVolume(), Positional() ? Pan : 0.0f, loop, group);
        duration = (float)sound->Seconds();
    }
    else
    {
        // TS1(sims): long WAV samples (the stings) stream from their archive instead of being cached
        auto stream = audContent->GetSFXStream(Patch, ResGroup);
        if (stream != nullptr)
        {
            duration = (float)stream->DurationSeconds();
            instance = mixer->PlayStream(std::move(stream), NoteVolume(), Positional() ? Pan : 0.0f, loop, group);
        }
    }
    NoteFailed = (instance == 0);
    if (instance != 0)
    {
        if (VM != nullptr) VM->NotePlayed(this, Patch);
        Notes.push_back(HITNoteEntry(duration, instance, Patch, TickN));
        return (int)Notes.size() - 1;
    }
    if (VM != nullptr && VM->Verbose) std::printf("HITThread: Couldn't find sound: %u\n", Patch);
    return -1;
}

int HITThread::NoteOn()
{
    return PlayPatch(false);
}

int HITThread::NoteLoop() //todo, make loop again.
{
    // TS1(sims): actually loops (the C# played it once, see its todo)
    return PlayPatch(true);
}

bool HITThread::NoteActive(int note)
{
    if (note == -1 || note >= (int)Notes.size()) return false;
    auto mixer = VM != nullptr ? VM->Mixer : nullptr;
    return mixer != nullptr && mixer->IsPlaying(Notes[(size_t)note].instance);
}

void HITThread::Duck()
{
    VM->Duck(this->DuckPriority());
}

void HITThread::Unduck()
{
    VM->Unduck();
}

void HITThread::LocalVarSet(int location, int value)
{
    switch (location)
    {
    case 0x32: // TS1(sims): "patch" is variable 50 in The Sims' defaultsyms.txt (TSO used 0x12, which is "vol" here)
        Patch = (uint32_t)value;
        break;
    }
}

void HITThread::SetFlags(int value)
{
    ZeroFlag = (value == 0);
    SignFlag = (value < 0);
}

void HITThread::WriteVar(int location, int value)
{
    if (location < 0x10)
    {
        Registers[(size_t)location] = value;
    }
    else if (location < 0x46)
    {
        LocalVarSet(location, value); //invoke any special behaviours, like track switch for setting patch
        LocalVar[(size_t)(location - 0x10)] = value;
    }
    else if (location < 0x64)
    {
        return; //not mapped
    }
    else if (location < 0x88)
    {
        VM->WriteGlobal(location - 0x64, value);
    }
    else if (location < 0x271a)
    {
        return; //not mapped
    }
    else if (location < 0x2737)
    {
        ObjectVar[(size_t)(location - 0x271a)] = value; //this probably should not be valid... but if it is used it may require some reworking to get this to sync across object threads.
    }
}

int HITThread::ReadVar(int location)
{
    if (location < 0)
    {
        return 0;
    }
    else if (location < 0x10)
    {
        return Registers[(size_t)location];
    }
    else if (location < 0x46)
    {
        return LocalVar[(size_t)(location - 0x10)];
    }
    else if (location < 0x64)
    {
        return 0; //not mapped
    }
    else if (location < 0x88)
    {
        return VM->ReadGlobal(location - 0x64);
    }
    else if (location < 0x271a)
    {
        return 0; //not mapped
    }
    else if (location < 0x2737)
    {
        return ObjectVar[(size_t)(location - 0x271a)];
    }
    return 0;
}

void HITThread::JumpToEntryPoint(int TrackID)
{
    auto it = Src->EntryPointByTrackID.find((uint32_t)TrackID);
    if (it == Src->EntryPointByTrackID.end()) throw std::out_of_range("HIT: no entry point");
    PC = it->second;
}

void HITThread::Dispose()
{
    if (auto waiter = InterruptWaiter.lock())
        waiter->Unblock();
    InterruptWaiter.reset();
    // C#: foreach (var note in Notes) note.instance.Dispose(); -- finished voices free themselves in the mixer
}

}} // namespace FSO::HIT
