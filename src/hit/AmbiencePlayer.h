/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims SimsVille/HIT/AmbiencePlayer.cs (namespace TSO.HIT -> FSO::HIT)
// TSO lot ambience (VMAmbientSound's sounddata/ambience/*); The Sims 1 has no such files.
#pragma once
#include <string>

namespace FSO { namespace HIT {

class FSCPlayer;

struct Ambience
{
    std::string Path;
    bool Loop = false; //certain ambiences are simple xa loops instead of fscs.

    Ambience() {}
    Ambience(const std::string& path, bool loop) : Path(path), Loop(loop) {}
};

class AmbiencePlayer
{
private:
    bool fscMode = false;
    FSCPlayer* fsc = nullptr;
    int inst = 0; // mixer voice

public:
    /// basePath: the content root (C#: Content.GetPath).
    AmbiencePlayer(const Ambience& amb, const std::string& basePath);
    void Kill();
};

}} // namespace FSO::HIT
