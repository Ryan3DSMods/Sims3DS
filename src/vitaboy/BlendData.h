/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.Vitaboy.BlendData / BoneBinding / Model.VitaboyVertex / MeshVertex
// (BlendData.cs, BoneBinding.cs, VitaboyVertex.cs, MeshVertex.cs) - small POD types, header-only.
#pragma once
#include <cstdint>
#include <string>
#include "vitaboy/VitaboyMath.h"

namespace FSO { namespace Vitaboy {

/// <summary>
/// Blending data for a mesh/model.
/// </summary>
class BlendData {
public:
    float Weight = 0;
    int32_t OtherVertex = 0;
};

/// <summary>
/// A bone binding associates vertices and blende vertices with bones.
/// </summary>
class BoneBinding {
public:
    std::string BoneName;
    int32_t BoneIndex = 0;
    int32_t FirstRealVertex = 0;
    int32_t RealVertexCount = 0;
    int32_t FirstBlendVertex = 0;
    int32_t BlendVertexCount = 0;
};

/// FSO.Vitaboy.Model.VitaboyVertex. Parameters = (bone index, blend bone index, blend weight).
/// Positions are relative to the bone they are bound to (skinned by SkelBindings[Parameters.X]).
struct VitaboyVertex {
    Vector3 Position;
    Vector2 TextureCoordinate;
    Vector3 BvPosition; //blend vert
    Vector3 Parameters;
    Vector3 Normal;
};

/// FSO.Vitaboy.MeshVertex (unused by the TS1 path; kept for completeness).
struct MeshVertex {
    Vector3 Position;
    /** UV Mapping **/
    Vector2 UV;
    Vector3 Normal;
};

}}
