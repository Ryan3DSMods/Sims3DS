/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims SimsVille/ContentManager/Audio.cs (FSO.Content.Audio), re-targeted at The Sims 1.
//
// TS1(sims): FreeSims' Audio manager reads The Sims Online's DBPF archives (TSOAudio.dat, tsov2.dat...) and
// TRK/HLS resources. The Sims 1 keeps its sound in a different layout, which this class indexes instead:
//   * HIT banks: <bank>.hot (events, tracks, patches, hitlists) + <bank>.hsm (labels) + <bank>.hit (bytecode) in
//     SoundData/, ExpansionPack/, ExpansionPack5-7/Sound/, ExpansionShared/Sound/ and Downloads/*/;
//   * samples (Maxis XA ADPCM, WAV) inside the FAR archives next to each bank (SoundData/SoundData.far,
//     ExpansionPack*/Sound/Sound.far, ExpansionShared/Sound/Sound.far, ExpansionPack/ExpansionPack.far,
//     Downloads/*/*.far), found by file name;
//   * TV stations: TVStations/<station>/<episode>/*.xa + commercials TVStations/*.xa inside those FARs;
//   * music: MP3 folders Music/Modes/<mode>/ and Music/Stations/<station>/ (+ Modes/Load/loadloop.wav).
// Nothing is decoded up front: samples are decoded on first use into a byte-budgeted cache.
#pragma once
#include <cstdint>
#include <cstdio>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "audio/SoundBuffer.h"
#include "files/HIT/Hitlist.h"
#include "files/HIT/Patch.h"
#include "files/HIT/Track.h"
#include "hit/HITResourceGroup.h"

namespace FSO { namespace Content {

/// <summary>
/// Type of audio.
/// </summary>
enum class AudioType
{
    RADIO_STATION,
    MODE
};

/// <summary>
/// A reference to an audio file.
/// </summary>
class AudioReference
{
public:
    uint32_t ID = 0;
    AudioType Type = AudioType::RADIO_STATION;
    std::string FilePath;
    std::string Name() const { auto p = FilePath.find_last_of("/\\"); return p == std::string::npos ? FilePath : FilePath.substr(p + 1); }
};

/// <summary>
/// Manager for the audio content.
/// </summary>
class Audio
{
public:
    static Audio* Get();
    /// Indexes the install at basePath (the folder holding SoundData/, Music/, ExpansionPack*/ ...). Cheap: reads
    /// the .hot/.hsm/.hit banks and the FAR manifests only. sfxCacheBytes bounds the decoded-sample cache.
    static Audio* Init(const std::string& basePath, size_t sfxCacheBytes = 8u * 1024 * 1024);
    static void Shutdown();

    std::string BasePath;

    /// TS1 HIT banks, highest LoadPriority first (expansion banks override the base game's events).
    std::vector<std::shared_ptr<FSO::HIT::HITResourceGroup>> Groups;

    /// C# TracksById: every bank's [Track] entries (first = highest priority wins). Track ids are unique per pack.
    std::unordered_map<uint32_t, std::pair<FSO::Files::HIT::Track*, FSO::HIT::HITResourceGroup*>> TracksById;

    /// Looks a track up, preferring the given bank.
    FSO::Files::HIT::Track* GetTrack(uint32_t id, FSO::HIT::HITResourceGroup* prefer = nullptr, FSO::HIT::HITResourceGroup** owner = nullptr);
    /// <summary> Gets a Hitlist (C# GetHitlist(InstanceID)): the bank's [GlobalHitList] first, then any bank. </summary>
    std::shared_ptr<FSO::Files::HIT::Hitlist> GetHitlist(uint32_t InstanceID, FSO::HIT::HITResourceGroup* prefer = nullptr);
    /// The [Patch] entry for a sound id (bank first).
    const FSO::Files::HIT::Patch* GetPatch(uint32_t id, FSO::HIT::HITResourceGroup* prefer = nullptr, FSO::HIT::HITResourceGroup** owner = nullptr);

