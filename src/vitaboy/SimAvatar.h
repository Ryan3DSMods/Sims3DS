/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.Vitaboy.SimAvatar / SimHandGesture (SimsVille/Vitaboy/SimAvatar.cs)
#pragma once
#include <cstdint>
#include <memory>
#include "vitaboy/Avatar.h"
#include "vitaboy/Outfit.h"

namespace FSO { namespace Vitaboy {

enum class SimHandGesture : int {
    Idle = 0,
    Fist = 2,
    Pointing = 1,
    None = 3 //some animations remove the hands completely, eg. for puppets
};

/// C# property with a setter that reloads part of the avatar: `avatar->Body = outfit;` stores the
/// value and calls the owner's reload method, reads convert implicitly to T (as in C#).
template <class Owner, class T, void (Owner::*OnSet)()>
class AvatarProperty {
public:
    explicit AvatarProperty(Owner* owner, T value = T()) : m_Owner(owner), m_Value(value) {}
    AvatarProperty& operator=(const T& v) { m_Value = v; (m_Owner->*OnSet)(); return *this; }
    AvatarProperty& operator=(const AvatarProperty& o) { return *this = (T)o.m_Value; }
    operator T() const { return m_Value; }
    T Value() const { return m_Value; }
    T operator->() const { return m_Value; }
    /// sets without reloading (used by constructors)
    void SetSilent(const T& v) { m_Value = v; }
private:
    Owner* m_Owner;
    T m_Value;
};

/// <summary>
/// Represents all sims in the game.
/// </summary>
class SimAvatar : public Avatar {
public:
    /// <summary>
    /// Creates a new instance of SimAvatar.
    /// </summary>
    explicit SimAvatar(FSO::Vitaboy::Skeleton* skel = nullptr);
    SimAvatar(const SimAvatar& old);

    /// <summary>
    /// Helper method to remove all body parts from an avatar save for the head.
    /// Used by pie menus.
    /// </summary>
    void StripAllButHead();

    void ReloadHandgroup();
    void ReloadHead();
    void ReloadBody();
    void ReloadAll() { ReloadHead(); ReloadBody(); ReloadHandgroup(); }

    /// <summary>
    /// Gets or sets the handgroup of this SimAvatar. Handgroups use the same outfit as bodies!
    /// </summary>
    AvatarProperty<SimAvatar, Outfit*, &SimAvatar::ReloadHandgroup> Handgroup{this, nullptr};
    /// <summary>
    /// Left/right hand gesture: Idle, fist or pointing! Usually set by animations, defaults to idle.
    /// </summary>
    AvatarProperty<SimAvatar, SimHandGesture, &SimAvatar::ReloadHandgroup> LeftHandGesture{this, SimHandGesture::Idle};
    AvatarProperty<SimAvatar, SimHandGesture, &SimAvatar::ReloadHandgroup> RightHandGesture{this, SimHandGesture::Idle};
    /// <summary>
    /// Gets or sets the head of this SimAvatar.
    /// </summary>
    AvatarProperty<SimAvatar, Outfit*, &SimAvatar::ReloadHead> Head{this, nullptr};
    /// <summary>
    /// Gets of sets the body of this SimAvatar.
    /// </summary>
    AvatarProperty<SimAvatar, Outfit*, &SimAvatar::ReloadBody> Body{this, nullptr};
    /// <summary>
    /// Gets or sets the ApperanceType of this SimAvatar. (C#: only reloads the head.)
    /// </summary>
    AvatarProperty<SimAvatar, AppearanceType, &SimAvatar::ReloadHead> Appearance{this, AppearanceType::Light};

    /// <summary>
    /// Graphics device was reset. (Reloads everything.)
    /// </summary>
    void DeviceReset(void* /*device*/) { ReloadSkeleton(); ReloadHead(); ReloadBody(); ReloadHandgroup(); }

private:
    std::shared_ptr<AvatarAppearanceInstance> m_LeftHandInstance;
    std::shared_ptr<AvatarAppearanceInstance> m_RightHandInstance;
    std::shared_ptr<AvatarAppearanceInstance> m_HeadInstance;
    std::shared_ptr<AvatarAppearanceInstance> m_BodyInstance;
};

/// <summary>
/// Wrapper class for SimAvatar with a default skeleton, "adult.skel". (AdultVitaboyModel.cs)
/// </summary>
class AdultVitaboyModel : public SimAvatar {
public:
    AdultVitaboyModel();
    AdultVitaboyModel(const AdultVitaboyModel& old) : SimAvatar(old) {}
};

}}
