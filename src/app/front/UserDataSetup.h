/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims front end — the user save area (what the PC installer and the game do on first run).
//
// TS1(sims). The Complete Collection installer copies the expansion packs' template lots into every
// neighbourhood folder (TemplateDowntown -> Houses/House21-30 + DTDesc.iff, TemplateVacation -> 40-48 + VIDesc,
// TemplateCommunity -> 50-80 + NeighborhoodDesc + LotZoning/StreetNames/LotLocations, TemplateStudiotown ->
// 81-89 + STDesc, TemplateMagicTown -> 90-99 + MTDesc, TemplateNPCs -> Characters/), and the game creates
// UserData2..UserData10 from TemplateUserData when the player switches to an empty neighbourhood.
// Like Simitone/FreeSims ("simitone shouldn't modify existing ts1 data ... we should copy to the simitone user
// data"), the install tree is never written: everything is copied into <SaveRoot>/UserData, UserData2, ...
// once (files that already exist there are never overwritten), and the game plays and saves there.
//
// The copy is incremental (Step) so the loading screen keeps animating while it runs on the console.
#pragma once
#include <set>
#include <string>
#include <vector>

namespace simapp { namespace front {

class UserDataSetup {
public:
    /// Neighbourhood folder name: 0 -> "UserData", n -> "UserData<n+1>" (as the game).
    static std::string FolderName(int index);
    static const int MaxNeighbourhoods = 10;

    void Configure(const std::string& dataRoot, const std::string& saveRoot);
    const std::string& SaveRoot() const { return _Save; }
    const std::string& DataRoot() const { return _Data; }

    /// Plans the copy of every neighbourhood the install has (UserData, UserData2, ...) that is not set up yet.
    void PlanAll();
    /// Plans a brand-new neighbourhood (from TemplateUserData) at `index`.
    void PlanNew(int index);
    /// Copies up to ~budgetBytes. Returns true while work remains. `status` names the current file.
    bool Step(size_t budgetBytes, std::string* status = nullptr);
    float Progress() const;
    bool Busy() const { return _Next < _Jobs.size(); }

    /// Neighbourhoods available in the save area (set up), ascending. FIX(sims) SAVE: a set-up neighbourhood whose
    /// Neighborhood.iff is missing gets it back from an interrupted save's .new / .tmp / .bak here (SafeFile::Recover);
    /// when nothing usable is left it is listed by Broken() instead.
    std::vector<int> Available() const;
    /// FIX(sims) SAVE: set-up neighbourhoods whose Neighborhood.iff is missing or damaged with no usable backup.
    std::vector<int> Broken() const;
    void MarkBroken(int index) const { _Broken.insert(index); }
    /// FIX(sims) SAVE: the install has an original for this neighbourhood (UserData<n>, else TemplateUserData).
    bool CanRestore(int index) const;
    /// FIX(sims) SAVE: replaces this neighbourhood (only this folder) by a fresh copy from the install: the damaged
    /// folder is renamed to "<folder>.damaged[N]" (kept for a manual rescue) and copied again (blocking).
    bool RestoreFromInstall(int index, std::string* error = nullptr, std::string* log = nullptr);
    /// Install neighbourhoods that exist (UserData, UserData2, ...): what the player can have.
    bool HasTemplates() const;
    std::string NeighbourhoodDir(int index) const { return _Save + "/" + FolderName(index); }

    static bool FileExists(const std::string& p);
    static bool DirExists(const std::string& p);
    static void MakeDirs(const std::string& p);
    /// Lists file names in a directory (dirent when the platform has it, else probing known name patterns).
    static std::vector<std::string> ListFiles(const std::string& dir, const std::string& probePattern = "");

private:
    struct Job { std::string Src, Dst; size_t Size = 0; bool Marker = false; };
    std::string _Data, _Save;
    std::vector<Job> _Jobs;
    size_t _Next = 0, _TotalBytes = 0, _DoneBytes = 0;
    size_t _Offset = 0; // within the current job
    void Plan(int index, const std::string& sourceFolder);
    mutable std::set<int> _Checked, _Broken; // FIX(sims) SAVE: Available()'s recovery, once per run
    void Add(const std::string& src, const std::string& dst);
};

}}
