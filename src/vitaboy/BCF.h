/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.Vitaboy.BCF (sims.common/rendering/vitaboy/BCF.cs)
#pragma once
#include <cstdint>
#include <memory>
#include <vector>
#include "vitaboy/Skeleton.h"
#include "vitaboy/Appearance.h"
#include "vitaboy/Animation.h"

namespace FSO { namespace Vitaboy {

/// <summary>
/// A TS1 character file: skeletons, suits (appearances) and skills (animation headers).
/// Binary .bcf (cmx=false) or text .cmx (cmx=true); both have the same record layout:
///   int32 skeletonCount x Skeleton, int32 suitCount x Appearance, int32 skillCount x Animation.
/// </summary>
class BCF {
public:
    std::vector<std::unique_ptr<Skeleton>> Skeletons;
    std::vector<std::unique_ptr<Appearance>> Appearances;
    std::vector<std::unique_ptr<Animation>> Animations;

    BCF(const std::vector<uint8_t>& data, bool cmx = false);
};

}}