    /// <summary>
    /// Gets a sound effect from the sound effects cache (C# GetSFX(InstanceID)): the patch's XA/WAV/UTK decoded
    /// to PCM. Returns null for unknown patches, and for long WAVs (stings) which play as streams instead.
    /// </summary>
    simaudio::SoundBufferPtr GetSFX(uint32_t InstanceID, FSO::HIT::HITResourceGroup* prefer = nullptr);
    /// Opens a long sample (a WAV sting inside a FAR) as a stream; null if the patch is not such a sample.
    std::unique_ptr<simaudio::AudioStream> GetSFXStream(uint32_t InstanceID, FSO::HIT::HITResourceGroup* prefer = nullptr);
    /// Decodes a sample by archive file name ("vox_juggler_tadam1.xa"); tools and TV clips.
    simaudio::SoundBufferPtr GetSampleByName(const std::string& fileName, FSO::HIT::HITResourceGroup* prefer = nullptr);
    /// Raw bytes of a sample by name (null result = not found).
    bool ReadSample(const std::string& fileName, std::vector<uint8_t>& data, FSO::HIT::HITResourceGroup* prefer = nullptr);

    // ---- stations / music ----
    struct StationInfo
    {
        std::string Path;   // "Music/Stations/Country/" (music) or the TV station key "tv_action"
        bool TV = false;
        std::string Label;  // human-readable
    };
    /// TS1(sims): kTurnOnTV track ids -> stations (FreeSims used TSO's 4-character station codes "KCOU").
    const std::map<uint32_t, StationInfo>& StationPaths() const { return _StationPaths; }
    /// kSetMusicMode ids -> music folders (FreeSims: _MusicModes, TSO ids 11/12/13 -> "KSEL"/"KCRE"/"KMAP").
    const std::map<int, std::string>& MusicModes() const { return _MusicModes; }
    /// Files (*.mp3, *.wav) in a music folder relative to BasePath, sorted. Empty when missing.
    std::vector<std::string> ListMusic(const std::string& relDir);

    /// One TV station: episodes (each a list of clip names) and the commercials of its pack.
    struct TVStation
    {
        std::vector<std::vector<std::string>> Episodes;
        std::vector<std::string> Commercials;
    };
    const TVStation* GetTVStation(const std::string& key) const;

    /// <summary>
    /// Compiles the radio stations in the game to a list of AudioReference instances.
    /// </summary>
    std::vector<AudioReference> List();

    // ---- stats ----
    size_t CacheBytes() const { return _CacheBytes; }
    size_t IndexedSamples() const { return _SampleIndex.size(); }
    int Misses = 0;

private:
    Audio() {}
    static std::unique_ptr<Audio> INSTANCE;

    struct FarArchive { std::string Path; std::string Dir; };
    struct SampleLoc { int Far; uint32_t Offset, Size; };
    std::vector<FarArchive> _Fars;
    std::unordered_map<uint32_t, std::vector<SampleLoc>> _SampleIndex; // FNV-1a of the lower-case file name
    std::map<std::string, TVStation> _TV;
    std::map<uint32_t, StationInfo> _StationPaths;
    std::map<int, std::string> _MusicModes;

    // decoded sample cache (LRU, byte budget). Keyed by archive location.
    struct CacheEntry { simaudio::SoundBufferPtr Buf; std::list<uint64_t>::iterator It; };
    std::unordered_map<uint64_t, CacheEntry> _Cache;
    std::list<uint64_t> _Lru;
    size_t _CacheBytes = 0, _CacheBudget = 8u * 1024 * 1024;

    // TS1(sims): archive reads go through the shared FilePool (a few handles stay open, LRU)
    bool ReadArchive(int far, uint32_t offset, uint32_t size, std::vector<uint8_t>& data);

    void IndexFar(const std::string& path, std::vector<int>* localList);
    void LoadBank(const std::string& dir, const std::string& baseName);
    void ScanBankDir(const std::string& dir, bool recurse);
    const SampleLoc* FindSample(const std::string& fileName, FSO::HIT::HITResourceGroup* prefer);
    simaudio::SoundBufferPtr Decode(const SampleLoc& loc, const std::string& name);
    void BuildTables();
};

/// FNV-1a of a lower-cased file name (no directory).
uint32_t AudioNameHash(const std::string& fileName);

}} // namespace FSO::Content
