/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.Vitaboy.Skeleton (sims.common/rendering/vitaboy/Skeleton.cs)
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "vitaboy/Bone.h"

namespace FSO { namespace Vitaboy {

class IOProxy;

/// <summary>
/// Skeletons specify the network of bones that can be moved by an animation to bend
/// the applied meshes of a rendered character. Skeletons also provide non-animated
/// default translation and rotation values for each bone.
/// </summary>
class Skeleton {
public:
    std::string Name;
    std::vector<std::unique_ptr<Bone>> Bones;
    Bone* RootBone = nullptr;

    Skeleton() {}
    Skeleton(const Skeleton&) = delete;
    Skeleton& operator=(const Skeleton&) = delete;

    /// <summary>
    /// Gets a bone from this Skeleton instance (exact, case-sensitive match as in C#).
    /// </summary>
    Bone* GetBone(const std::string& name) const;

    /// <summary>
    /// Clones this skeleton (caller owns the result).
    /// </summary>
    Skeleton* Clone() const;

    /// <summary>
    /// Reads a skeleton. bcf=false: TSO .skel (big endian, version + int16 bone count).
    /// bcf=true: TS1 BCF/CMX skeleton record.
    /// </summary>
    void Read(const std::vector<uint8_t>& data, bool bcf);
    void Read(IOProxy& io, bool bcf);

    /// <summary>
    /// Computes the absolute position for all the bones in this skeleton.
    /// </summary>
    void ComputeBonePositions(Bone* bone, const Matrix& world);

private:
    Bone* ReadBone(IOProxy& reader, bool bcf);
    void ConstructTree();
};

}}
