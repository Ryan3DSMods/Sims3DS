/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims SimsVille/ContentManager/Audio.cs; the archive layout is The Sims 1's (see Audio.h).
#include "files/FilePool.h"
#include "content/Audio.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include "audio/WavFile.h"
#include "files/HIT/HITText.h"
#include "files/UTK/UTKFile2.h"
#include "files/XA/XAFile.h"

#if !defined(__3DS__) || defined(ST_HAVE_DIRENT)
#include <dirent.h>
#include <sys/stat.h>
#define ST_AUDIO_DIRENT 1
#endif

namespace FSO { namespace Content {

using namespace FSO::Files::HIT;
using FSO::HIT::HITResourceGroup;
namespace Text = FSO::Files::HIT::Text;

std::unique_ptr<Audio> Audio::INSTANCE;

Audio* Audio::Get() { return INSTANCE.get(); }

void Audio::Shutdown() {
    if (INSTANCE) for (auto& fa : INSTANCE->_Fars) FSO::Files::FilePool::Close(fa.Path);
    INSTANCE.reset();
}

uint32_t AudioNameHash(const std::string& fileName) {
    size_t p = fileName.find_last_of("/\\");
    uint32_t h = 2166136261u;
    for (size_t i = (p == std::string::npos ? 0 : p + 1); i < fileName.size(); i++) {
        h ^= (uint8_t)std::tolower((unsigned char)fileName[i]);
        h *= 16777619u;
    }
    return h;
}

namespace {

struct DirEntry { std::string Name; bool Dir; };

std::vector<DirEntry> ListDir(const std::string& path) {
    std::vector<DirEntry> out;
#ifdef ST_AUDIO_DIRENT
    DIR* d = opendir(path.c_str());
    if (!d) return out;
    while (dirent* e = readdir(d)) {
        std::string n = e->d_name;
        if (n == "." || n == "..") continue;
        struct stat st;
        bool isDir = false;
        if (stat((path + "/" + n).c_str(), &st) == 0) isDir = S_ISDIR(st.st_mode);
        out.push_back({ n, isDir });
    }
    closedir(d);
    std::sort(out.begin(), out.end(), [](const DirEntry& a, const DirEntry& b) { return Text::ToLower(a.Name) < Text::ToLower(b.Name); });
#else
    (void)path;
#endif
    return out;
}

bool FileExists(const std::string& p) {
    FILE* f = std::fopen(p.c_str(), "rb");
    if (!f) return false;
    std::fclose(f);
    return true;
}

bool EndsWithI(const std::string& s, const std::string& suf) {
    if (s.size() < suf.size()) return false;
    return Text::ToLower(s.substr(s.size() - suf.size())) == suf;
}

/// finds "name" in dir ignoring case (host file systems are case sensitive; the game's names are not)
std::string FindI(const std::string& dir, const std::string& name) {
    std::string direct = dir + "/" + name;
#ifdef ST_AUDIO_DIRENT
    auto want = Text::ToLower(name);
    for (auto& e : ListDir(dir)) if (Text::ToLower(e.Name) == want) return dir + "/" + e.Name;
    return "";
#else
    return FileExists(direct) ? direct : "";
#endif
}

uint32_t RdU32(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

// TS1(sims): without a directory API (bare newlib), the banks and music of the Complete Collection.
const char* const kKnownBanks[] = {
    "SoundData/SimsGeneratedHitSource", "SoundData/SimsSound",
    "ExpansionPack/xpack", "ExpansionPack/repair_vox2",
    "ExpansionShared/Sound/HotDate", "ExpansionShared/Sound/HouseParty", "ExpansionShared/Sound/vacsnd", "ExpansionShared/Sound/gnome",
    "ExpansionPack5/Sound/sep5snds", "ExpansionPack6/Sound/sep6snds", "ExpansionPack7/Sound/sep7snds",
    "Downloads/BirthdayCake/bcakesnd", "Downloads/CClock/Cuckoo", "Downloads/Christmas2000/santa_snd",
    "Downloads/FunHouseTrackLoop/funhouseloop_snd", "Downloads/GuineaPg/GuineaPig", "Downloads/HPPottyPack/toilets",
    "Downloads/Jukebox/jukebox", "Downloads/LampWallBar/LampWallBarSounds", "Downloads/Moose/Moose",
    "Downloads/PartyBalloons/balloons", "Downloads/PlantCarnivore/plantcarnivore", "Downloads/SCDrinks/SC4Drinks",
    "Downloads/SlotMach/SlotMachine", "Downloads/TableSoccer/tablesoccer_snd", "Downloads/Turkey/turkey",
    "Downloads/dart_board/dartboardsnd",
};
const char* const kKnownFars[] = {
    "SoundData/SoundData.far", "ExpansionPack/ExpansionPack.far", "ExpansionShared/Sound/Sound.far",
    "ExpansionPack3/Sound/Sound.far", "ExpansionPack4/Sound/Sound.far", "ExpansionPack5/Sound/Sound.far",
    "ExpansionPack6/Sound/Sound.far", "ExpansionPack7/Sound/Sound.far",
    "Downloads/BirthdayCake/BirthdayCake.far", "Downloads/CClock/CClock.far", "Downloads/Christmas2000/Christmas2000.far",
    "Downloads/FunHouseTrackLoop/FunHouseTrackLoop.far", "Downloads/GuineaPg/GuineaPg.far", "Downloads/HPPottyPack/HPPottyPack.far",
    "Downloads/Jukebox/Jukebox.far", "Downloads/LampWallBar/LampWallBar.far", "Downloads/Moose/Moose.far",
    "Downloads/PartyBalloons/PartyBalloons.far", "Downloads/PlantCarnivore/PlantCarnivore.far", "Downloads/SCDrinks/SCDrinks.far",
    "Downloads/SlotMach/SlotMach.far", "Downloads/TableSoccer/TableSoccer.far", "Downloads/Turkey/Turkey.far",
    "Downloads/dart_board/dart_board.far",
};
struct KnownMusic { const char* Dir; const char* Files; };
const KnownMusic kKnownMusic[] = {
    {"Music/Modes/Build", "build1.mp3,build2.mp3,build3.mp3,build4.mp3,build5.mp3,build6.mp3"},
    {"Music/Modes/Buy", "buy1.mp3,buy2.mp3,buy3.mp3,buy4.mp3"},
    {"Music/Modes/Load", "loadloop.wav"},
    {"Music/Modes/MM_Build", "build3.mp3,build4.mp3,build5.mp3,build6.mp3,build7.mp3,build8.mp3,build9.mp3"},
    {"Music/Modes/MM_Buy", "buy1.mp3,buy2.mp3,buy3.mp3,buy4.mp3,buy5.mp3"},
    {"Music/Modes/MM_credits", "sep7_credits1.mp3"},
    {"Music/Modes/MakinMagic", "sep7_bb1.mp3,sep7_bb3.mp3,sep7_bb4.mp3,sep7_bb5.mp3,sep7_bb6.mp3,sep7_bb7.mp3,sep7_bb8.mp3"},
    {"Music/Modes/Nhood", "Latin7.mp3,latin2.mp3,latin4.mp3,latin6.mp3"},
    {"Music/Modes/NhoodUS", "latin6.mp3,latin7.mp3,nhood1.mp3,nhood2.mp3,nhood3.mp3"},
    {"Music/Modes/Superstar", "sep6_bb1.mp3,sep6_bb2.mp3,sep6_bb3.mp3,sep6_bb4.mp3,sep6_bb5.mp3,sep6_bb6.mp3"},
    {"Music/Modes/Vacation", "SimsVac1.mp3,SimsVac2.mp3,SimsVac3.mp3,SimsVac4.mp3,SimsVac5.mp3,SimsVac6.mp3,SimsVac7.mp3,SimsVac8.mp3"},
    {"Music/Modes/dt_nhood", "shd_dt_jazz1.mp3,shd_dt_jazz2.mp3,shd_dt_jazz3.mp3,shd_dt_jazz4.mp3,shd_dt_jazz5.mp3,shd_dt_jazz6.mp3,shd_dt_jazz7.mp3,shd_dt_jazz8.mp3"},
    {"Music/Stations/Beach", "beach1.mp3,beach2.mp3,beach3.mp3,beach4.mp3,beach5.mp3"},
    {"Music/Stations/Classica", "BbMaj.mp3,Cmaj.mp3,EbMaj.mp3,Fmaj.mp3,Gmaj.mp3"},
    {"Music/Stations/Country", "beaumont.mp3,devilsdr.mp3,downroad.mp3,downyon.mp3,sallygoo.mp3,splatter.mp3,turkey.mp3"},
    {"Music/Stations/CountryD", "cntryd1.mp3,cntryd2.mp3,cntryd3.mp3,cntryd4.mp3,cntryd5.mp3"},
    {"Music/Stations/Disco", "disco1.mp3,disco2.mp3,disco3.mp3,disco4.mp3,disco5.mp3"},
    {"Music/Stations/EZ", "SHD_dt_cheap1.mp3,SHD_dt_cheap2.mp3,SHD_dt_cheap3.mp3,SHD_dt_cheap4.mp3,SHD_dt_cheap5.mp3,SHD_dt_cheap6.mp3,SHD_dt_cheap7.mp3,SHD_dt_cheap8.mp3,SHD_dt_cheap9.mp3"},
    {"Music/Stations/EZExp", "shd_dt_exp1.mp3,shd_dt_exp3.mp3,shd_dt_exp4.mp3,shd_dt_exp5.mp3,shd_dt_exp6.mp3"},
    {"Music/Stations/Latin", "latin1.mp3,latin2.mp3,latin3.mp3,latin4.mp3,latin5.mp3"},
    {"Music/Stations/Rap", "shp_rap1.mp3,shp_rap2.mp3,shp_rap3.mp3,shp_rap4.mp3,shp_rap5.mp3"},
    {"Music/Stations/Rave", "rave1.mp3,rave2.mp3,rave3.mp3,rave4.mp3,rave5.mp3"},
    {"Music/Stations/Rock", "rock1.mp3,rock2.mp3,rock3.mp3,rock4.mp3,rock5.mp3"},
    {"Music/Stations/Superstar", "sep6_trans_wall1.mp3,sep6_trans_wall2.mp3,sep6_trans_wall3.mp3,sep6_trans_wall4.mp3,sep6_trans_wall5.mp3"},
    {"Music/Stations/UnlCheap", "SUL_Band1_M.mp3,SUL_Band2_M.mp3,SUL_Band3_M.mp3,SUL_Band4_M.mp3,SUL_Band5_M.mp3,SUL_Band6_M.mp3,SUL_Piano1_M.mp3,SUL_Piano2_M.mp3"},
    {"Music/Stations/UnlSpeak", "SUL_Zyd1_M.mp3,SUL_Zyd2_M.mp3,SUL_Zyd3_M.mp3,SUL_Zyd4_M.mp3,SUL_Zyd5_M.mp3,SUL_Zyd6_M.mp3"},
};

bool IsSoundName(const std::string& n) {
    return EndsWithI(n, ".xa") || EndsWithI(n, ".wav") || EndsWithI(n, ".utk") || EndsWithI(n, ".mp3");
}

} // namespace

Audio* Audio::Init(const std::string& basePath, size_t sfxCacheBytes) {
    INSTANCE.reset(new Audio());
    auto a = INSTANCE.get();
    a->BasePath = basePath;
    a->_CacheBudget = sfxCacheBytes;
    a->BuildTables();

#ifdef ST_AUDIO_DIRENT
    a->ScanBankDir(basePath + "/SoundData", false);
    a->ScanBankDir(basePath + "/ExpansionPack", false);
    for (int ep = 2; ep <= 7; ep++) a->ScanBankDir(basePath + "/ExpansionPack" + std::to_string(ep) + "/Sound", false);
    a->ScanBankDir(basePath + "/ExpansionShared/Sound", false);
    a->ScanBankDir(basePath + "/Downloads", true);
#else
    for (auto f : kKnownFars) if (FileExists(basePath + "/" + f)) a->IndexFar(basePath + "/" + f, nullptr);
    for (auto b : kKnownBanks) {
        std::string s = b;
        auto p = s.find_last_of('/');
        a->LoadBank(basePath + "/" + s.substr(0, p), s.substr(p + 1));
    }
    // archives next to each bank
    for (auto& g : a->Groups)
        for (int i = 0; i < (int)a->_Fars.size(); i++) if (a->_Fars[i].Dir == g->Dir) g->LocalFars.push_back(i);
#endif

    // highest priority first; stable keeps the scan order (base before packs) among equals
    std::stable_sort(a->Groups.begin(), a->Groups.end(), [](const std::shared_ptr<HITResourceGroup>& x, const std::shared_ptr<HITResourceGroup>& y) {
        return x->LoadPriority > y->LoadPriority;
    });
    for (auto& g : a->Groups) {
        for (auto& t : g->hot->Tracks) a->TracksById.emplace(t.first, std::make_pair(&t.second, g.get()));
    }
    return a;
}

void Audio::ScanBankDir(const std::string& dir, bool recurse) {
    auto entries = ListDir(dir);
    if (entries.empty()) return;
    std::vector<int> local;
    for (auto& e : entries) {
        if (e.Dir) { if (recurse) ScanBankDir(dir + "/" + e.Name, false); continue; }
        if (EndsWithI(e.Name, ".far")) IndexFar(dir + "/" + e.Name, &local);
    }
    size_t before = Groups.size();
    for (auto& e : entries) {
        if (!e.Dir && EndsWithI(e.Name, ".hot")) LoadBank(dir, e.Name.substr(0, e.Name.size() - 4));
    }
    for (size_t i = before; i < Groups.size(); i++) Groups[i]->LocalFars = local;
}

void Audio::LoadBank(const std::string& dir, const std::string& baseName) {
    std::string hotPath = FindI(dir, baseName + ".hot");
    if (hotPath.empty()) return;
    std::string hsmPath = FindI(dir, baseName + ".hsm");
    std::string hitPath = FindI(dir, baseName + ".hit");
    try {
        auto g = std::make_shared<HITResourceGroup>();
        if (!hsmPath.empty()) g->hsm = std::make_shared<HSM>(hsmPath);
        g->hot = std::make_shared<Hot>(hotPath, g->hsm);
        if (!hitPath.empty()) g->hit = std::make_shared<HITFile>(hitPath);
        g->Dir = dir;
        g->LoadPriority = g->hot->LoadPriority();
        std::string rel = dir.size() > BasePath.size() ? dir.substr(BasePath.size() + 1) : dir;
        g->Name = rel + "/" + baseName;
        Groups.push_back(g);
    } catch (std::exception& e) {
        std::printf("audio: bank %s/%s failed: %s\n", dir.c_str(), baseName.c_str(), e.what());
    }
}

void Audio::IndexFar(const std::string& path, std::vector<int>* localList) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return;
    uint8_t hdr[16];
    if (std::fread(hdr, 1, 16, f) != 16 || std::memcmp(hdr, "FAR!byAZ", 8) != 0 || RdU32(hdr + 8) != 1) { std::fclose(f); return; }
    uint32_t manifest = RdU32(hdr + 12);
    std::fseek(f, 0, SEEK_END);
    long fileSize = std::ftell(f);
    std::fseek(f, (long)manifest, SEEK_SET);
    std::vector<uint8_t> man((size_t)(fileSize - (long)manifest));
    man.resize(std::fread(man.data(), 1, man.size(), f));
    std::fclose(f);
    if (man.size() < 4) return;

