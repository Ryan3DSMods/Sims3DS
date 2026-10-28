/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.Vitaboy.Skeleton (sims.common/rendering/vitaboy/Skeleton.cs)
#include "vitaboy/Skeleton.h"
#include "vitaboy/BCFReadProxy.h"

namespace FSO { namespace Vitaboy {

Bone* Skeleton::GetBone(const std::string& name) const {
    for (auto& b : Bones) if (b && b->Name == name) return b.get();
    return nullptr;
}

void Skeleton::ConstructTree() {
    /** Construct tree **/
    for (auto& bone : Bones) {
        bone->Children.clear();
        for (auto& x : Bones) if (x->ParentName == bone->Name) bone->Children.push_back(x.get());
    }
    RootBone = nullptr;
    for (auto& x : Bones) if (x->ParentName == "NULL") { RootBone = x.get(); break; }
    if (RootBone != nullptr) ComputeBonePositions(RootBone, Matrix::Identity());
}

Skeleton* Skeleton::Clone() const {
    auto result = new Skeleton();
    result->Name = Name;
    result->Bones.reserve(Bones.size());
    for (size_t i = 0; i < Bones.size(); i++) result->Bones.emplace_back(Bones[i]->Clone());
    result->ConstructTree();
    return result;
}

void Skeleton::Read(const std::vector<uint8_t>& data, bool bcf) {
    // C# used the default IoBuffer order (big endian) for both; BCF data is little endian.
    BinaryReadProxy io(data, bcf ? st::Endian::Little : st::Endian::Big);
    Read(io, bcf);
}

/// TS1 BCF/CMX skeleton record: pstr Name, int32 boneCount (TSO: version + int16 count).
void Skeleton::Read(IOProxy& io, bool bcf) {
    if (!bcf) {
        auto version = io.ReadUInt32(); (void)version;
    }
    Name = io.ReadPascalString();

    int32_t boneCount = bcf ? io.ReadInt32() : io.ReadInt16();

    Bones.clear();
    for (int32_t i = 0; i < boneCount; i++) {
        Bone* bone = ReadBone(io, bcf);
        if (bone == nullptr) continue;
        bone->Index = (int32_t)Bones.size();
        Bones.emplace_back(bone);
    }
    ConstructTree();
}

/// <summary>
/// Reads a bone.
/// TSO: int32 unknown, pstr name, pstr parent, byte hasProps [int32 count x (int32 pairs x kv)], ...
/// TS1 BCF/CMX: pstr name, pstr parent, int32 propsCount x (int32 pairs x kv), then the same tail:
///   float3 translation, float4 rotation (x,y,z,w), int32 canTranslate, canRotate, canBlend,
///   float wiggleValue, float wigglePower.
/// Coordinates are converted to XNA handedness exactly like the C# (-x; quaternion x,-y,-z,-w).
/// </summary>
Bone* Skeleton::ReadBone(IOProxy& reader, bool bcf) {
    auto bone = new Bone();
    if (!bcf) bone->Unknown = reader.ReadInt32();
    bone->Name = reader.ReadPascalString();
    bone->ParentName = reader.ReadPascalString();
    int32_t propertyCount = 0;
    if (bcf) {
        propertyCount = reader.ReadInt32();
        bone->HasProps = (uint8_t)(propertyCount != 0);
    } else {
        bone->HasProps = reader.ReadByte();
        if (bone->HasProps != 0) propertyCount = reader.ReadInt32();
    }
    if (propertyCount != 0) {
        PropertyListItem property;
        for (int32_t i = 0; i < propertyCount; i++) {
            auto pairCount = reader.ReadInt32();
            for (int32_t x = 0; x < pairCount; x++) {
                auto key = reader.ReadPascalString();
                auto value = reader.ReadPascalString();
                property.KeyPairs.emplace_back(key, value);
            }
        }
        bone->Properties.push_back(property);
    }

    auto xx = -reader.ReadFloat();
    auto yy = reader.ReadFloat();
    auto zz = reader.ReadFloat();
    bone->Translation = Vector3(xx, yy, zz);
    auto qx = reader.ReadFloat();
    auto qy = -reader.ReadFloat();
    auto qz = -reader.ReadFloat();
    auto qw = -reader.ReadFloat();
    bone->Rotation = Quaternion(qx, qy, qz, qw);
    bone->CanTranslate = reader.ReadInt32();
    bone->CanRotate = reader.ReadInt32();
    bone->CanBlend = reader.ReadInt32();
    bone->WiggleValue = reader.ReadFloat();
    bone->WigglePower = reader.ReadFloat();
    if (bcf && bone->Name == "") { delete bone; return nullptr; }
    return bone;
}

void Skeleton::ComputeBonePositions(Bone* bone, const Matrix& world) {
    auto translateMatrix = Matrix::CreateTranslation(bone->Translation);
    auto rotationMatrix = Matrix::CreateFromQuaternion(bone->Rotation);

    auto myWorld = (rotationMatrix * translateMatrix) * world;
    bone->AbsolutePosition = Transform(Vector3(0, 0, 0), myWorld);
    bone->AbsoluteMatrix = myWorld;

    for (auto child : bone->Children) {
        ComputeBonePositions(child, myWorld);
    }
}

}}
