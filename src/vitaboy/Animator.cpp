/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.Vitaboy.Animator (SimsVille/Vitaboy/Animator.cs)
#include "vitaboy/Animator.h"
#include <algorithm>

namespace FSO { namespace Vitaboy {

AnimationHandle* Animator::RunAnimation(Avatar* avatar, Animation* animation) {
    std::unique_ptr<AnimationHandle> instance(new AnimationHandle(this));
    instance->Animation = animation;
    instance->Avatar = avatar;
    auto result = instance.get();
    Animations.push_back(std::move(instance));
    return result;
}

void Animator::DisposeAnimation(AnimationHandle* animation) {
    for (auto it = Animations.begin(); it != Animations.end(); ++it) {
        if (it->get() == animation) { Animations.erase(it); return; }
    }
}

void Animator::Update(int64_t timeMs) {
    for (size_t i = 0; i < Animations.size(); i++) {
        auto item = Animations[i].get();
        item->Update(timeMs);
        if (item->Status == AnimationStatus::COMPLETED) {
            Animations.erase(Animations.begin() + i);
            i--;
        }
    }
}

AnimationStatus Animator::RenderFrame(Avatar* avatar, Animation* animation, int32_t frame, float fraction, float weight) {
    if (animation == nullptr) return AnimationStatus::COMPLETED;
    int32_t numFrames = animation->NumFrames();
    bool completed = (frame < 0 || frame > numFrames);
    frame = std::max(std::min(frame, numFrames), (int32_t)0);

    // 3DS: keyframes are loaded on first use (see Animation::EnsureKeyframes). Without them (or
    // without a skeleton) only the completion status is computed, which is all the VM needs.
    if (avatar != nullptr && avatar->Skeleton != nullptr && avatar->Skeleton->Bones.size() > 0 && animation->EnsureKeyframes()) {
        auto& T = animation->Translations;
        auto& R = animation->Rotations;
        for (auto& motion : animation->Motions) {
            auto bone = avatar->Skeleton->GetBone(motion.BoneName);
            if (bone == nullptr) continue; //fixes bugs with missing bones.. need to find out what R_FINGERPOLY0 is though.
            if (motion.FrameCount == 0) continue;

            auto motionFrame = frame;
            if (frame >= (int32_t)motion.FrameCount) {
                motionFrame = (int32_t)motion.FrameCount - 1;
            }

            if (motion.HasTranslation) {
                size_t i1 = (size_t)motion.FirstTranslationIndex + motionFrame;
                if (i1 < T.size()) {
                    Vector3 trans;
                    if (fraction >= 0) {
                        auto trans1 = T[i1];
                        auto trans2 = (frame + 1 >= (int32_t)motion.FrameCount || i1 + 1 >= T.size()) ? trans1 : T[i1 + 1];
                        trans = Vector3::Lerp(trans1, trans2, fraction);
                    } else {
                        trans = T[i1];
                    }
                    if (weight == 1) bone->Translation = trans;
                    else bone->Translation = Vector3::Lerp(bone->Translation, trans, weight);
                }
            }
            if (motion.HasRotation) {
                size_t i1 = (size_t)motion.FirstRotationIndex + motionFrame;
                if (i1 < R.size()) {
                    Quaternion quat;
                    if (fraction >= 0) {
                        auto quat1 = R[i1];
                        auto quat2 = (frame + 1 >= (int32_t)motion.FrameCount || i1 + 1 >= R.size()) ? quat1 : R[i1 + 1];
                        quat = Quaternion::Slerp(quat1, quat2, fraction);
                    } else {
                        quat = R[i1];
                    }
                    if (weight == 1) bone->Rotation = quat;
                    else bone->Rotation = Quaternion::Slerp(bone->Rotation, quat, weight);
                }
            }
        }
    }

    if (completed || frame + 1 > numFrames) return AnimationStatus::COMPLETED;
    return AnimationStatus::IN_PROGRESS;
}

void AnimationHandle::Dispose() {
    Owner->DisposeAnimation(this);
}

void AnimationHandle::Update(int64_t now) {
    if (this->Status == AnimationStatus::WAITING_TO_START) {
        StartTime = now;
        this->Status = AnimationStatus::IN_PROGRESS;
    }
    if (Animation == nullptr || Avatar == nullptr) { Status = AnimationStatus::COMPLETED; return; }
    Animation->EnsureKeyframes();

    auto fps = 30.0f * Speed;
    auto fpms = fps / 1000;

    auto runTime = now - StartTime;
    uint32_t frame = (uint32_t)(runTime * fpms);

    size_t numDone = 0;

    /** Speed is 30fps by default **/
    for (auto& motion : Animation->Motions) {
        auto bone = Avatar->Skeleton->GetBone(motion.BoneName);
        auto motionFrame = frame;
        if (frame >= motion.FrameCount) {
            numDone++;
            motionFrame = motion.FrameCount - 1;
        }
        if (bone == nullptr || motion.FrameCount == 0) continue;

        if (motion.HasTranslation && motion.FirstTranslationIndex + motionFrame < Animation->Translations.size()) {
            bone->Translation = Animation->Translations[motion.FirstTranslationIndex + motionFrame];
        }
        if (motion.HasRotation && motion.FirstRotationIndex + motionFrame < Animation->Rotations.size()) {
            bone->Rotation = Animation->Rotations[motion.FirstRotationIndex + motionFrame];
        }
    }

    if (numDone == Animation->Motions.size()) {
        /** Completed! **/
        this->Status = AnimationStatus::COMPLETED;
    } else {
        Avatar->ReloadSkeleton();
    }
}

}}
