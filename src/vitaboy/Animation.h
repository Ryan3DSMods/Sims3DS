/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.Vitaboy.Animation (sims.common/rendering/vitaboy/Animation.cs)
// Also: AnimationMotion, PropertyList, TimePropertyList, TimePropertyListItem.
// PropertyListItem lives here too (Bone.cs in C#) so the VM can include just this header.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <utility>
#include <algorithm>
#include "vitaboy/VitaboyMath.h"

namespace FSO { namespace Vitaboy {

class IOProxy;

/// <summary>
/// An item in a skeleton's Property List.
/// </summary>
class PropertyListItem {
public:
    std::vector<std::pair<std::string, std::string>> KeyPairs;
};

/// <summary>
/// Lists properties associated with a motion.
/// </summary>
class PropertyList {
public:
    std::vector<PropertyListItem> Items;

    /// C# indexer this[string key]: returns null (nullptr) when the key is missing.
    const std::string* Get(const std::string& key) const {
        for (auto& item : Items)
            for (auto& keypair : item.KeyPairs)
                if (keypair.first == key) return &keypair.second;
        return nullptr;
    }
};

/// <summary>
/// Stores PropertyList instances and associates them with an ID.
/// For TS1 data the ID is the event time in milliseconds (VMAvatar compares it to frame*33.33).
/// </summary>
class TimePropertyListItem {
public:
    uint32_t ID = 0;
    PropertyList Properties;
};

/// <summary>
/// Lists property lists associated with a motion.
/// </summary>
class TimePropertyList {
public:
    std::vector<TimePropertyListItem> Items;
};

/// <summary>
/// An animation consists of a number of motions that each move a bone.
/// </summary>
class AnimationMotion {
public:
    std::string BoneName;
    uint32_t FrameCount = 0;
    float Duration = 0;
    bool HasTranslation = false;
    bool HasRotation = false;
    uint32_t FirstTranslationIndex = 0;
    uint32_t FirstRotationIndex = 0;

    std::vector<PropertyList> Properties;
    std::vector<TimePropertyList> TimeProperties;
};

/// <summary>
/// Represents an animation for a model.
/// </summary>
class Animation {
public:
    Animation() = default;
    Animation(const Animation&) = delete;
    Animation& operator=(const Animation&) = delete;
    ~Animation();
    std::string Name;
    std::string XSkillName;
    float Duration = 0;
    float Distance = 0;
    uint8_t IsMoving = 0;
    std::vector<Vector3> Translations;
    std::vector<Quaternion> Rotations;
    std::vector<AnimationMotion> Motions;

    uint32_t TranslationCount = 0;
    uint32_t RotationCount = 0;

    /// <summary>
    /// Total number of frames in this animation.
    /// </summary>
    int32_t NumFrames() const {
        int32_t result = 0;
        for (const auto& motion : Motions) result = std::max(result, (int32_t)motion.FrameCount);
        return result;
    }

    /// <summary>
    /// Reads an animation from a stream (TSO .anim: big endian, bcf=false) or from a BCF/CMX
    /// skill record (bcf=true). See Animation.cpp for the format notes.
    /// </summary>
    void Read(const std::vector<uint8_t>& data, bool bcf);
    void Read(IOProxy& io, bool bcf);

    // ---- 3DS addition: lazy keyframes ----
    // TS1 skill records (BCF/CMX) carry only the counts; the vectors live in xskill-*.cfp and are
    // decoded by CFP::EnrichAnim. To keep memory low the VM can hold thousands of animation
    // headers while only animations that are actually posed load their keyframes.
    bool KeyframesLoaded() const { return Translations.size() == TranslationCount && Rotations.size() == RotationCount; }
    /// Loads the CFP keyframes on demand (no-op when loaded or when no loader is registered).
    bool EnsureKeyframes();
    /// Frees the keyframes again (can be reloaded later by EnsureKeyframes).
    void UnloadKeyframes() { std::vector<Vector3>().swap(Translations); std::vector<Quaternion>().swap(Rotations); }
    /// Registered by the TS1 content provider: fills Translations/Rotations from the CFP.
    static bool (*KeyframeLoader)(Animation* anim);
    /// FIX(sims): the loaded keyframes are kept within this budget (least recently posed are unloaded first and
    /// reload from the CFP when posed again). Before, every animation ever posed stayed loaded: +20 MB of heap per
    /// game day on a busy lot. 0 = unlimited.
    static size_t KeyframeBudgetBytes;
    static size_t KeyframeBytesLoaded();
    /// FIX(sims) memory: unloads every animation's keyframes (leaving a lot: no avatar is posed any more).
    static size_t UnloadAllKeyframes();
    uint32_t LastPosed = 0;   // LRU stamp (EnsureKeyframes)
    size_t LoadedBytes = 0;   // accounted size of the loaded keyframes

private:
    PropertyList ReadPropertyList(IOProxy& io, bool bcf);
};

}}
