/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/formats/iff/IffRuntimeInfo.cs
#pragma once
#include <string>
#include <vector>

namespace FSO { namespace Files { namespace Formats { namespace IFF {

class IffFile;

enum class IffRuntimeState
{
    ReadOnly, //orignal game iff
    PIFFPatch, //replacement patch
    PIFFClone, //clone of original object
    Standalone //standalone, mutable iff
};

enum class IffUseCase
{
    Global,
    Object,
    ObjectSprites,
};

class IffRuntimeInfo
{
public:
    IffRuntimeState State = IffRuntimeState::ReadOnly;
    IffUseCase UseCase = IffUseCase::Global;
    std::string Filename;
    std::string Path;
    bool Dirty = false;
    std::vector<IffFile*> Patches;
};

}}}} // namespace FSO::Files::Formats::IFF
