/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.Vitaboy.Mesh (sims.common/rendering/vitaboy/Mesh.cs)
#include "vitaboy/Mesh.h"
#include "vitaboy/BCFReadProxy.h"
#include <cctype>

namespace FSO { namespace Vitaboy {

static bool EqualsIgnoreCase(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++)
        if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i])) return false;
    return true;
}

Mesh* Mesh::Clone() const {
    auto result = new Mesh();
    result->BlendData = BlendData;
    result->BoneBindings = BoneBindings;
    result->NumPrimitives = NumPrimitives;
    result->IndexBuffer = IndexBuffer;
    result->VertexBuffer = VertexBuffer;
    result->BlendVerts = BlendVerts;
    result->BlendVertBoneIndices = BlendVertBoneIndices;
    result->SkinName = SkinName;
    result->TextureName = TextureName;
    return result;
}

//TODO: assumes that skeleton will be same configuration for all bindings of this mesh.
//If any meshes are used by pets and avatars(???) or we implement children this will need
//to be changed to support binds to multiple SKEL bases.
// (TS1 adult and child skeletons share the same bone order, so this holds for the base game.)
void Mesh::Prepare(Bone* bone) {
    if (m_Prepared) return;
    const BoneBinding* binding = nullptr;
    for (auto& x : BoneBindings) if (EqualsIgnoreCase(x.BoneName, bone->Name)) { binding = &x; break; }
    if (binding != nullptr) {
        for (int32_t i = 0; i < binding->RealVertexCount; i++) {
            auto vertexIndex = binding->FirstRealVertex + i;
            if (vertexIndex >= 0 && vertexIndex < (int32_t)VertexBuffer.size())
                VertexBuffer[vertexIndex].Parameters.X = (float)bone->Index;
        }

        for (int32_t i = 0; i < binding->BlendVertexCount; i++) {
            auto blendVertexIndex = binding->FirstBlendVertex + i;
            if (blendVertexIndex >= 0 && blendVertexIndex < (int32_t)BlendVertBoneIndices.size())
                BlendVertBoneIndices[blendVertexIndex] = bone->Index;
        }
    }

    for (auto child : bone->Children) {
        Prepare(child);
    }

    if (EqualsIgnoreCase(bone->Name, "ROOT")) {
        for (size_t i = 0; i < BlendData.size(); i++) {
            auto& data = BlendData[i];
            if (data.OtherVertex < 0 || data.OtherVertex >= (int32_t)VertexBuffer.size()) continue;

            VertexBuffer[data.OtherVertex].Parameters.Y = (float)BlendVertBoneIndices[i];
            VertexBuffer[data.OtherVertex].Parameters.Z = data.Weight;
            VertexBuffer[data.OtherVertex].BvPosition = BlendVerts[i];
        }

        InvalidateMesh();
        m_Prepared = true;
    }
}

void Mesh::Read(const std::vector<uint8_t>& data, bool bmf) {
    BinaryReadProxy io(data, bmf ? st::Endian::Little : st::Endian::Big);
    Read(io, bmf);
}

/// Layout (identical for TSO .mesh, TS1 .bmf and TS1 .skn text, apart from the header):
///   [bmf/skn: pstr SkinName, pstr TextureName | mesh: int32 version]
///   int32 boneCount x pstr boneName
///   int32 faceCount x (int32 a, int32 b, int32 c)
///   int32 bindingCount x (int32 boneIndex, firstReal, realCount, firstBlend, blendCount)
///   int32 realVertexCount x (float u, float v)
///   int32 blendVertexCount x (int32 weight(0x8000 = 1.0), int32 otherVertex)
///        NOTE: the TS1 text .skn writes each blend line as "otherVertex weight" (verified on the
///        shipped skins: the first column indexes real vertices, the second is 0..0x8000).
///   int32 realVertexCount+blendVertexCount, then real vertices then blend vertices,
///        each (float px,py,pz, nx,ny,nz) with x negated for XNA handedness.
void Mesh::Read(IOProxy& io, bool bmf) {
    if (bmf) {
        SkinName = io.ReadPascalString();
        TextureName = io.ReadPascalString();
    } else {
        auto version = io.ReadInt32(); (void)version;
    }
    auto boneCount = io.ReadInt32();
    std::vector<std::string> boneNames(boneCount > 0 ? boneCount : 0);
    for (int32_t i = 0; i < boneCount; i++) {
        boneNames[i] = io.ReadPascalString();
    }

    auto faceCount = io.ReadInt32();
    NumPrimitives = faceCount;

    IndexBuffer.resize(faceCount * 3);
    int32_t offset = 0;
    for (int32_t i = 0; i < faceCount; i++) {
        IndexBuffer[offset++] = (int16_t)io.ReadInt32();
        IndexBuffer[offset++] = (int16_t)io.ReadInt32();
        IndexBuffer[offset++] = (int16_t)io.ReadInt32();
    }

    /** Bone bindings **/
    auto bindingCount = io.ReadInt32();
    BoneBindings.resize(bindingCount);
    for (int32_t i = 0; i < bindingCount; i++) {
        BoneBinding b;
        b.BoneIndex = io.ReadInt32();
        b.FirstRealVertex = io.ReadInt32();
        b.RealVertexCount = io.ReadInt32();
        b.FirstBlendVertex = io.ReadInt32();
        b.BlendVertexCount = io.ReadInt32();
        if (b.BoneIndex >= 0 && b.BoneIndex < (int32_t)boneNames.size()) b.BoneName = boneNames[b.BoneIndex];
        BoneBindings[i] = b;
    }

    auto realVertexCount = io.ReadInt32();
    VertexBuffer.assign(realVertexCount, VitaboyVertex());

    for (int32_t i = 0; i < realVertexCount; i++) {
        VertexBuffer[i].TextureCoordinate.X = io.ReadFloat();
        VertexBuffer[i].TextureCoordinate.Y = io.ReadFloat();
    }

    /** Blend data **/
    auto blendVertexCount = io.ReadInt32();
    BlendData.resize(blendVertexCount);
    bool text = io.IsText();
    for (int32_t i = 0; i < blendVertexCount; i++) {
        if (text) { // .skn: "otherVertex weight"
            BlendData[i].OtherVertex = io.ReadInt32();
            BlendData[i].Weight = (float)io.ReadInt32() / 0x8000;
        } else {
            BlendData[i].Weight = (float)io.ReadInt32() / 0x8000;
            BlendData[i].OtherVertex = io.ReadInt32();
        }
    }

    auto realVertexCount2 = io.ReadInt32(); (void)realVertexCount2;

    for (int32_t i = 0; i < realVertexCount; i++) {
        float px = -io.ReadFloat(), py = io.ReadFloat(), pz = io.ReadFloat();
        VertexBuffer[i].Position = Vector3(px, py, pz);
        float nx = -io.ReadFloat(), ny = io.ReadFloat(), nz = io.ReadFloat();
        VertexBuffer[i].Normal = Vector3(nx, ny, nz);
    }

    BlendVerts.resize(blendVertexCount);

    for (int32_t i = 0; i < blendVertexCount; i++) {
        float px = -io.ReadFloat(), py = io.ReadFloat(), pz = io.ReadFloat();
        BlendVerts[i] = Vector3(px, py, pz);

        float nx = -io.ReadFloat(), ny = io.ReadFloat(), nz = io.ReadFloat();
        (void)nx; (void)ny; (void)nz; //todo: read this in somewhere and use it for lighting.
    }

    BlendVertBoneIndices.assign(blendVertexCount, 0);
}

}}
