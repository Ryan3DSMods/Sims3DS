/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/formats/iff/IffFile.cs
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include "core/io.h"
#include "files/utils/IFileInfoUtilizer.h"
#include "files/formats/iff/AbstractIffChunk.h"
#include "files/formats/iff/IffRuntimeInfo.h"

namespace FSO { namespace Files { namespace Formats { namespace IFF {

namespace Chunks { class PIFF; }

/// <summary>
/// Interchange File Format (IFF) is a chunk-based file format for binary resource data
/// intended to promote a common model for store and use by an executable.
/// </summary>
class IffFile : public Utils::IFileInfoUtilizer
{
public:
    /// <summary>
    /// Set to true to force the game to retain a copy of all chunk data at time of loading (used to generate piffs)
    /// Should really only be set when the user wants to use the IDE, as it uses a lot more memory.
    /// </summary>
    static bool RETAIN_CHUNK_DATA;

    std::string Filename;

    typedef IffChunk* (*ChunkFactory)();
    /// Maps the 4-char chunk type string to a factory for its chunk class
    /// (C#: Dictionary<string, Type> CHUNK_TYPES).
    static const std::unordered_map<std::string, ChunkFactory>& CHUNK_TYPES();

    IffRuntimeInfo RuntimeInfo;
private:
    // C# keys these by System.Type. Here the key is the chunk class's TYPE string
    // (IffChunk::GetTypeKey / T::TYPE), which is unique per registered class.
    std::unordered_map<std::string, std::unordered_map<uint16_t, IffChunk*>> ByChunkId;
    std::unordered_map<std::string, std::vector<IffChunk*>> ByChunkType;
    std::vector<std::string> ByChunkTypeOrder; //insertion order of ByChunkType keys (C# Dictionary enumeration order)
    std::vector<std::unique_ptr<IffChunk>> OwnedChunks; //chunks created by this file (ownership; chunks may be referenced by other files via Patch)
public:
    std::vector<IffChunk*> RemovedOriginal;
    uint32_t ExecutableHash = 0; //hash of BHAV and BCON chunks

    /// <summary>
    /// Constructs a new IFF instance.
    /// </summary>
    IffFile();

    /// <summary>
    /// Constructs an IFF instance from a filepath.
    /// </summary>
    /// <param name="filepath">Path to the IFF.</param>
    explicit IffFile(const std::string& filepath);

    ~IffFile() override;

    /// <summary>
    /// Reads an IFF from a stream.
    /// </summary>
    /// <param name="stream">The stream to read from.</param>
    void Read(st::Reader& stream);
    /// Convenience: read from an in-memory copy of the file (e.g. a FAR archive member).
    void Read(const std::vector<uint8_t>& data);

    void Write(st::Writer& stream);

    /// TS1(sims): reads the iff stored at [base, base + length) of a file (a FAR member or a loose file) through
    /// the shared FilePool, using the resource map (rsmp) to read only the chunks it needs: with deferHeavy the sprite
    /// and bitmap chunks (SPR2, SPR#, BMP_: ~85% of an object file) stay on disk until first used. Falls back to a
    /// full read when the file has no usable rsmp. Throws on read errors like Read().
    /// onlyType: read just the chunks of this type (e.g. "OBJD" for the object index; nothing else is registered).
    void ReadFromFile(const std::string& path, int64_t base, uint32_t length, bool deferHeavy, const char* onlyType = nullptr);
    /// TS1(sims): a read-only install file (walls.iff, floors.iff, Build.iff, Sprites.iff, *.wll, *.flr) read
    /// with ReadFromFile (pictures and strings stay on disk until used). Never for files the game rewrites (saves).
    static std::unique_ptr<IffFile> OpenReadOnly(const std::string& path);
    /// Default for the content loaders' deferHeavy (host A/B tests: SIM3DS_NODEFER=1).
    static bool DEFER_HEAVY_CHUNKS;
    /// The file deferred chunks are loaded from (set by ReadFromFile).
    std::string SourcePath;
    /// Counters (all files): chunks deferred / loaded later, bytes not read thanks to deferral.
    static uint64_t DeferredChunks, DeferredLoads, DeferredBytesSkipped;
    /// FIX(sims) memory: before a file opened with ReadFromFile is rewritten (character files: outfit changes),
    /// every live IffFile reading from it loads its deferred chunks and decodes its on-disk sprite frames into
    /// memory, so nothing reads the new file at the old offsets; the pooled handle is closed. Returns the files detached.
    static int DetachSource(const std::string& path);
    /// FIX(sims) memory: frees the decoded pixels of every SPR#/SPR2 frame that can be decoded again from its file
    /// (read-only files: walls, floors, objects). Only when nothing references decoded pixels (zoom change after the
    /// texture cache was cleared, lot left). Returns the bytes freed.
    static size_t DropDecodedFrames();

private:
    IffChunk* prepare(IffChunk* input);
    IffChunk* AddReadChunk(const std::string& chunkType, uint16_t chunkID, uint16_t chunkFlags, const std::string& chunkLabel);
    bool LoadDeferred(IffChunk* chunk);

public:
    /// <summary>
    /// Get a chunk by its type and ID
    /// </summary>
    /// <typeparam name="T">Type of the chunk.</typeparam>
    /// <param name="id">ID of the chunk.</param>
    /// <returns>A chunk.</returns>
    template<class T>
    T* Get(uint16_t id)
    {
        auto it = ByChunkId.find(T::TYPE);
        if (it != ByChunkId.end())
        {
            auto& lookup = it->second;
            auto cit = lookup.find(id);
            if (cit != lookup.end())
            {
                return static_cast<T*>(prepare(cit->second));
            }
        }
        return nullptr;
    }

