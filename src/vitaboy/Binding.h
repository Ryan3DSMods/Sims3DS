/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.Vitaboy.Binding (sims.common/rendering/vitaboy/Binding.cs)
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace FSO { namespace Vitaboy {

/// <summary>
/// Bindings points to meshes and appearances.
/// </summary>
class Binding {
public:
    std::string Bone;
    uint32_t MeshGroupID = 0;
    uint32_t MeshFileID = 0;
    uint32_t MeshTypeID = 0;

    // C# string fields default to null; HasMeshName/HasTextureName stand in for "!= null".
    std::string MeshName; //bmd
    std::string TextureName;
    bool HasMeshName = false;
    bool HasTextureName = false;

    uint32_t TextureGroupID = 0;
    uint32_t TextureFileID = 0;
    uint32_t TextureTypeID = 0;

    // TS1 BCF/CMX extras (censor flags / unknown, read verbatim)
    int32_t CensorFlags = 0;
    int32_t Unknown = 0;

    Binding TS1Copy() const {
        Binding b;
        b.MeshName = MeshName; b.HasMeshName = HasMeshName;
        b.TextureName = TextureName; b.HasTextureName = HasTextureName;
        b.Bone = Bone; // not copied in C# (unused by AddBinding); kept for debugging
        return b;
    }

    /// <summary>
    /// Reads a TSO binding (.bnd, big endian).
    /// </summary>
    void Read(const std::vector<uint8_t>& data);
};

}}
