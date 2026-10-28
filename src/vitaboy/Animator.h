/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.Vitaboy.Animator / AnimationHandle / AnimationStatus (SimsVille/Vitaboy/Animator.cs)
#pragma once
#include <cstdint>
#include <memory>
#include <vector>
#include "vitaboy/Animation.h"
#include "vitaboy/Avatar.h"

namespace FSO { namespace Vitaboy {

enum class AnimationStatus {
    WAITING_TO_START,
    IN_PROGRESS,
    COMPLETED,
    STOPPED
};

class Animator;

/// <summary>
/// Handle to an animation.
/// </summary>
class AnimationHandle {
public:
    FSO::Vitaboy::Animation* Animation = nullptr;
    double Speed = 1.0;
    FSO::Vitaboy::Avatar* Avatar = nullptr;
    int64_t StartTime = 0;
    AnimationStatus Status = AnimationStatus::WAITING_TO_START;

    explicit AnimationHandle(Animator* animator) : Owner(animator) {}
    /// <summary>
    /// Disposes this animation handle.
    /// </summary>
    void Dispose();
    /// C# Update(GameTime): `now` is the game time in milliseconds.
    void Update(int64_t now);
private:
    Animator* Owner;
};

/// <summary>
/// An animator is used to animate an avatar.
/// </summary>
class Animator {
public:
    /// <summary>
    /// Runs an animation.
    /// </summary>
    AnimationHandle* RunAnimation(Avatar* avatar, Animation* animation);
    /// <summary>
    /// Disposes an animation.
    /// </summary>
    void DisposeAnimation(AnimationHandle* animation);
    void Update(int64_t timeMs);

    /// <summary>
    /// Renders an animation's frame: poses the avatar's skeleton (bone Translation/Rotation;
    /// call Avatar::ReloadSkeleton afterwards to recompute absolute matrices).
    /// </summary>
    /// <param name="frame">Frame number in animation.</param>
    /// <param name="fraction">Interpolation towards the next frame (negative: none).</param>
    /// <param name="weight">The amount to apply this animation to the skeleton.</param>
    /// <returns>Status of animation.</returns>
    static AnimationStatus RenderFrame(Avatar* avatar, Animation* animation, int32_t frame, float fraction, float weight);

protected:
    std::vector<std::unique_ptr<AnimationHandle>> Animations;
};

}}
