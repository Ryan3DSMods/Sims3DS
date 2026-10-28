/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims SimsVille/HIT/HITVM.cs (namespace TSO.HIT -> FSO::HIT)
//
// The HIT virtual machine: maps sound *events* (the names in objects' FWAV chunks, animation "sound" events and the
// UI's ui_* names) to HIT threads that run the bank's bytecode and play notes through the sims mixer.
// TS1(sims): the banks are The Sims 1's .hot/.hsm/.hit files (see Content::Audio) instead of TSO's newmain,
// relationships, tsoep5, tsov2, tsov3 and turkey groups.
#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "files/HIT/HITConstants.h"
#include "hit/HITEventRegistration.h"
#include "hit/HITResourceGroup.h"
#include "hit/HITSound.h"

namespace simaudio { class AudioMixer; }

namespace FSO { namespace HIT {

class HITThread;
class HITTVOn;
class FSCPlayer;

class HITVM
{
private:
    static std::unique_ptr<HITVM> INSTANCE;

public:
    static HITVM* Get()
    {
        return INSTANCE.get(); //there can be only one!
    }

    /// Needs Content::Audio to be initialised first. The mixer receives every note.
    static void Init(simaudio::AudioMixer* mixer = nullptr);
    static void Shutdown();
    /// C# new Random().Next(max)
    static int NextRandom(int max);

    //non static stuff

    simaudio::AudioMixer* Mixer = nullptr; // TS1(sims): replaces XNA SoundEffect playback
    bool Verbose = false;
    /// TS1(sims): called for every event played ("evt", what happened); the host tools log it.
    std::function<void(const std::string& evt, const std::string& info)> EventLog;
    /// TS1(sims): every kSetMusicMode event (the app's mode changes and the lot controllers' station_* events).
    std::function<void(const std::string& evt)> OnMusicEvent;

private:
    std::shared_ptr<HITTVOn> MusicEvent;
    std::shared_ptr<HITTVOn> NextMusic;

    std::map<std::string, std::shared_ptr<HITSound>> ActiveEvents; //events that are active are reused for all objects calling that event.
    std::vector<std::shared_ptr<HITSound>> Threads;
    std::vector<int32_t> Globals; //SimSpeed 0x64 to CampfireSize 0x87.

    std::vector<std::unique_ptr<FSCPlayer>> FSCPlayers;

    std::unordered_map<std::string, HITEventRegistration> Events;

    float GroupMasterVolumes[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    float DuckLevel = 1.0f;    // FreeSims Duck()/Unduck() state (HIT duck opcode)
    bool StingActive = false;  // TS1(sims): a kDuckPriNever sting is playing: music is ducked

public:
    HITVM();
    ~HITVM();

    void WriteGlobal(int num, int value);
    int ReadGlobal(int num);

private:
    void RegisterEvents(std::shared_ptr<HITResourceGroup> group);
    std::shared_ptr<HITResourceGroup> LoadHitGroup(const std::string& HITPath, const std::string& EVTPath, const std::string& HSMPath);

public:
    void Tick();

    void StopFSC(FSCPlayer* input);
    FSCPlayer* PlayFSC(const std::string& path);

    std::shared_ptr<HITSound> PlaySoundEvent(std::string evt);

    float GetMasterVolume(HITVolumeGroup group);
    /// TS1(sims): the options' volume sliders (also forwarded to the mixer's groups).
    void SetMasterVolume(HITVolumeGroup group, float volume);

    /// <summary>
    /// Ducks all sounds with a priority lower than the one passed.
    /// </summary>
    void Duck(FSO::Files::HIT::HITDuckingPriorities DuckPri);

    /// <summary>
    /// Unducks all threads, I.E sets their volume back to what it was before Duck() was called.
    /// </summary>
    void Unduck();

    // ---- TS1(sims) ----
    const HITEventRegistration* GetEvent(const std::string& evt) const;
    /// seqgroup_kill: kills the other threads that joined sequence group `group`.
    void KillSeqGroup(int group, HITThread* except);
    /// play_trk: starts a track of the bank as its own thread.
    std::shared_ptr<HITSound> PlayTrack(uint32_t trackID, HITResourceGroup* group);
    /// kill_trk: kills the threads playing a track.
    void KillTrack(uint32_t trackID, HITThread* except);
    /// kKillAll / leaving a lot: stops every sound (music too when includeMusic).
    void KillAll(bool includeMusic);
    /// Leaving a lot: stops the sounds owned by objects (the VM that owned them is gone).
    void KillOwned();
    /// Volume multiplier for automatic ducking (music under stings, the HIT duck opcode).
    float DuckFactor(const HITSound* s) const;
    /// note_on bookkeeping (logs).
    void NotePlayed(HITThread* thread, uint32_t patch);
    int ThreadCount() const { return (int)Threads.size(); }
    std::string MusicStation() const;
    int EventsPlayed = 0, NotesPlayed = 0;
};

}} // namespace FSO::HIT
