/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.Vitaboy.Mesh (sims.common/rendering/vitaboy/Mesh.cs)
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "vitaboy/BlendData.h"
#include "vitaboy/Bone.h"

namespace FSO { namespace Vitaboy {

class IOProxy;

/// <summary>
/// 3D Mesh.
/// </summary>
class Mesh {
public:
    /** 3D Data **/
    std::vector<VitaboyVertex> VertexBuffer;

    std::vector<int32_t> BlendVertBoneIndices;
    std::vector<Vector3> BlendVerts;

    std::vector<int16_t> IndexBuffer;
    int32_t NumPrimitives = 0;
    std::vector<BoneBinding> BoneBindings;
    std::vector<FSO::Vitaboy::BlendData> BlendData;

    std::string SkinName;
    std::string TextureName;

    Mesh() {}

    /// <summary>
    /// Clones this mesh (caller owns the result).
    /// </summary>
    Mesh* Clone() const;

    /// <summary>
    /// Transforms the verticies making up this mesh into the designated bone positions.
    /// (Assigns the skeleton's bone indices to the vertices; runs once per mesh.)
    /// </summary>
    void Prepare(Bone* bone);

    // STUB(3ds): StoreOnGPU / InvalidateMesh / DrawGeometry / Draw were MonoGame vertex/index
    // buffer uploads; the software renderer (AvatarRenderer) reads VertexBuffer/IndexBuffer directly.
    void InvalidateMesh() {}

    /// <summary>
    /// Reads a mesh.
    ///   bmf=false: TSO .mesh (big endian)
    ///   bmf=true : TS1 .bmf (binary, little endian) or, through CMXReadProxy, TS1 .skn (text)
    /// </summary>
    void Read(const std::vector<uint8_t>& data, bool bmf);
    void Read(IOProxy& io, bool bmf);

    bool Prepared() const { return m_Prepared; }
private:
    bool m_Prepared = false;
};

}}
