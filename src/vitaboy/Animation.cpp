/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.Vitaboy.Animation (sims.common/rendering/vitaboy/Animation.cs)
#include "vitaboy/Animation.h"
#include "vitaboy/BCFReadProxy.h"
#include <cstdint>
#include <vector>

namespace FSO { namespace Vitaboy {

bool (*Animation::KeyframeLoader)(Animation* anim) = nullptr;

// FIX(sims): LRU budget for the lazily loaded keyframes (see Animation.h)
size_t Animation::KeyframeBudgetBytes = 6u * 1024u * 1024u;
static std::vector<Animation*>& LoadedAnims() { static std::vector<Animation*> v; return v; }
static size_t g_KeyframeBytes = 0;
static uint32_t g_PoseClock = 0;
size_t Animation::KeyframeBytesLoaded() { return g_KeyframeBytes; }
size_t Animation::UnloadAllKeyframes() { // FIX(sims): see Animation.h
    size_t freed = g_KeyframeBytes;
    for (auto* a : LoadedAnims()) { a->LoadedBytes = 0; a->UnloadKeyframes(); }
    LoadedAnims().clear();
    g_KeyframeBytes = 0;
    return freed;
}

Animation::~Animation() {
    if (LoadedBytes == 0) return;
    auto& v = LoadedAnims();
    for (size_t i = 0; i < v.size(); i++) if (v[i] == this) { v[i] = v.back(); v.pop_back(); break; }
    g_KeyframeBytes -= LoadedBytes;
}

bool Animation::EnsureKeyframes() {
    LastPosed = ++g_PoseClock;
    if (KeyframesLoaded()) return true;
    if (KeyframeLoader == nullptr) return false;
    if (!KeyframeLoader(this)) return false;
    size_t bytes = Translations.capacity() * sizeof(Vector3) + Rotations.capacity() * sizeof(Quaternion);
    if (bytes == 0) return true;
    auto& v = LoadedAnims();
    if (LoadedBytes == 0) v.push_back(this); else g_KeyframeBytes -= LoadedBytes;
    LoadedBytes = bytes;
    g_KeyframeBytes += bytes;
    while (KeyframeBudgetBytes > 0 && g_KeyframeBytes > KeyframeBudgetBytes && v.size() > 1) {
        size_t oldest = SIZE_MAX;
        for (size_t i = 0; i < v.size(); i++)
            if (v[i] != this && (oldest == SIZE_MAX || v[i]->LastPosed < v[oldest]->LastPosed)) oldest = i;
        if (oldest == SIZE_MAX) break;
        Animation* a = v[oldest];
        // only animations not posed in this pass of the animator (every playing animation is re-posed each tick)
        if (g_PoseClock - a->LastPosed < 64) break;
        g_KeyframeBytes -= a->LoadedBytes;
        a->LoadedBytes = 0;
        a->UnloadKeyframes();
        v[oldest] = v.back(); v.pop_back();
    }
    return true;
}

void Animation::Read(const std::vector<uint8_t>& data, bool bcf) {
    BinaryReadProxy io(data, bcf ? st::Endian::Little : st::Endian::Big);
    Read(io, bcf);
}

/// <summary>
/// Reads an animation from a stream.
/// TSO .anim (bcf=false) is the C# code verbatim.
/// TS1 BCF/CMX skill (bcf=true) layout, verified against every skill in Animation.far
/// (the FreeSims bcf branch was unfinished: it read a version and byte flags that do not exist):
///   pstr Name, pstr XSkillName (cfp file name), float Duration(ms), float Distance,
///   int32 IsMoving, uint32 TranslationCount, uint32 RotationCount (keyframes live in the CFP),
///   uint32 MotionCount, then per motion:
///     pstr BoneName, uint32 FrameCount, float Duration, int32 HasTranslation, int32 HasRotation,
///     uint32 FirstTranslationIndex, uint32 FirstRotationIndex,
///     uint32 PropsListCount  x PropertyList,
///     uint32 TimePropsListCount x { uint32 count x { uint32 time(ms), PropertyList } }
///   where PropertyList = uint32 pairCount x (pstr key, pstr value).
/// </summary>
void Animation::Read(IOProxy& io, bool bcf) {
    if (bcf) {
        Name = io.ReadPascalString();
        XSkillName = io.ReadPascalString();
    } else {
        auto version = io.ReadUInt32(); (void)version;
        Name = io.ReadLongPascalString();
    }

    Duration = io.ReadFloat();
    Distance = io.ReadFloat();
    IsMoving = bcf ? (uint8_t)io.ReadInt32() : io.ReadByte();

    TranslationCount = io.ReadUInt32();
    if (!bcf) {
        Translations.resize(TranslationCount);
        for (uint32_t i = 0; i < TranslationCount; i++) {
            Vector3 v;
            v.X = -io.ReadFloat();
            v.Y = io.ReadFloat();
            v.Z = io.ReadFloat();
            Translations[i] = v;
        }
    }

    RotationCount = io.ReadUInt32();
    if (!bcf) {
        Rotations.resize(RotationCount);
        for (uint32_t i = 0; i < RotationCount; i++) {
            Quaternion q;
            q.X = io.ReadFloat();
            q.Y = -io.ReadFloat();
            q.Z = -io.ReadFloat();
            q.W = -io.ReadFloat();
            Rotations[i] = q;
        }
    }

    auto motionCount = io.ReadUInt32();
    Motions.resize(motionCount);
    for (uint32_t i = 0; i < motionCount; i++) {
        AnimationMotion motion;
        if (!bcf) { auto unknown = io.ReadUInt32(); (void)unknown; }
        motion.BoneName = io.ReadPascalString();
        motion.FrameCount = io.ReadUInt32();
        motion.Duration = io.ReadFloat();
        if (bcf) {
            motion.HasTranslation = (io.ReadInt32() == 1);
            motion.HasRotation = (io.ReadInt32() == 1);
        } else {
            motion.HasTranslation = (io.ReadByte() == 1);
            motion.HasRotation = (io.ReadByte() == 1);
        }
        motion.FirstTranslationIndex = io.ReadUInt32();
        motion.FirstRotationIndex = io.ReadUInt32();

        // TSO: byte flag then count. TS1: the count only.
        uint32_t propListCount = 0;
        if (bcf) propListCount = io.ReadUInt32();
        else if (io.ReadByte() == 1) propListCount = io.ReadUInt32();
        for (uint32_t x = 0; x < propListCount; x++) {
            motion.Properties.push_back(ReadPropertyList(io, bcf));
        }

        uint32_t timePropsListCount = 0;
        if (bcf) timePropsListCount = io.ReadUInt32();
        else if (io.ReadByte() == 1) timePropsListCount = io.ReadUInt32();
        for (uint32_t x = 0; x < timePropsListCount; x++) {
            TimePropertyList list;
            auto timePropsCount = io.ReadUInt32();
            list.Items.resize(timePropsCount);
            for (uint32_t y = 0; y < timePropsCount; y++) {
                auto id = io.ReadUInt32();
                list.Items[y].ID = id;
                list.Items[y].Properties = ReadPropertyList(io, bcf);
            }
            motion.TimeProperties.push_back(std::move(list));
        }

        Motions[i] = std::move(motion);
    }
}

/// <summary>
/// Reads a property list from a stream.
/// TSO: uint32 itemCount x (uint32 pairCount x pairs). TS1: a single item (uint32 pairCount x pairs).
/// </summary>
PropertyList Animation::ReadPropertyList(IOProxy& io, bool bcf) {
    auto propsCount = bcf ? 1u : io.ReadUInt32();
    PropertyList result;
    result.Items.resize(propsCount);

    for (uint32_t y = 0; y < propsCount; y++) {
        PropertyListItem item;
        auto pairsCount = io.ReadUInt32();
        for (uint32_t z = 0; z < pairsCount; z++) {
            auto key = io.ReadPascalString();
            auto value = io.ReadPascalString();
            item.KeyPairs.emplace_back(key, value);
        }
        result.Items[y] = std::move(item);
    }

    return result;
}

}}