    int farIdx = (int)_Fars.size();
    auto slash = path.find_last_of('/');
    _Fars.push_back({ path, slash == std::string::npos ? "." : path.substr(0, slash) });
    if (localList) localList->push_back(farIdx);

    std::map<std::string, std::map<std::string, std::vector<std::string>>> episodes; // station -> episode -> clips
    std::vector<std::string> commercials;
    uint32_t n = RdU32(man.data());
    size_t p = 4;
    for (uint32_t i = 0; i < n && p + 16 <= man.size(); i++) {
        uint32_t size = RdU32(&man[p]);
        uint32_t off = RdU32(&man[p + 8]);
        uint32_t nameLen = RdU32(&man[p + 12]); // FAR 1a (The Sims): 4-byte name length
        p += 16;
        if (p + nameLen > man.size()) break;
        std::string name((const char*)&man[p], nameLen);
        p += nameLen;
        if (!IsSoundName(name)) continue;
        _SampleIndex[AudioNameHash(name)].push_back({ farIdx, off, size });
        // TV stations: TVStations\<station>\<episode>\clip.xa, commercials TVStations\clip.xa
        std::string lower = Text::ToLower(name);
        std::replace(lower.begin(), lower.end(), '\\', '/');
        if (Text::StartsWith(lower, "tvstations/")) {
            auto parts = Text::Split(lower, '/');
            if (parts.size() == 2) commercials.push_back(parts[1]);
            else if (parts.size() >= 3) episodes[parts[1]][parts.size() >= 4 ? parts[2] : std::string("_")].push_back(parts.back());
        }
    }
    for (auto& st : episodes) {
        auto& tv = _TV[st.first];
        for (auto& ep : st.second) tv.Episodes.push_back(ep.second); // folders a, b, c... in order
        tv.Commercials = commercials;                                 // the commercials of the same pack
    }
}

const Audio::TVStation* Audio::GetTVStation(const std::string& key) const {
    auto it = _TV.find(Text::ToLower(key));
    return it == _TV.end() ? nullptr : &it->second;
}

void Audio::BuildTables() {
    // TS1(sims): kTurnOnTV track ids (SimsSound.hot, HouseParty.hot, HotDate.hot, sep5/6snds.hot) -> station.
    _StationPaths = {
        {260, {"Music/Stations/Country/", false, "Country"}},
        {269, {"Music/Stations/Classica/", false, "Classical"}},
        {270, {"Music/Stations/Latin/", false, "Bossa Nova"}},      // "TrkBossaNova" (stereo_jazz)
        {280, {"Music/Stations/Rock/", false, "Rock"}},
        {6681, {"Music/Stations/CountryD/", false, "Country Dance"}}, // House Party DJ booth
        {6682, {"Music/Stations/Rap/", false, "Rap"}},
        {6683, {"Music/Stations/Rave/", false, "Rave"}},
        {6684, {"Music/Stations/Disco/", false, "Disco"}},
        {6685, {"Music/Stations/Beach/", false, "Beach"}},
        {22900, {"Music/Stations/UnlSpeak/", false, "Zydeco"}},     // Unleashed speakers (cheap) / stereo_zydeco
        {22901, {"Music/Stations/UnlCheap/", false, "Saloon"}},     // speakers_unl_exp / stereo_unlspeakers
        {22902, {"Music/Stations/UnlCheap/", false, "Saloon"}},     // stereo_unleashedui
        {65536, {"Music/Stations/EZ/", false, "Easy Listening"}},  // Hot Date station_ezcheap
        {65537, {"Music/Stations/EZExp/", false, "Lounge"}},       // station_ezexp
        {100100, {"Music/Stations/Superstar/", false, "Superstar"}},
        {26, {"tv_romance", true, "Romance"}},
        {27, {"tv_mystery", true, "Mystery"}},
        {28, {"tv_action", true, "Action"}},
        {29, {"tv_comedy_cartoon", true, "Cartoon"}},
        {103000, {"animal", true, "Animal"}},                      // Superstar TV channels
        {103001, {"entertainment", true, "Entertainment"}},
        {103002, {"news", true, "News"}},
        {103003, {"sports", true, "Sports"}},
    };
    // TS1(sims): kSetMusicMode ids (SimsSound.hot bkground_*, the packs' station_* / music_*).
    std::string nhood = "Music/Modes/Nhood/";
#ifdef ST_AUDIO_DIRENT
    if (!ListDir(BasePath + "/Music/Modes/NhoodUS").empty()) nhood = "Music/Modes/NhoodUS/";
#else
    if (FileExists(BasePath + "/Music/Modes/NhoodUS/nhood1.mp3")) nhood = "Music/Modes/NhoodUS/";
#endif
    _MusicModes = {
        {0, ""},                          // bkground_live: live mode has no score (radio/TV only)
        {1, "Music/Modes/Buy/"},          // bkground_buy1
        {2, "Music/Modes/Build/"},        // bkground_build, bkground_camera
        {3, nhood},                       // bkground_nhood1
        {5, "Music/Modes/Load/"},         // bkground_load
        {6, "Music/Modes/MM_credits/"},   // bkground_credits
        {8, nhood},                       // bkground_family (Create-A-Family keeps the neighbourhood tune)
        {9, ""},                          // bkground_fade
        {10, "Music/Modes/dt_nhood/"},    // station_dtnhood / station_dtbuy (Downtown)
        {11, "Music/Modes/Vacation/"},    // station_vacation (Vacation Island)
        {12, "Music/Stations/UnlCheap/"}, // station_unleashed (Old Town)
        {13, "Music/Modes/Superstar/"},   // station_superstar (Studio Town)
        {14, "Music/Modes/Superstar/"},   // music_superstar_transition
        {15, "Music/Modes/MakinMagic/"},  // music_magictown (Magic Town)
        {16, "Music/Modes/MM_credits/"},  // music_magictown_credits
        {17, "Music/Modes/MM_Build/"},    // music_magictown_build
        {18, "Music/Modes/MM_Buy/"},      // music_magictown_buy
    };
}

std::vector<std::string> Audio::ListMusic(const std::string& relDir) {
    std::vector<std::string> out;
    std::string dir = relDir;
    while (!dir.empty() && dir.back() == '/') dir.pop_back();
#ifdef ST_AUDIO_DIRENT
    for (auto& e : ListDir(BasePath + "/" + dir))
        if (!e.Dir && (EndsWithI(e.Name, ".mp3") || EndsWithI(e.Name, ".wav"))) out.push_back(BasePath + "/" + dir + "/" + e.Name);
#else
    for (auto& k : kKnownMusic) {
        if (Text::ToLower(k.Dir) != Text::ToLower(dir)) continue;
        for (auto& f : Text::Split(k.Files, ',')) {
            std::string p = BasePath + "/" + dir + "/" + f;
            if (FileExists(p)) out.push_back(p);
        }
    }
#endif
    return out;
}

std::vector<AudioReference> Audio::List() {
    std::vector<AudioReference> result;
    for (auto& s : _StationPaths) {
        if (s.second.TV) continue;
        for (auto& f : ListMusic(s.second.Path)) {
            AudioReference r;
            r.ID = s.first;
            r.Type = AudioType::RADIO_STATION;
            r.FilePath = f;
            result.push_back(r);
        }
    }
    return result;
}

Files::HIT::Track* Audio::GetTrack(uint32_t id, HITResourceGroup* prefer, HITResourceGroup** owner) {
    if (prefer != nullptr && prefer->hot != nullptr) {
        auto it = prefer->hot->Tracks.find(id);
        if (it != prefer->hot->Tracks.end()) { if (owner) *owner = prefer; return &it->second; }
    }
    auto it = TracksById.find(id);
    if (it == TracksById.end()) return nullptr;
    if (owner) *owner = it->second.second;
    return it->second.first;
}

std::shared_ptr<Hitlist> Audio::GetHitlist(uint32_t InstanceID, HITResourceGroup* prefer) {
    if (prefer != nullptr && prefer->hot != nullptr) {
        auto it = prefer->hot->Hitlists.find(InstanceID);
        if (it != prefer->hot->Hitlists.end()) return it->second;
    }
    for (auto& g : Groups) {
        auto it = g->hot->Hitlists.find(InstanceID);
        if (it != g->hot->Hitlists.end()) return it->second;
    }
    return nullptr; //found nothing :'(
}

const Files::HIT::Patch* Audio::GetPatch(uint32_t id, HITResourceGroup* prefer, HITResourceGroup** owner) {
    if (prefer != nullptr && prefer->hot != nullptr) {
        auto it = prefer->hot->Patches.find(id);
        if (it != prefer->hot->Patches.end()) { if (owner) *owner = prefer; return &it->second; }
    }
    for (auto& g : Groups) {
        auto it = g->hot->Patches.find(id);
        if (it != g->hot->Patches.end()) { if (owner) *owner = g.get(); return &it->second; }
    }
    return nullptr;
}

const Audio::SampleLoc* Audio::FindSample(const std::string& fileName, HITResourceGroup* prefer) {
    auto it = _SampleIndex.find(AudioNameHash(fileName));
    if (it == _SampleIndex.end() || it->second.empty()) return nullptr;
    if (prefer != nullptr) {
        for (auto& loc : it->second)
            for (int f : prefer->LocalFars) if (loc.Far == f) return &loc;
    }
    return &it->second.front();
}

// TS1(sims): sound archives are read through the shared FilePool (src/files/FilePool.h), like every other archive
bool Audio::ReadArchive(int far, uint32_t offset, uint32_t size, std::vector<uint8_t>& data) {
    if (far < 0 || far >= (int)_Fars.size()) return false;
    return FSO::Files::FilePool::ReadAt(_Fars[far].Path, offset, size, data);
}

bool Audio::ReadSample(const std::string& fileName, std::vector<uint8_t>& data, HITResourceGroup* prefer) {
    auto loc = FindSample(fileName, prefer);
    if (loc == nullptr) return false;
    if (!ReadArchive(loc->Far, loc->Offset, loc->Size, data)) return false;
    return !data.empty();
}

simaudio::SoundBufferPtr Audio::Decode(const SampleLoc& loc, const std::string& name) {
    uint64_t key = ((uint64_t)(uint32_t)loc.Far << 32) | loc.Offset;
    auto c = _Cache.find(key);
    if (c != _Cache.end()) {
        _Lru.erase(c->second.It);
        _Lru.push_front(key);
        c->second.It = _Lru.begin();
        return c->second.Buf;
    }
    std::vector<uint8_t> data;
    if (!ReadArchive(loc.Far, loc.Offset, loc.Size, data)) return nullptr;
    if (data.size() < 8) return nullptr;

    std::shared_ptr<simaudio::SoundBuffer> buf;
    try {
        if (data[0] == 'X' && data[1] == 'A') {
            Files::XA::XAFile xa(data);
            buf = std::make_shared<simaudio::SoundBuffer>();
            buf->Channels = xa.Channels() ? xa.Channels() : 1;
            buf->Rate = (int)(xa.SampleRate() ? xa.SampleRate() : 22050);
            buf->Samples.assign(xa.Samples(), xa.Samples() + xa.SampleCount());
        } else if (std::memcmp(data.data(), "UTM0", 4) == 0) {
            Files::UTK::UTKFile2 utk(data);
            utk.UTKDecode();
            auto& wav = utk.DecompressedWav();
            buf = simaudio::DecodeWav(wav.data(), wav.size());
        } else {
            buf = simaudio::DecodeWav(data.data(), data.size()); //either wav or mp3.
        }
    } catch (std::exception& e) {
        std::printf("audio: cannot decode %s: %s\n", name.c_str(), e.what());
        return nullptr;
    }
    if (!buf || buf->Frames() == 0) return nullptr;

    _CacheBytes += buf->Bytes();
    _Lru.push_front(key);
    _Cache[key] = CacheEntry{ buf, _Lru.begin() };
    // evict least recently used (playing voices keep their buffer alive through the shared_ptr)
    while (_CacheBytes > _CacheBudget && _Lru.size() > 1) {
        uint64_t k = _Lru.back();
        _Lru.pop_back();
        auto e = _Cache.find(k);
        if (e != _Cache.end()) { _CacheBytes -= e->second.Buf->Bytes(); _Cache.erase(e); }
    }
    return buf;
}

simaudio::SoundBufferPtr Audio::GetSampleByName(const std::string& fileName, HITResourceGroup* prefer) {
    auto loc = FindSample(fileName, prefer);
    if (loc == nullptr) return nullptr;
    return Decode(*loc, fileName);
}

static const uint32_t kStreamThreshold = 256 * 1024; // WAV stings above this play as streams

simaudio::SoundBufferPtr Audio::GetSFX(uint32_t InstanceID, HITResourceGroup* prefer) {
    if (InstanceID == 0) return nullptr;
    HITResourceGroup* owner = nullptr;
    auto patch = GetPatch(InstanceID, prefer, &owner);
    if (patch == nullptr || patch->Filename.empty()) { Misses++; return nullptr; }
    auto loc = FindSample(patch->Filename, owner);
    if (loc == nullptr) {
        // loose file next to the install (some downloads ship unpacked samples)
        std::string p = BasePath + "/" + patch->Filename;
        if (FileExists(p)) {
            try {
                if (EndsWithI(p, ".xa")) {
                    Files::XA::XAFile xa(p);
                    auto buf = std::make_shared<simaudio::SoundBuffer>();
                    buf->Channels = xa.Channels() ? xa.Channels() : 1;
                    buf->Rate = (int)xa.SampleRate();
                    buf->Samples.assign(xa.Samples(), xa.Samples() + xa.SampleCount());
                    return buf;
                }
            } catch (std::exception&) {}
        }
        Misses++;
        return nullptr;
    }
    if (loc->Size > kStreamThreshold && EndsWithI(patch->Filename, ".wav")) return nullptr; // streamed
    return Decode(*loc, patch->Filename);
}

std::unique_ptr<simaudio::AudioStream> Audio::GetSFXStream(uint32_t InstanceID, HITResourceGroup* prefer) {
    HITResourceGroup* owner = nullptr;
    auto patch = GetPatch(InstanceID, prefer, &owner);
    if (patch == nullptr) return nullptr;
    auto loc = FindSample(patch->Filename, owner);
    if (loc == nullptr || loc->Size <= kStreamThreshold) return nullptr;
    return simaudio::WavStream::Open(_Fars[loc->Far].Path, (long)loc->Offset, (long)loc->Size);
}

}} // namespace FSO::Content
