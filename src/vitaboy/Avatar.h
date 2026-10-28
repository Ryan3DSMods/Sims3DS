/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.Vitaboy.Avatar / AvatarAppearanceInstance / AvatarBindingInstance (SimsVille/Vitaboy/Avatar.cs)
#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <vector>
#include "vitaboy/Skeleton.h"
#include "vitaboy/Mesh.h"
#include "vitaboy/Texture.h"
#include "vitaboy/Binding.h"

namespace FSO { namespace Vitaboy {

class Appearance;

/// <summary>
/// Holds a mesh and texture for an avatar.
/// </summary>
class AvatarBindingInstance {
public:
    FSO::Vitaboy::Mesh* Mesh = nullptr;          // owned by the content cache
    FSO::Vitaboy::Texture* Texture = nullptr;    // owned by the content cache (C#: ITextureRef)
};

/// <summary>
/// Holds bindings for an avatar.
/// </summary>
class AvatarAppearanceInstance {
public:
    std::vector<std::shared_ptr<AvatarBindingInstance>> Bindings;
};

/// <summary>
/// The base class for all avatars in the game.
/// (C#: abstract, derives _3DComponent - the MonoGame scene graph part is dropped; drawing is done
/// by AvatarRenderer::DrawAvatar, see AvatarRenderer.h.)
/// </summary>
class Avatar {
public:
    std::vector<std::shared_ptr<AvatarBindingInstance>> Bindings;
    // STUB(3ds): static Effect Effect / setVitaboyEffect (the HLSL Vitaboy.fx shader). The software
    // renderer implements vsVitaboy/psVitaboy in AvatarRenderer.cpp.

    /// Skeleton { get; internal set; } - the animated copy (owned by this avatar).
    FSO::Vitaboy::Skeleton* Skeleton = nullptr;
    /// BaseSkeleton - an unanimated copy we can revert back to (owned by this avatar).
    FSO::Vitaboy::Skeleton* BaseSkeleton = nullptr;

    /// <summary>
    /// Creates a new Avatar instance (clones the skeleton; null gives an empty skeleton).
    /// </summary>
    explicit Avatar(FSO::Vitaboy::Skeleton* skel);
    Avatar(const Avatar& old);
    virtual ~Avatar() {}
    Avatar& operator=(const Avatar&) = delete;

    /// <summary>
    /// Adds an accessory to this avatar.
    /// </summary>
    void AddAccessory(Appearance* apr);
    /// <summary>
    /// Remove an accessory from this avatar.
    /// </summary>
    void RemoveAccessory(Appearance* apr);

    /// <summary>
    /// Removes an Appearance instance from this avatar.
    /// </summary>
    void RemoveAppearance(const std::shared_ptr<AvatarAppearanceInstance>& appearance, bool dispose);

    // STUB(3ds): StoreOnGPU(GraphicsDevice) uploaded every binding's mesh/texture.
    void StoreOnGPU(void* /*device*/) {}

    /// <summary>
    /// When the skeleton changes (for example due to an animation) this
    /// method will recompute the meshes to adhere to the new skeleton positions.
    /// </summary>
    void ReloadSkeleton();

    /// Absolute bone matrices (C# SkelBones, the shader's SkelBindings[]), refreshed by ReloadSkeleton.
    const std::vector<Matrix>& SkelBones() { if (m_SkelBones.empty()) ReloadSkeleton(); return m_SkelBones; }

    /// Reverts the animated skeleton to the bind pose (BaseSkeleton). 3DS helper used by tools.
    void ResetPose();

protected:
    /// <summary>
    /// Adds an Appearance instance to this avatar. texOverride replaces the texture of the first binding (TS1).
    /// </summary>
    std::shared_ptr<AvatarAppearanceInstance> AddAppearance(Appearance* appearance, const std::string& texOverride);
    /// <summary>
    /// Adds a Binding instance to this avatar.
    /// </summary>
    std::shared_ptr<AvatarBindingInstance> AddBinding(const Binding& binding);
    /// <summary>
    /// Removes a Binding instance from this avatar.
    /// </summary>
    void RemoveBinding(const std::shared_ptr<AvatarBindingInstance>& instance, bool dispose);

    std::unique_ptr<FSO::Vitaboy::Skeleton> m_Skeleton;
    std::unique_ptr<FSO::Vitaboy::Skeleton> m_BaseSkeleton;
    std::vector<Matrix> m_SkelBones;
    std::map<Appearance*, std::shared_ptr<AvatarAppearanceInstance>> Accessories;
};

}}
