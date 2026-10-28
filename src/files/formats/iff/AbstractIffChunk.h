/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/. 
 */
// Translated from FreeSims sims.files/formats/iff/AbstractIffChunk.cs
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "core/io.h"
#include "core/math.h"
#include "files/utils/IoBuffer.h"
#include "files/formats/iff/ChunkRuntimeInfo.h"

/// Bitwise operators for a C# [Flags] enum translated to an enum class. `a & b` yields the
/// underlying integer (so `(Flags & F::X) > 0` reads as in C#), `|` and `~` yield the enum.
#define FSO_FLAGS_ENUM(T) \
    inline T operator|(T a, T b) { return (T)((int64_t)a | (int64_t)b); } \
    inline T& operator|=(T& a, T b) { a = a | b; return a; } \
    inline int64_t operator&(T a, T b) { return (int64_t)a & (int64_t)b; } \
    inline T& operator&=(T& a, T b) { a = (T)((int64_t)a & (int64_t)b); return a; } \
    inline T operator~(T a) { return (T)(~(int64_t)a); } \
    inline bool operator>(T a, int b) { return (int64_t)a > b; } \
    inline bool operator==(T a, int b) { return (int64_t)a == b; } \
    inline bool operator!=(T a, int b) { return (int64_t)a != b; }

namespace FSO { namespace Files { namespace Formats { namespace IFF {

class IffFile;

/// Declares the 4-character chunk type key of a chunk class. The key stands in for
/// System.Type in the IffFile lookups (Get<T>/List<T> use T::TYPE).
#define IFF_CHUNK_TYPE(str) \
    static constexpr const char* TYPE = str; \
    const char* GetTypeKey() const override { return TYPE; }

/// <summary>
/// An IFF is made up of chunks.
/// </summary>
class IffChunk
{
public:
    uint16_t ChunkID = 0;
    uint16_t ChunkFlags = 0;
    std::string ChunkType; //just making it easier to access
    std::string ChunkLabel;
    bool ChunkProcessed = false;
    std::vector<uint8_t> OriginalData; //IDE ONLY: always contains base data for any chunk.
    bool HasOriginalData = false; //C#: OriginalData != null
    uint16_t OriginalID = 0;
    bool AddedByPatch = false;
    std::string OriginalLabel;
    std::vector<uint8_t> ChunkData;
    IffFile* ChunkParent = nullptr;
    // TS1(sims): a chunk left on disk by IffFile::ReadFromFile (sprites, bitmaps): its 76-byte header + data
    // start at DeferredOffset of ChunkParent->SourcePath (at most DeferredSize bytes); loaded on first prepare().
    int64_t DeferredOffset = -1;
    uint32_t DeferredSize = 0;
    int64_t SourceOffset = -1;   // where a deferred chunk lives (kept after loading; SPR2 frames of such chunks keep
                                 // their compressed bytes, see SPR2Frame::DropDecoded)

    ChunkRuntimeState RuntimeInfo = ChunkRuntimeState::Normal;

    virtual ~IffChunk() {}

    /// C# GetType() stand-in: the registry key of this chunk's class.
    virtual const char* GetTypeKey() const = 0;

    /// <summary>
    /// Reads this chunk from an IFF.
    /// </summary>
    /// <param name="iff">The IFF to read from.</param>
    /// <param name="stream">The stream to read from.</param>
    virtual void Read(IffFile* iff, st::Reader& stream) = 0;

    /// <summary>
    /// The name of this chunk.
    /// </summary>
    /// <returns>The name of this chunk as a string.</returns>
    virtual std::string ToString() const
    {
        return "#" + std::to_string(ChunkID) + " " + ChunkLabel;
    }

    /// <summary>
    /// Attempts to write this chunk to a stream (presumably an IFF or PIFF)
    /// </summary>
    /// <returns>True if data has been written, false if not. </returns>
    virtual bool Write(IffFile* iff, st::Writer& stream) { (void)iff; (void)stream; return false; }

    virtual void Dispose() { }
};

namespace Chunks {
    // XNA value types used throughout the chunk classes.
    using st::Color;
    using st::Vector2;
    using st::Vector3;
    using st::Point;
    using st::Rectangle;
    using Utils::ByteOrder;
    using Utils::SeekOrigin;
}

}}}} // namespace FSO::Files::Formats::IFF
