/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.Vitaboy.Appearance (sims.common/rendering/vitaboy/Appearance.cs) + Binding.Read (Binding.cs)
#include "vitaboy/Appearance.h"
#include "vitaboy/BCFReadProxy.h"
#include <stdexcept>

namespace FSO { namespace Vitaboy {

void Appearance::ReadBCF(IOProxy& io) {
    Name = io.ReadPascalString();
    auto type = io.ReadInt32();
    auto zero = io.ReadInt32(); (void)zero;
    Type = type;

    auto numBindings = io.ReadUInt32();
    Bindings.resize(numBindings);

    for (uint32_t i = 0; i < numBindings; i++) {
        //bindings are included verbatim here.
        Binding bnd;
        bnd.Bone = io.ReadPascalString();
        bnd.MeshName = io.ReadPascalString();
        bnd.HasMeshName = true;
        bnd.CensorFlags = io.ReadInt32();
        bnd.Unknown = io.ReadInt32();

        Bindings[i].RealBinding = bnd;
        Bindings[i].HasRealBinding = true;
    }
}

void Appearance::Read(const std::vector<uint8_t>& data) {
    BinaryReadProxy io(data, st::Endian::Big);
    auto version = io.ReadUInt32(); (void)version;

    ThumbnailFileID = io.ReadUInt32();
    ThumbnailTypeID = io.ReadUInt32();

    auto numBindings = io.ReadUInt32();
    Bindings.resize(numBindings);

    for (uint32_t i = 0; i < numBindings; i++) {
        Bindings[i].FileID = io.ReadUInt32();
        Bindings[i].TypeID = io.ReadUInt32();
    }
}

void Binding::Read(const std::vector<uint8_t>& data) {
    BinaryReadProxy io(data, st::Endian::Big);
    auto version = io.ReadUInt32();
    if (version != 1) {
        throw std::runtime_error("Unknown binding version");
    }

    Bone = io.ReadPascalString();
    auto meshType = io.ReadUInt32();
    if (meshType == 8) {
        this->MeshGroupID = io.ReadUInt32();
        this->MeshFileID = io.ReadUInt32();
        this->MeshTypeID = io.ReadUInt32();
    }

    auto textureType = io.ReadUInt32();
    if (textureType == 8) {
        this->TextureGroupID = io.ReadUInt32();
        this->TextureFileID = io.ReadUInt32();
        this->TextureTypeID = io.ReadUInt32();
    }
}

}}
