/*This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
If a copy of the MPL was not distributed with this file, You can obtain one at
http://mozilla.org/MPL/2.0/.

The Original Code is the SimsLib.

The Initial Developer of the Original Code is
Rhys Simpson. All Rights Reserved.

Contributor(s): Mats 'Afr0' Vederhus
*/
// Translated from FreeSims SimsVille/HIT/HITThread.cs (namespace TSO.HIT -> FSO::HIT)
#pragma once
#include <cstdint>
#include <memory>
#include <vector>
#include "files/HIT/HITFile.h"
#include "files/HIT/Hitlist.h"
#include "hit/HITResourceGroup.h"
#include "hit/HITSound.h"

namespace FSO { namespace Content { class Audio; } }

namespace FSO { namespace HIT {

struct HITNoteEntry
{
    int instance = 0;     // C#: SoundEffectInstance -> a simaudio::AudioMixer voice handle
    uint32_t SoundID = 0; //This is for killing specific sounds, see HITInterpreter.SeqGroupKill.
    bool ended = false;
    bool started = false;
    int StartTick = 0;
    int EndTick = -1;
    float Duration = 0;

    HITNoteEntry() {}
    HITNoteEntry(float durationSeconds, int instance, uint32_t SoundID, int startTick)
    {
        this->instance = instance;
        this->SoundID = SoundID;
        this->ended = false;
        this->started = false;
        this->EndTick = -1;
        this->Duration = durationSeconds;
        this->StartTick = startTick;
    }
};

class HITThread : public HITSound
{
public:
    uint32_t PC = 0; //program counter
    std::shared_ptr<FSO::Files::HIT::HITFile> Src;
private:
    std::shared_ptr<FSO::Files::HIT::Hitlist> Hitlist;
    std::vector<int32_t> Registers; //includes args, vars, whatever "h" is up to 0xf
    std::vector<int32_t> LocalVar; //the sims online set, 0x10 "argstyle" up to 0x45 orientz. are half of these even used? no. but even in the test files? no
public:
    std::vector<int32_t> ObjectVar; //IsInsideViewFrustrum 0x271a to Neatness 0x2736. Set by object on thread invocation.

    int32_t LoopPointer = -1;
    int32_t WaitRemain = -1;
    bool Paused = false;
    int TickN = 0;

private:
    bool SimpleMode = false; //certain sounds play with no HIT.
    bool PlaySimple = false;

public:
    bool HasSetLoop = false;
    bool LoopDefined = false;
    bool ThreadDead = false;

    bool Interruptable() const
    {
        return !LocalVar.empty() && (LocalVar[0x21] > 0 || LocalVar[0x27] > 0);
    }
    bool Interrupted = false;
    std::weak_ptr<HITThread> InterruptWaiter;
    std::weak_ptr<HITThread> InterruptBlocker;
private:
    uint32_t Patch = 0; //sound id

    std::vector<HITNoteEntry> Notes;
public:
    int LastNote() const
    {
        return (int)Notes.size() - 1;
    }

    bool ZeroFlag = false; //flags set by instructions
    bool SignFlag = false;

    std::vector<int32_t> Stack;

    // TS1(sims): the bank whose code/hitlists/patches this thread uses, the code address it started at
    // (default loop point), the sequence group it joined (seqgroup_kill) and the instruction budget per tick.
    HITResourceGroup* ResGroup = nullptr;
    uint32_t EntryPC = 0;
    int32_t SeqGroup = -1;
    bool NoteFailed = false; // the last note_on found no sample

private:
    FSO::Content::Audio* audContent = nullptr;

public:
    void Interrupt(HITThread* waiter);
    void Unblock();

    bool Tick() override; //true if continue, false if kill

    /// <summary>
    /// Kills all playing sounds.
    /// </summary>
    void KillVocals();

    HITThread();
    HITThread(std::shared_ptr<FSO::Files::HIT::HITFile> Src, HITVM* VM);
    explicit HITThread(uint32_t TrackID);
    ~HITThread() override;

    void SetVolume(float volume, float pan) override;

    void LoadHitlist(uint32_t id);
    uint32_t HitlistChoose(); //returns a random id from the hitlist

    uint8_t ReadByte();
    uint32_t ReadUInt32();
    int32_t ReadInt32();
    /// TS1(sims): peek without moving the PC
    uint32_t PeekUInt32() const;

    void SetTrack(uint32_t value);

    void Pause() override;
    void Resume() override;

    /// <summary>
    /// Loads a track from the current HitList.
    /// </summary>
    /// <param name="value">ID of track to load.</param>
    uint32_t LoadTrack(int value);

    int NoteOn();

    /// <summary>
    /// Plays a note and loops it.
    /// </summary>
    /// <returns>-1 if unsuccessful, or the number of notes in this thread if successful.</returns>
    int NoteLoop(); //todo, make loop again.

    /// <summary>
    /// Is a note active?
    /// </summary>
    bool NoteActive(int note);

    /// <summary>
    /// Signals the VM to duck all threads with a higher ducking priority than this one.
    /// </summary>
    void Duck();

    /// <summary>
    /// Signals to the VM to unduck all threads that are currently ducked.
    /// </summary>
    void Unduck();

private:
    void LocalVarSet(int location, int value);
    int PlayPatch(bool loop);
    float NoteVolume() const;
    bool Positional() const;

public:
    void SetFlags(int value);
    void WriteVar(int location, int value);
    int ReadVar(int location);
    void JumpToEntryPoint(int TrackID);

    void Dispose() override;

    /// TS1(sims): the thread's current patch (sound id), for logs.
    uint32_t CurrentPatch() const { return Patch; }
    /// TS1(sims): stops the notes and marks the thread dead (seqgroup_kill of another actor's vocals).
    void Kill() { KillVocals(); Dead = true; }
};

}} // namespace FSO::HIT
