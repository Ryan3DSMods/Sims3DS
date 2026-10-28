/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims SimsVille/HIT/HITVM.cs
#include "hit/HITVM.h"
#include <algorithm>
#include <cstdio>
#include <random>
#include "audio/AudioMixer.h"
#include "content/Audio.h"
#include "files/HIT/HITText.h"
#include "hit/FSCPlayer.h"
#include "hit/HITTVOn.h"
#include "hit/HITThread.h"

namespace FSO { namespace HIT {

using namespace FSO::Files::HIT;

std::unique_ptr<HITVM> HITVM::INSTANCE;

void HITVM::Init(simaudio::AudioMixer* mixer)
{
    INSTANCE.reset(new HITVM());
    INSTANCE->Mixer = mixer;
}

void HITVM::Shutdown()
{
    if (INSTANCE) INSTANCE->KillAll(true);
    INSTANCE.reset();
}

int HITVM::NextRandom(int max)
{
    static std::mt19937 rng(0x5135u);
    if (max <= 1) return 0;
    return (int)(rng() % (uint32_t)max);
}

HITVM::HITVM()
{
    auto content = FSO::Content::Audio::Get();
    Events.clear();

    // C#: newmain / relationships / tsoep5 / tsov2 / tsov3 / turkey = LoadHitGroup(content.GetPath("sounddata/..."))
    // TS1(sims): every bank Content::Audio found, highest LoadPriority first (the first registration wins).
    if (content != nullptr)
    {
        for (auto& group : content->Groups) RegisterEvents(group);
    }

    Globals.assign(36, 0);
    Threads.clear();
    ActiveEvents.clear();
    FSCPlayers.clear();
}

HITVM::~HITVM()
{
}

void HITVM::WriteGlobal(int num, int value)
{
    if (num >= 0 && num < (int)Globals.size()) Globals[(size_t)num] = value;
}

int HITVM::ReadGlobal(int num)
{
    return (num >= 0 && num < (int)Globals.size()) ? Globals[(size_t)num] : 0;
}

void HITVM::RegisterEvents(std::shared_ptr<HITResourceGroup> group)
{
    // C#: var events = group.evt; TS1(sims): the .hot [EventMapping] section (or an .evt when present)
    std::vector<EVTEntry> entries;
    if (group->evt != nullptr) entries = group->evt->Entries;
    if (group->hot != nullptr) for (auto& e : group->hot->Events) entries.push_back(e.second);

    for (size_t i = 0; i < entries.size(); i++)
    {
        auto& entry = entries[i];
        if (Events.find(entry.Name) == Events.end())
        {
            HITEventRegistration reg;
            reg.Name = entry.Name;
            reg.EventType = (HITEvents)entry.EventType;
            reg.TrackID = entry.TrackID;
            reg.ResGroup = group;
            reg.Extra = entry.Unknown2;
            Events.emplace(entry.Name, reg);
        }
    }
}

std::shared_ptr<HITResourceGroup> HITVM::LoadHitGroup(const std::string& HITPath, const std::string& EVTPath, const std::string& HSMPath)
{
    auto events = std::make_shared<EVT>(EVTPath);
    auto hitfile = std::make_shared<HITFile>(HITPath);
    std::shared_ptr<HSM> hsmfile;
    if (!HSMPath.empty()) hsmfile = std::make_shared<HSM>(HSMPath);

    auto g = std::make_shared<HITResourceGroup>();
    g->evt = events;
    g->hit = hitfile;
    g->hsm = hsmfile;
    return g;
}

void HITVM::Tick()
{
    // TS1(sims): automatic ducking. While a sting (kDuckPriNever, e.g. sting_death, the promotion fanfare) or
    // another high-priority track plays, the music (radio, mode music) is pulled down underneath it.
    bool sting = false;
    for (auto& t : Threads)
    {
        if (t->Dead) continue;
        auto pri = t->DuckPriority();
        // kDuckPriHigh (30) and above: stings and their helpers
        if (t->ActiveTrack != nullptr && (uint32_t)pri >= (uint32_t)HITDuckingPriorities::duckpri_high) sting = true;
    }
    StingActive = sting;

    for (size_t i = 0; i < Threads.size(); i++)
    {
        auto keep = Threads[i]; // the thread may remove itself from ActiveEvents while ticking
        if (!keep->Tick()) { Threads.erase(Threads.begin() + i); i--; }
    }

    for (size_t i = 0; i < FSCPlayers.size(); i++)
    {
        FSCPlayers[i]->Tick(1 / 60.0f);
    }

    if (NextMusic != nullptr)
    {
        if (MusicEvent == nullptr || MusicEvent->Dead)
        {
            MusicEvent = NextMusic;
            Threads.push_back(NextMusic);
            NextMusic = nullptr;
        }
    }

    // TS1(sims): forget finished events (the C# kept them in ActiveEvents until the name was played again)
    for (auto it = ActiveEvents.begin(); it != ActiveEvents.end();)
    {
        if (it->second->Dead && it->second.use_count() <= 1) it = ActiveEvents.erase(it);
        else ++it;
    }
}

void HITVM::StopFSC(FSCPlayer* input)
{
    for (size_t i = 0; i < FSCPlayers.size(); i++)
        if (FSCPlayers[i].get() == input) { FSCPlayers.erase(FSCPlayers.begin() + i); return; }
}

FSCPlayer* HITVM::PlayFSC(const std::string& path)
{
    auto slash = path.find_last_of("/\\");
    auto dir = (slash == std::string::npos) ? std::string() : path.substr(0, slash + 1);
    auto fsc = std::make_shared<FSC>(path);
    FSCPlayers.emplace_back(new FSCPlayer(fsc, dir));

    return FSCPlayers.back().get();
}

const HITEventRegistration* HITVM::GetEvent(const std::string& evt) const
{
    auto it = Events.find(Text::ToLower(evt));
    return (it == Events.end()) ? nullptr : &it->second;
}

std::shared_ptr<HITSound> HITVM::PlaySoundEvent(std::string evt)
{
    evt = Text::ToLower(evt);
    std::shared_ptr<HITThread> InterruptBlocker;  //the thread we have to wait for to finish before we begin.

    auto ae = ActiveEvents.find(evt);
    if (ae != ActiveEvents.end())
    {
        auto aevt = ae->second;

        if (aevt->Dead) ActiveEvents.erase(ae); //if the last event is dead, remove and make a new one
        else
        {
            auto athread = std::dynamic_pointer_cast<HITThread>(aevt);
            if (athread != nullptr && athread->InterruptBlocker.lock() != nullptr)
            {
                //we can stop this thread - steal its waiter
                athread->Dead = true;
                InterruptBlocker = athread->InterruptBlocker.lock();
            }
            else if (athread != nullptr && athread->Interruptable() == true)
            {
                InterruptBlocker = athread;
            }
            else return aevt; //an event of this type is already alive - here, take it.
        }
    }

    auto content = FSO::Content::Audio::Get();
    auto found = Events.find(evt);
    if (found != Events.end() && content != nullptr)
    {
        auto& evtent = found->second;
        EventsPlayed++;

        if (evt == "piano_play")
        {
            evt = "playpiano";
            auto pe = ActiveEvents.find(evt);
            if (pe != ActiveEvents.end())
            {
                if (pe->second->Dead) ActiveEvents.erase(pe); //if the last event is dead, remove and make a new one
                else return pe->second; //an event of this type is already alive - here, take it.
            }
        }

        uint32_t TrackID = 0;
        uint32_t SubroutinePointer = 0;
        auto group = evtent.ResGroup.get();
        if (group->hsm != nullptr)
        {
            auto& c = group->hsm->Constants;
            auto ci = c.find(evt);
            if (ci != c.end()) SubroutinePointer = (uint32_t)ci->second;
            auto trackIdName = "guid_tkd_" + evt;
            auto ti = c.find(trackIdName);
            if (ti != c.end()) TrackID = (uint32_t)ti->second;
            else TrackID = evtent.TrackID;
        }
        else if (group->hit != nullptr)
        { //no hsm, fallback to eent and event track ids (tsov2)
            auto& entPoints = group->hit->EntryPointByTrackID;
            TrackID = evtent.TrackID;
            auto ei = entPoints.find(evtent.TrackID);
            if (ei != entPoints.end()) SubroutinePointer = ei->second;
        }
        else TrackID = evtent.TrackID;

        // TS1(sims): The Sims' HSM names labels after tracks, not events: the event's track gives the code
        // ([Track] -> @tkd_label -> [TrackData]). The event-name lookup above still wins when it matches.
        if (group->hot != nullptr && evtent.EventType != HITEvents::kTurnOnTV && evtent.EventType != HITEvents::kSetMusicMode)
        {
            auto trk = content->GetTrack(evtent.TrackID, group);
            if (trk != nullptr && trk->SubroutineID != 0) { SubroutinePointer = trk->SubroutineID; TrackID = evtent.TrackID; }
        }

        if (EventLog) EventLog(evt, std::to_string((int)evtent.EventType) + " track " + std::to_string(evtent.TrackID) + " (" + group->Name + ")");

        if (evtent.EventType == HITEvents::kTurnOnTV)
        {
            auto thread = std::make_shared<HITTVOn>(evtent.TrackID, this);
            thread->Name = evt;
            // C#: VolGroup = FX. TS1(sims): radio stations follow the music slider, TV the sound one
            auto st = content->StationPaths().find(evtent.TrackID);
            thread->VolGroup = (st != content->StationPaths().end() && !st->second.TV) ? HITVolumeGroup::MUSIC : HITVolumeGroup::FX;
            Threads.push_back(thread);
            ActiveEvents[evt] = thread;
            return thread;
        }
        else if (evtent.EventType == HITEvents::kSetMusicMode)
        {
            if (OnMusicEvent) OnMusicEvent(evt);
            auto thread = std::make_shared<HITTVOn>(evtent.TrackID, this, true);
            thread->Name = evt;
            thread->VolGroup = HITVolumeGroup::MUSIC;
            ActiveEvents[evt] = thread;
            if (NextMusic != nullptr) NextMusic->Kill();
            if (MusicEvent != nullptr) MusicEvent->Fade();
            NextMusic = thread;
            return thread;
        }
        else if (evtent.EventType == HITEvents::kKillAll)
        {
            KillAll(false); // TS1(sims): "kill_all"
            return nullptr;
        }
        else if (evtent.EventType != HITEvents::kSoundobPlay && evtent.EventType != HITEvents::kSequenceTrackHitList
            && evtent.EventType != HITEvents::kPlayPiano)
        {
            // TS1(sims): kKillInstance (tv_turn_off, stereo_turnoff), kSndobCancelNote (radio_next_song),
            // kSndobStop/Kill act on the calling object's sounds: VMPlaySound handles them with GetEvent().
            return nullptr;
        }
        else if (SubroutinePointer != 0 && group->hit != nullptr)
        {
            auto thread = std::make_shared<HITThread>(group->hit, this);
            thread->Name = evt;
            thread->ResGroup = group; // TS1(sims)
            thread->PC = SubroutinePointer;
            thread->EntryPC = SubroutinePointer;
            if (TrackID != 0) thread->SetTrack(TrackID);
            // TS1(sims): kSequenceTrackHitList passes a hitlist to a shared sequencing track (Superstar's
            // music-video sets): it is preloaded as the thread's hitlist and v1.
            if (evtent.EventType == HITEvents::kSequenceTrackHitList && evtent.Extra != 0)
            {
                thread->WriteVar(5, (int)evtent.Extra);
                thread->LoadHitlist(evtent.Extra);
            }

            Threads.push_back(thread);
            if (ActiveEvents.find(evt) == ActiveEvents.end())
                ActiveEvents.emplace(evt, thread);

            if (InterruptBlocker != nullptr)
            {
                InterruptBlocker->Interrupt(thread.get());
                InterruptBlocker->KillVocals();
            }

            return thread;
        }
        else if (TrackID != 0 && content->GetTrack(TrackID, group) != nullptr)
        {
            auto thread = std::make_shared<HITThread>(TrackID);
            thread->Name = evt;
            thread->VM = this;
            Threads.push_back(thread);
            ActiveEvents[evt] = thread;

            if (InterruptBlocker != nullptr)
            {
                InterruptBlocker->Interrupt(thread.get());
                InterruptBlocker->KillVocals();
            }

            return thread;
        }
    }
    else if (EventLog) EventLog(evt, "unknown event");

    return nullptr;
}

float HITVM::GetMasterVolume(HITVolumeGroup group)
{
    return GroupMasterVolumes[(int)group];
}

void HITVM::SetMasterVolume(HITVolumeGroup group, float volume)
{
    GroupMasterVolumes[(int)group] = volume;
    if (Mixer != nullptr) Mixer->SetGroupVolume((int)group, volume);
}

void HITVM::Duck(HITDuckingPriorities DuckPri)
{
    for (size_t i = 0; i < Threads.size(); i++)
    {
        //0 means least importance, so it gets ducked.
        if (Threads[i]->DuckPriority() < DuckPri)
        {
            switch (DuckPri)
            {
            case HITDuckingPriorities::duckpri_low:
                Threads[i]->SetVolume(0.15f, Threads[i]->Pan);
                break;
            case HITDuckingPriorities::duckpri_normal:
                Threads[i]->SetVolume(0.25f, Threads[i]->Pan);
                break;
            case HITDuckingPriorities::duckpri_high:
                Threads[i]->SetVolume(0.45f, Threads[i]->Pan);
                break;
            case HITDuckingPriorities::duckpri_higher:
                Threads[i]->SetVolume(0.65f, Threads[i]->Pan);
                break;
            case HITDuckingPriorities::duckpri_evenhigher:
                //Threads[i].SetVolume(0.85f, Threads[i].Pan);
                //If ducking priority is duckpri_never, it shouldn't be ducked!
                break;
            default:
                break;
            }
        }
    }
}

void HITVM::Unduck()
{
    for (size_t i = 0; i < Threads.size(); i++)
        Threads[i]->SetVolume(Threads[i]->PreviousVolume, Threads[i]->Pan);
}

void HITVM::KillSeqGroup(int group, HITThread* except)
{
    for (auto& t : Threads)
    {
        auto th = dynamic_cast<HITThread*>(t.get());
        if (th == nullptr || th == except || th->SeqGroup != group || th->Dead) continue;
        th->Kill();
    }
}

std::shared_ptr<HITSound> HITVM::PlayTrack(uint32_t trackID, HITResourceGroup* group)
{
    auto content = FSO::Content::Audio::Get();
    if (content == nullptr) return nullptr;
    HITResourceGroup* owner = nullptr;
    auto trk = content->GetTrack(trackID, group, &owner);
    if (trk == nullptr || owner == nullptr) return nullptr;
    std::shared_ptr<HITThread> thread;
    if (trk->SubroutineID != 0 && owner->hit != nullptr)
    {
        thread = std::make_shared<HITThread>(owner->hit, this);
        thread->ResGroup = owner;
        thread->PC = thread->EntryPC = trk->SubroutineID;
        thread->SetTrack(trackID);
    }
    else
    {
        thread = std::make_shared<HITThread>(trackID);
        thread->VM = this;
    }
    thread->Name = "track:" + std::to_string(trackID);
    Threads.push_back(thread);
    return thread;
}

void HITVM::KillTrack(uint32_t trackID, HITThread* except)
{
    for (auto& t : Threads)
    {
        auto th = dynamic_cast<HITThread*>(t.get());
        if (th == nullptr || th == except || th->ActiveTrack == nullptr || th->ActiveTrack->TrackID != trackID) continue;
        th->Kill();
    }
}

void HITVM::KillAll(bool includeMusic)
{
    for (auto& t : Threads)
    {
        if (!includeMusic && (t == MusicEvent || t == NextMusic)) continue;
        if (auto tv = std::dynamic_pointer_cast<HITTVOn>(t)) tv->Kill();
        else if (auto th = std::dynamic_pointer_cast<HITThread>(t)) th->Kill();
        t->Dead = true;
    }
    if (includeMusic)
    {
        if (NextMusic) NextMusic->Kill();
        NextMusic = nullptr;
        MusicEvent = nullptr;
    }
    FSCPlayers.clear();
}

void HITVM::KillOwned()
{
    for (auto& t : Threads)
    {
        if (!t->EverHadOwners) continue;
        t->Owners.clear(); // the next Tick kills them (EverHadOwners && no owners)
        if (auto tv = std::dynamic_pointer_cast<HITTVOn>(t)) tv->Kill();
        else if (auto th = std::dynamic_pointer_cast<HITThread>(t)) th->Kill();
    }
}

float HITVM::DuckFactor(const HITSound* s) const
{
    float f = 1.0f;
    if (StingActive && s->VolGroup == HITVolumeGroup::MUSIC && (uint32_t)s->DuckPriority() < (uint32_t)HITDuckingPriorities::duckpri_high)
        f *= 0.35f;
    return f;
}

void HITVM::NotePlayed(HITThread* thread, uint32_t patch)
{
    NotesPlayed++;
    if (EventLog)
    {
        auto content = FSO::Content::Audio::Get();
        auto p = content ? content->GetPatch(patch, thread->ResGroup) : nullptr;
        EventLog(thread->Name, "note " + std::to_string(patch) + (p ? " " + p->Filename : std::string()));
    }
}

std::string HITVM::MusicStation() const
{
    auto m = NextMusic ? NextMusic : MusicEvent;
    return m ? m->StationName() : std::string();
}

}} // namespace FSO::HIT