    std::vector<IffChunk*> ListAll();

    std::vector<IffChunk*> SilentListAll();

    /// <summary>
    /// List all chunks of a certain type
    /// </summary>
    /// <typeparam name="T">The type of the chunks to list.</typeparam>
    /// <returns>A list of chunks of the type. NOTE: C# returns null when the type is
    /// absent; here an empty vector is returned (see HasChunkType).</returns>
    template<class T>
    std::vector<T*> List()
    {
        std::vector<T*> result;
        auto it = ByChunkType.find(T::TYPE);
        if (it != ByChunkType.end())
        {
            result.reserve(it->second.size());
            for (auto item : it->second)
            {
                result.push_back(static_cast<T*>(prepare(item)));
            }
        }
        return result;
    }

    /// True when C# List<T>() would return non-null.
    bool HasChunkType(const char* type) const { return ByChunkType.find(type) != ByChunkType.end(); }
    template<class T> bool HasChunkType() const { return HasChunkType(T::TYPE); }

    void RemoveChunk(IffChunk* chunk);

    void FullRemoveChunk(IffChunk* chunk);

    void InitHash();

    void AddChunk(IffChunk* chunk);

    /// Registers a chunk this file owns (ownership helper for chunks created at runtime).
    void TakeOwnership(std::unique_ptr<IffChunk> chunk) { OwnedChunks.push_back(std::move(chunk)); }

    void MoveAndSwap(IffChunk* chunk, uint16_t targID);

    void Revert();

    template<class T>
    void Revert(T* chunk);

    void Patch(IffFile* piffFile);

    void SetFilename(const std::string& filename) override;

private:
    friend class PIFFRegistry;
    IffChunk* FindChunkByType(const std::string& typeKey, uint16_t id);
};

}}}} // namespace FSO::Files::Formats::IFF

// Template implementation that needs the PIFF chunk definition.
#include "files/formats/iff/chunks/PIFF.h"

namespace FSO { namespace Files { namespace Formats { namespace IFF {

template<class T>
void IffFile::Revert(T* chunk)
{
    chunk->RuntimeInfo = ChunkRuntimeState::Normal;
    if (RuntimeInfo.State != IffRuntimeState::Standalone && chunk->AddedByPatch)
    {
        //added by piff.
        for (auto piff : RuntimeInfo.Patches)
        {
            auto oldC = piff->Get<T>(chunk->ChunkID);
            if (oldC != nullptr)
            {
                chunk->ChunkData = oldC->OriginalData;
                chunk->Dispose();
                chunk->ChunkProcessed = false;
                chunk->RuntimeInfo = ChunkRuntimeState::Patched;
                prepare(chunk);
            }
        }
    }
    else
    {
        chunk->ChunkData = chunk->OriginalData;
        for (auto piffFile : RuntimeInfo.Patches)
        {
            auto piff = piffFile->List<Chunks::PIFF>()[0];
            for (auto& e : piff->Entries)
            {
                //C#: type.IsAssignableFrom(chunk.GetType()) - compared by type key here.
                if (e.Type != chunk->GetTypeKey() || e.ChunkID != chunk->ChunkID) continue;

                chunk->ChunkData = e.Apply(chunk->ChunkData);
                chunk->RuntimeInfo = ChunkRuntimeState::Patched;
            }
        }
        chunk->ChunkProcessed = false;
        chunk->Dispose();
        prepare(chunk);
    }
}

}}}} // namespace FSO::Files::Formats::IFF
