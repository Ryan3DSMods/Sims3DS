/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.Vitaboy.Avatar (SimsVille/Vitaboy/Avatar.cs)
#include "vitaboy/Avatar.h"
#include "vitaboy/Appearance.h"
#include "vitaboy/content/TS1AvatarProvider.h"
#include <algorithm>

namespace FSO { namespace Vitaboy {

using FSO::Content::TS1::TS1AvatarContent;

Avatar::Avatar(FSO::Vitaboy::Skeleton* skel) {
    if (skel != nullptr) {
        m_Skeleton.reset(skel->Clone());
        m_BaseSkeleton.reset(skel->Clone()); //keep a copy we can revert back to
    } else {
        // C# would throw on a null skeleton; keep an empty one so the VM can run without content.
        m_Skeleton.reset(new FSO::Vitaboy::Skeleton());
        m_BaseSkeleton.reset(new FSO::Vitaboy::Skeleton());
    }
    Skeleton = m_Skeleton.get();
    BaseSkeleton = m_BaseSkeleton.get();
}

Avatar::Avatar(const Avatar& old) {
    m_BaseSkeleton.reset(old.BaseSkeleton->Clone());
    m_Skeleton.reset(old.BaseSkeleton->Clone());
    Skeleton = m_Skeleton.get();
    BaseSkeleton = m_BaseSkeleton.get();
    for (size_t i = 0; i < old.Bindings.size(); i++) {
        auto oldb = old.Bindings[i];
        auto b = std::make_shared<AvatarBindingInstance>();
        b->Mesh = oldb->Mesh;
        b->Texture = oldb->Texture;
        Bindings.push_back(b);
    }
    Accessories = old.Accessories;
    //just shallow copy the binding and accessory list, as the data inside isn't going to change any time soon...
}

void Avatar::AddAccessory(Appearance* apr) {
    if (apr == nullptr) return;
    // C# wrapped this in `if (Accessories.Count > 0)`, so no accessory could ever be added
    // (FreeSims bug). Fixed here so TS1 suits/accessories (STR#304) show up.
    if (Accessories.count(apr)) {
        return;
    }
    auto add = AddAppearance(apr, std::string());
    Accessories[apr] = add;
}

void Avatar::RemoveAccessory(Appearance* apr) {
    auto it = Accessories.find(apr);
    if (Accessories.size() > 0 && it != Accessories.end()) {
        RemoveAppearance(it->second, true);
        Accessories.erase(it);
    }
}

std::shared_ptr<AvatarAppearanceInstance> Avatar::AddAppearance(Appearance* appearance, const std::string& texOverride) {
    auto result = std::make_shared<AvatarAppearanceInstance>();

    int i = 0;
    for (auto& bindingReference : appearance->Bindings) {
        Binding binding;
        if (bindingReference.HasRealBinding) binding = bindingReference.RealBinding;
        else {
            // STUB(3ds): Content.AvatarBindings.Get(TypeID, FileID) - TSO .bnd lookup, no TSO data.
            continue;
        }
        if ((i++ == 0) && !texOverride.empty()) {
            binding = binding.TS1Copy();
            binding.TextureName = texOverride;
            binding.HasTextureName = true;
        }
        auto inst = AddBinding(binding);
        if (inst) result->Bindings.push_back(inst);
    }

    return result;
}

void Avatar::RemoveAppearance(const std::shared_ptr<AvatarAppearanceInstance>& appearance, bool dispose) {
    if (!appearance) return;
    for (auto& binding : appearance->Bindings) {
        RemoveBinding(binding, dispose);
    }
}

std::shared_ptr<AvatarBindingInstance> Avatar::AddBinding(const Binding& binding) {
    auto content = TS1AvatarContent::Get();
    auto instance = std::make_shared<AvatarBindingInstance>();

    if (binding.HasMeshName && content->EnsureInit()) {
        instance->Mesh = content->AvatarMeshes.Get(binding.MeshName);
        if (instance->Mesh != nullptr) {
            const std::string& tex = binding.HasTextureName ? binding.TextureName : instance->Mesh->TextureName;
            instance->Texture = content->AvatarTextures.Get(tex);
        }
    } else {
        // STUB(3ds): content.AvatarMeshes.Get(MeshTypeID, MeshFileID) / AvatarTextures.Get(TextureTypeID,
        // TextureFileID) / TS1AvatarMeshes / TS1AvatarTextures by id - TSO content, not shipped with TS1.
    }

    // C# dereferenced a null mesh here (NullReferenceException); skip the binding instead.
    if (instance->Mesh == nullptr) return nullptr;

    if (Skeleton->RootBone != nullptr) instance->Mesh->Prepare(Skeleton->RootBone);
    Bindings.push_back(instance);
    return instance;
}

void Avatar::RemoveBinding(const std::shared_ptr<AvatarBindingInstance>& instance, bool /*dispose*/) {
    auto it = std::find(Bindings.begin(), Bindings.end(), instance);
    if (it != Bindings.end()) Bindings.erase(it);
}

void Avatar::ReloadSkeleton() {
    if (Skeleton->RootBone != nullptr) Skeleton->ComputeBonePositions(Skeleton->RootBone, Matrix::Identity());
    m_SkelBones.resize(Skeleton->Bones.size());
    for (size_t i = 0; i < Skeleton->Bones.size(); i++) {
        m_SkelBones[i] = Skeleton->Bones[i]->AbsoluteMatrix;
    }
}

void Avatar::ResetPose() {
    for (size_t i = 0; i < Skeleton->Bones.size() && i < BaseSkeleton->Bones.size(); i++) {
        Skeleton->Bones[i]->Translation = BaseSkeleton->Bones[i]->Translation;
        Skeleton->Bones[i]->Rotation = BaseSkeleton->Bones[i]->Rotation;
    }
    ReloadSkeleton();
}

}}
