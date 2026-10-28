/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims front end — see UserDataSetup.h. TS1(sims): new code.
#include "files/SafeFile.h" // FIX(sims) SAVE
#include "content/TS1FamilyImport.h"
#include <set>
#include "files/FilePool.h"
#include "files/LoadProgress.h"
#include "app/front/UserDataSetup.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <sys/stat.h>
// directory listing: POSIX hosts, and the 3DS when the build says devkitARM's libsysbase opendir is there
// (src/platform/3ds/Makefile passes -DST_HAVE_DIRENT); plain arm-none-eabi newlib has none (probing instead).
#if defined(ST_HAVE_DIRENT)
#include <dirent.h>
#define ST_USE_DIRENT 1
#elif !defined(__3DS__) && defined(__has_include)
#if __has_include(<dirent.h>)
#include <dirent.h>
#define ST_USE_DIRENT 1
#endif
#endif

namespace simapp { namespace front {

static const char* MARKER = "sims_setup.txt";

std::string UserDataSetup::FolderName(int index) {
    return index <= 0 ? std::string("UserData") : "UserData" + std::to_string(index + 1);
}

bool UserDataSetup::FileExists(const std::string& p) {
    FILE* f = fopen(p.c_str(), "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

bool UserDataSetup::DirExists(const std::string& p) {
    struct stat st;
    return stat(p.c_str(), &st) == 0 && (st.st_mode & S_IFDIR);
}

void UserDataSetup::MakeDirs(const std::string& p) {
    for (size_t i = 1; i <= p.size(); i++) {
        if (i == p.size() || p[i] == '/') {
            std::string sub = p.substr(0, i);
            if (!sub.empty() && sub.back() != ':' && !DirExists(sub)) mkdir(sub.c_str(), 0777);
        }
    }
}

std::vector<std::string> UserDataSetup::ListFiles(const std::string& dir, const std::string& probePattern) {
    std::vector<std::string> r;
#ifdef ST_USE_DIRENT
    if (DIR* d = opendir(dir.c_str())) {
        while (dirent* e = readdir(d)) {
            std::string n = e->d_name;
            if (n == "." || n == "..") continue;
            if (FileExists(dir + "/" + n)) r.push_back(n);
        }
        closedir(d);
        std::sort(r.begin(), r.end());
        return r;
    }
#endif
    // no directory listing: probe "<pattern with %d>" names (e.g. "User%05d.iff"), stop after a long gap
    if (!probePattern.empty()) {
        int misses = 0;
        for (int i = 0; i < 2000 && misses < 64; i++) {
            char name[64];
            snprintf(name, sizeof name, probePattern.c_str(), i);
            if (FileExists(dir + "/" + name)) { r.push_back(name); misses = 0; }
            else misses++;
        }
    }
    return r;
}

void UserDataSetup::Configure(const std::string& dataRoot, const std::string& saveRoot) {
    _Data = dataRoot;
    _Save = saveRoot;
    _Jobs.clear();
    _Next = _TotalBytes = _DoneBytes = 0;
}

static size_t FileSize(const std::string& p) {
    struct stat st;
    if (stat(p.c_str(), &st) != 0) return 0;
    return (size_t)st.st_size;
}

void UserDataSetup::Add(const std::string& src, const std::string& dst) {
    if (!FileExists(src)) return;
    Job j;
    j.Src = src; j.Dst = dst; j.Size = FileSize(src);
    _Jobs.push_back(j);
    _TotalBytes += j.Size;
}

bool UserDataSetup::HasTemplates() const { return DirExists(_Data + "/TemplateUserData"); }

void UserDataSetup::Plan(int index, const std::string& source) {
    std::string dst = NeighbourhoodDir(index);
    if (FileExists(dst + "/" + MARKER)) return;
    MakeDirs(dst + "/Houses");
    MakeDirs(dst + "/Characters");
    auto keep = [&](const std::string& d) { return !FileExists(d); }; // never overwrite the player's files
    // the neighbourhood itself
    if (keep(dst + "/Neighborhood.iff")) Add(source + "/Neighborhood.iff", dst + "/Neighborhood.iff");
    if (keep(dst + "/LotZoning.iff")) Add(source + "/LotZoning.iff", dst + "/LotZoning.iff");
    for (int h = 0; h < 100; h++) {
        char n[32]; snprintf(n, sizeof n, "/Houses/House%02d.iff", h);
        if (keep(dst + n)) Add(source + n, dst + n);
    }
    for (auto& f : ListFiles(source + "/Characters", "User%05d.iff"))
        if (keep(dst + "/Characters/" + f)) Add(source + "/Characters/" + f, dst + "/Characters/" + f);
    // the expansion packs' template lots (installer). Later templates carry the more complete zoning /
    // street / lot location tables (Magic Town's lists every lot), so they replace the earlier ones.
    struct Tpl { const char* Dir; int First, Last; const char* Desc; };
    static const Tpl tpls[] = {
        { "TemplateCommunity", 50, 80, "NeighborhoodDesc.iff" },
        { "TemplateDowntown", 21, 30, "DTDesc.iff" },
        { "TemplateVacation", 40, 49, "VIDesc.iff" },
        { "TemplateStudiotown", 81, 89, "STDesc.iff" },
        { "TemplateMagicTown", 90, 99, "MTDesc.iff" },
    };
    bool userZoning = FileExists(dst + "/LotZoning.iff"); // already in the save area: the player's (rezoned) lots
    // FIX(sims): the Old Town homes of the Unleashed families (TemplateFamilyUnleashed/*.FAM: 56 63 69 76 77) are
    // not copied from TemplateCommunity: Maxis saved those with other GUIDs for the family members (14 missing
    // objects each). NhoodModel::Switch installs the families with their own houses (TS1FamilyImport::InstallTemplates)
    // the first time the neighbourhood is opened; until then the provider falls back to the template copy.
    std::set<int> famHouses;
    {
        std::string fd = _Data + "/TemplateFamilyUnleashed";
        auto fams = ListFiles(fd);
        if (fams.empty()) fams = { "Burb_11.FAM", "Charming_13.FAM", "Goth Sr_12.FAM", "Hick_10.FAM", "Kat_6.FAM", "Strays_4000.FAM" };
        for (auto& f : fams) {
            FSO::Content::TS1::TS1FamFile fam;
            if (FileExists(fd + "/" + f) && FSO::Content::TS1::TS1FamilyImport::Read(fd + "/" + f, fam) && fam.HouseNumber > 0) famHouses.insert(fam.HouseNumber);
        }
    }
    for (auto& t : tpls) {
        std::string td = _Data + "/" + t.Dir;
        if (!DirExists(td)) continue;
        for (int h = t.First; h <= t.Last; h++) {
            char n[32]; snprintf(n, sizeof n, "House%02d.iff", h);
            if (famHouses.count(h)) continue;
            if (keep(dst + "/Houses/" + n)) Add(td + "/" + n, dst + "/Houses/" + n);
        }
        if (keep(dst + "/Houses/" + t.Desc)) Add(td + "/" + t.Desc, dst + "/Houses/" + t.Desc);
        for (const char* meta : { "LotZoning.iff", "LotLocations.iff", "StreetNames.iff" }) {
            if (std::string(meta) == "LotZoning.iff" && userZoning) continue;
            Add(td + "/" + meta, dst + "/" + meta);
        }
    }
    std::string npcs = _Data + "/TemplateNPCs";
    for (auto& f : ListFiles(npcs)) {
        if (f.size() < 4) continue;
        std::string ext = f.substr(f.size() - 4);
        for (auto& c : ext) c = (char)tolower((unsigned char)c);
        if (ext == ".iff" && keep(dst + "/Characters/" + f)) Add(npcs + "/" + f, dst + "/Characters/" + f);
    }
    Job m;
    m.Dst = dst + "/" + MARKER;
    m.Marker = true;
    _Jobs.push_back(m);
}

void UserDataSetup::PlanAll() {
    for (int i = 0; i < MaxNeighbourhoods; i++) {
        std::string src = _Data + "/" + FolderName(i);
        if (FileExists(src + "/Neighborhood.iff")) Plan(i, src);
        // FIX(sims) SAVE: a neighbourhood created from TemplateUserData whose copy was cut (power off / HOME close)
        // has no marker yet: finish it (only missing files are copied)
        else if (FileExists(NeighbourhoodDir(i) + "/Neighborhood.iff") && !FileExists(NeighbourhoodDir(i) + "/" + MARKER) &&
                 FileExists(_Data + "/TemplateUserData/Neighborhood.iff")) Plan(i, _Data + "/TemplateUserData");
    }
}

void UserDataSetup::PlanNew(int index) {
    std::string src = _Data + "/TemplateUserData";
    if (FileExists(_Data + "/" + FolderName(index) + "/Neighborhood.iff")) src = _Data + "/" + FolderName(index);
    Plan(index, src);
}

bool UserDataSetup::Step(size_t budget, std::string* status) {
    size_t done = 0;
    std::vector<uint8_t> buf(64 * 1024);
    while (_Next < _Jobs.size() && done < budget) {
        Job& j = _Jobs[_Next++];
        if (status) *status = j.Dst;
        if (j.Marker) {
            FSO::Files::SafeFile::WriteText(j.Dst, "sims save area: copied from the install (UserData + expansion pack templates).\n");
            continue;
        }
        FILE* in = fopen(j.Src.c_str(), "rb");
        if (!in) continue;
        // FIX(sims) SAVE: copied to <dst>.new and renamed when complete. Before, a copy cut by a power-off / HOME close
        // left a truncated file that the next start kept for good ("never overwrite the player's files").
        FILE* out = FSO::Files::SafeFile::BeginWrite(j.Dst);
        if (!out) { fclose(in); FSO::Files::LoadProgress::Step("WARNING: could not write " + j.Dst); continue; }
        size_t n;
        bool ok = true;
        while ((n = fread(buf.data(), 1, buf.size(), in)) > 0) {
            FSO::Files::FilePool::Stats.BytesRead += n; // TS1(sims): counted for the boot log / SD estimate
            if (fwrite(buf.data(), 1, n, out) != n) { ok = false; break; }
        }
        if (ferror(in)) ok = false;
        fclose(in);
        if (!FSO::Files::SafeFile::EndWrite(j.Dst, out, nullptr, ok)) // TS1(sims): SD card full / write error
            FSO::Files::LoadProgress::Step("WARNING: could not write " + j.Dst + " (SD card full?)");
        done += j.Size;
        _DoneBytes += j.Size;
    }
    return _Next < _Jobs.size();
}

float UserDataSetup::Progress() const {
    if (_Jobs.empty()) return 1.0f;
    if (_TotalBytes == 0) return (float)_Next / (float)_Jobs.size();
    return std::min(1.0f, (float)_DoneBytes / (float)_TotalBytes);
}

std::vector<int> UserDataSetup::Available() const {
    std::vector<int> r;
    for (int i = 0; i < MaxNeighbourhoods; i++) {
        std::string nh = NeighbourhoodDir(i) + "/Neighborhood.iff";
        if (FileExists(nh)) {
            if (!_Broken.count(i)) r.push_back(i); // (damaged contents: NhoodModel::Switch validates and marks it)
            continue;
        }
        // FIX(sims) SAVE: set up (marker) but no Neighborhood.iff = an interrupted save (the user's v6 card): restore
        // the newest complete .new / .tmp / .bak, once per run. Healthy folders cost nothing extra here.
        if (_Checked.count(i) || !FileExists(NeighbourhoodDir(i) + "/" + MARKER)) continue;
        _Checked.insert(i);
        auto res = FSO::Files::SafeFile::Recover(nh, FSO::Files::SafeFile::IffValidator("NBRS"));
        if (res == FSO::Files::SafeFile::Result::Restored) { _Broken.erase(i); r.push_back(i); }
        else {
            _Broken.insert(i);
            FSO::Files::LoadProgress::Step("WARNING: " + FolderName(i) + "/Neighborhood.iff is missing and has no backup");
        }
    }
    return r;
}

std::vector<int> UserDataSetup::Broken() const {
    Available();
    return std::vector<int>(_Broken.begin(), _Broken.end());
}

bool UserDataSetup::CanRestore(int index) const {
    return FileExists(_Data + "/" + FolderName(index) + "/Neighborhood.iff") || FileExists(_Data + "/TemplateUserData/Neighborhood.iff");
}

bool UserDataSetup::RestoreFromInstall(int index, std::string* error, std::string* log) {
    auto say = [&](const std::string& l) { printf("[save] %s\n", l.c_str()); fflush(stdout); if (log) *log += l + "\n"; };
    if (!CanRestore(index)) { if (error) *error = "The game's original of this neighborhood was not found."; return false; }
    std::string dir = NeighbourhoodDir(index);
    FSO::Files::FilePool::CloseAll(); // no handle may stay open inside the folder (the 3DS refuses to rename it)
    if (DirExists(dir)) {
        std::string aside;
        for (int n = 0; n < 100; n++) {
            aside = dir + ".damaged" + (n ? std::to_string(n + 1) : std::string());
            if (!DirExists(aside) && !FileExists(aside)) break;
        }
        if (rename(dir.c_str(), aside.c_str()) == 0) say("restore: moved " + dir + " to " + aside);
        else {
            // cannot move the folder: move the neighbourhood file and the marker aside, the copy then fills in only
            // what is missing (the player's houses / characters stay)
            int e = errno;
            say("restore: cannot rename " + dir + " (" + std::to_string(e) + " " + strerror(e) + "): replacing Neighborhood.iff only");
            std::string nh = dir + "/Neighborhood.iff";
            if (FileExists(nh)) { remove((nh + ".damaged").c_str()); rename(nh.c_str(), (nh + ".damaged").c_str()); }
            remove((dir + "/" + MARKER).c_str());
            remove((dir + "/fam_templates.txt").c_str());
        }
    }
    size_t jobs = _Jobs.size();
    PlanNew(index);
    if (_Jobs.size() == jobs) { if (error) *error = "Nothing to copy for " + dir; return false; }
    while (Step(4u << 20)) {}
    _Checked.erase(index);
    _Broken.erase(index);
    bool ok = FileExists(dir + "/Neighborhood.iff");
    say(std::string("restore: ") + FolderName(index) + (ok ? " copied from the original game" : " FAILED (SD card full?)"));
    if (!ok && error) *error = "Could not copy the neighborhood (SD card full?).";
    return ok;
}

}}
