/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/formats/iff/IffFile.cs
#include "files/formats/iff/IffFile.h"
#include "files/FilePool.h"
#include <algorithm>
#include <cstring>
#include "files/formats/iff/PIFFRegistry.h"
#include "files/Hash.h"
#include <stdexcept>
#include <algorithm>

#include "files/formats/iff/chunks/STR.h"
#include "files/formats/iff/chunks/CARR.h"
#include "files/formats/iff/chunks/CTSS.h"
#include "files/formats/iff/chunks/PALT.h"
#include "files/formats/iff/chunks/OBJD.h"
#include "files/formats/iff/chunks/DGRP.h"
#include "files/formats/iff/chunks/SPR.h"
#include "files/formats/iff/chunks/SPR2.h"
#include "files/formats/iff/chunks/BHAV.h"
#include "files/formats/iff/chunks/TPRP.h"
#include "files/formats/iff/chunks/SLOT.h"
#include "files/formats/iff/chunks/GLOB.h"
#include "files/formats/iff/chunks/BCON.h"
#include "files/formats/iff/chunks/TTAB.h"
#include "files/formats/iff/chunks/OBJf.h"
#include "files/formats/iff/chunks/TTAs.h"
#include "files/formats/iff/chunks/FWAV.h"
#include "files/formats/iff/chunks/BMP.h"
#include "files/formats/iff/chunks/NBRS.h"
#include "files/formats/iff/chunks/NGBH.h"
#include "files/formats/iff/chunks/OBJT.h"
#include "files/formats/iff/chunks/ARRY.h"
#include "files/formats/iff/chunks/OBJM.h"
#include "files/formats/iff/chunks/WALm.h"
#include "files/formats/iff/chunks/FAMI.h"
#include "files/formats/iff/chunks/FAMs.h"
#include "files/formats/iff/chunks/SIMI.h"
#include "files/formats/iff/chunks/TTAT.h"
#include "files/formats/iff/chunks/HOUS.h"
#include "files/formats/iff/chunks/THMB.h"
#include "files/formats/iff/chunks/PIFF.h"

namespace FSO { namespace Files { namespace Formats { namespace IFF {

using namespace Chunks;

bool IffFile::RETAIN_CHUNK_DATA = false;

template<class T> static IffChunk* MakeChunk() { return new T(); }

const std::unordered_map<std::string, IffFile::ChunkFactory>& IffFile::CHUNK_TYPES()
{
    static const std::unordered_map<std::string, ChunkFactory> types =
    {
        {"STR#", &MakeChunk<STR>},
        {"CARR", &MakeChunk<CARR>},
        {"CTSS", &MakeChunk<CTSS>},
        {"PALT", &MakeChunk<PALT>},
        {"OBJD", &MakeChunk<OBJD>},
        {"DGRP", &MakeChunk<DGRP>},
        {"SPR#", &MakeChunk<SPR>},
        {"SPR2", &MakeChunk<SPR2>},
        {"BHAV", &MakeChunk<BHAV>},
        {"TPRP", &MakeChunk<TPRP>},
        {"SLOT", &MakeChunk<SLOT>},
        {"GLOB", &MakeChunk<GLOB>},
        {"BCON", &MakeChunk<BCON>},
        {"TTAB", &MakeChunk<TTAB>},
        {"OBJf", &MakeChunk<OBJf>},
        {"TTAs", &MakeChunk<TTAs>},
        {"FWAV", &MakeChunk<FWAV>},
        {"BMP_", &MakeChunk<BMP>},
        {"NBRS", &MakeChunk<NBRS>},
        {"NGBH", &MakeChunk<NGBH>},
        {"objt", &MakeChunk<OBJT>},
        {"Arry", &MakeChunk<ARRY>},
        {"ObjM", &MakeChunk<OBJM>},
        {"WALm", &MakeChunk<WALm>},
        {"FLRm", &MakeChunk<FLRm>},
        {"FAMI", &MakeChunk<FAMI>},
        {"FAMs", &MakeChunk<FAMs>},
        {"SIMI", &MakeChunk<SIMI>},
        {"TATT", &MakeChunk<TATT>},
        {"HOUS", &MakeChunk<HOUS>},
        {"THMB", &MakeChunk<THMB>},
        {"PIFF", &MakeChunk<PIFF>}
    };
    return types;
}

/// <summary>
/// Constructs a new IFF instance.
/// </summary>
// FIX(sims) memory: the IffFiles that read chunks or sprite frames from their SourcePath later (see DetachSource)
static std::vector<IffFile*>& LiveSourced() { static std::vector<IffFile*> v; return v; }

IffFile::IffFile()
{
}

/// <summary>
/// Constructs an IFF instance from a filepath.
/// </summary>
/// <param name="filepath">Path to the IFF.</param>
IffFile::IffFile(const std::string& filepath) : IffFile()
{
    std::vector<uint8_t> data;
    if (!st::ReadAllBytes(filepath, data))
        throw std::runtime_error("Could not open file: " + (st::LastFileError.empty() ? filepath : st::LastFileError));
    st::Reader stream(data, st::Endian::Big);
    this->Read(stream);
    size_t slash = filepath.find_last_of("/\\");
    SetFilename(slash == std::string::npos ? filepath : filepath.substr(slash + 1));
}

IffFile::~IffFile()
{
    if (!SourcePath.empty()) { // FIX(sims): see DetachSource
        auto& v = LiveSourced();
        v.erase(std::remove(v.begin(), v.end(), this), v.end());
    }
}

size_t IffFile::DropDecodedFrames()
{
    size_t bytes = 0;
    for (auto* f : LiveSourced()) {
        auto spr = f->ByChunkType.find("SPR#");
        if (spr != f->ByChunkType.end())
            for (auto* c : spr->second) if (c->ChunkProcessed)
                for (auto& fr : static_cast<Chunks::SPR*>(c)->Frames) if (fr) bytes += fr->DropDecoded();
        auto spr2 = f->ByChunkType.find("SPR2");
        if (spr2 != f->ByChunkType.end())
            for (auto* c : spr2->second) if (c->ChunkProcessed)
                for (auto& fr : static_cast<Chunks::SPR2*>(c)->Frames) if (fr) bytes += fr->DropDecoded();
    }
    return bytes;
}

int IffFile::DetachSource(const std::string& path)
{
    int n = 0;
    for (auto* f : LiveSourced()) {
        if (f->SourcePath != path) continue;
        for (auto& kv : f->ByChunkType)
            for (auto* c : kv.second) {
                if (c->DeferredOffset >= 0) f->LoadDeferred(c);
                if (c->ChunkProcessed && c->ChunkType == "SPR2")
                    for (auto& fr : static_cast<Chunks::SPR2*>(c)->Frames) if (fr) fr->DetachFromSource();
                if (c->ChunkProcessed && c->ChunkType == "SPR#")
                    for (auto& fr : static_cast<Chunks::SPR*>(c)->Frames) if (fr) fr->DetachFromSource();
            }
        f->SourcePath.clear();
        n++;
    }
    auto& v = LiveSourced();
    v.erase(std::remove_if(v.begin(), v.end(), [](IffFile* f) { return f->SourcePath.empty(); }), v.end());
    FSO::Files::FilePool::Close(path);
    return n;
}

void IffFile::Read(const std::vector<uint8_t>& data)
{
    st::Reader stream(data, st::Endian::Big);
    Read(stream);
}

/// <summary>
/// Reads an IFF from a stream.
/// </summary>
/// <param name="stream">The stream to read from.</param>
void IffFile::Read(st::Reader& stream)
{
    st::Reader& io = stream;
    io.ByteOrder(st::Endian::Big);
    {
        auto identifier = io.ReadCString(60, false);
        identifier.erase(std::remove(identifier.begin(), identifier.end(), '\0'), identifier.end()); //.Replace("\0", "")
        if (identifier != "IFF FILE 2.5:TYPE FOLLOWED BY SIZE JAMIE DOORNBOS & MAXIS 1")
        {
            if (identifier != "IFF FILE 2.0:TYPE FOLLOWED BY SIZE JAMIE DOORNBOS & MAXIS 1") //house11.iff, seems to read fine
                throw std::runtime_error("Invalid iff file!");
        }

        auto rsmpOffset = io.ReadUInt32();
        (void)rsmpOffset;

        while (io.HasMore())
        {
            auto chunkType = io.ReadCString(4);
            auto chunkSize = io.ReadUInt32();
            auto chunkID = io.ReadUInt16();
            auto chunkFlags = io.ReadUInt16();
            auto chunkLabel = io.ReadCString(64);
            while (!chunkLabel.empty() && chunkLabel.back() == '\0') chunkLabel.pop_back(); //.TrimEnd('\0')
            uint32_t chunkDataSize = chunkSize - 76;

            /** Do we understand this chunk type? **/
            auto& types = CHUNK_TYPES();
            auto tit = types.find(chunkType);
            if (tit == types.end())
            {
                /** Skip it! **/
                int64_t remaining = io.Length() - io.Position() - 1;
                io.Skip(std::min<int64_t>((int64_t)chunkDataSize, remaining)); //if the chunk is invalid, it will likely provide a chunk size beyond the limits of the file. (walls2.iff)
            }else{
                IffChunk* newChunk = AddReadChunk(chunkType, chunkID, chunkFlags, chunkLabel);
                newChunk->ChunkData = io.ReadBytes(chunkDataSize);
                if (RETAIN_CHUNK_DATA)
                {
                    newChunk->OriginalLabel = chunkLabel;
                    newChunk->OriginalData = newChunk->ChunkData;
                    newChunk->HasOriginalData = true;
                }
            }
        }
    }
}

// TS1(sims): the chunk registration of Read(), shared with ReadFromFile.
IffChunk* IffFile::AddReadChunk(const std::string& chunkType, uint16_t chunkID, uint16_t chunkFlags, const std::string& chunkLabel)
{
    auto& types = CHUNK_TYPES();
    auto tit = types.find(chunkType);
    if (tit == types.end()) return nullptr;
    IffChunk* newChunk = tit->second();
    OwnedChunks.emplace_back(newChunk);
    std::string chunkClass = newChunk->GetTypeKey();
    newChunk->ChunkID = chunkID;
    newChunk->OriginalID = chunkID;
    newChunk->ChunkFlags = chunkFlags;
    newChunk->ChunkLabel = chunkLabel;
    newChunk->ChunkType = chunkType;
    newChunk->ChunkParent = this;

    if (ByChunkType.find(chunkClass) == ByChunkType.end()){
        ByChunkType[chunkClass] = std::vector<IffChunk*>();
        ByChunkTypeOrder.push_back(chunkClass);
    }
    auto& byId = ByChunkId[chunkClass];
    ByChunkType[chunkClass].push_back(newChunk);
    if (byId.find(chunkID) == byId.end()) byId[chunkID] = newChunk;
    return newChunk;
}

bool IffFile::DEFER_HEAVY_CHUNKS = true;
uint64_t IffFile::DeferredChunks = 0, IffFile::DeferredLoads = 0, IffFile::DeferredBytesSkipped = 0;

namespace {
uint32_t BE32(const uint8_t* p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
uint32_t LE32(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
uint16_t BE16(const uint8_t* p) { return (uint16_t)((p[0] << 8) | p[1]); }
uint16_t LE16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
std::string Label64(const uint8_t* p) { // as Read(): 64 bytes, trailing NULs trimmed
    size_t n = 64;
    while (n > 0 && p[n - 1] == 0) n--;
    return std::string((const char*)p, n);
}
struct RsmpEntry { std::string Type; uint32_t Offset; uint16_t ID, Flags; std::string Label; uint32_t Bound; };
}

void IffFile::ReadFromFile(const std::string& path, int64_t base, uint32_t length, bool deferHeavy, const char* onlyType)
{
    using FSO::Files::FilePool;
    std::string err;
    auto readAt = [&](uint32_t off, uint32_t len, std::vector<uint8_t>& out) {
        if (!FilePool::ReadAt(path, base + off, len, out, &err)) throw std::runtime_error("Could not read " + err);
        if (out.size() != len) throw std::runtime_error("IFF: short read in " + path);
    };
    auto fullRead = [&]() {
        std::vector<uint8_t> all;
        readAt(0, length, all);
        Read(all);
    };
    if (SourcePath.empty()) LiveSourced().push_back(this); // FIX(sims): see DetachSource
    SourcePath = path;
    const uint32_t HEAD = 16384;
    std::vector<uint8_t> head;
    readAt(0, length < HEAD ? length : HEAD, head);
    if (head.size() < 64 + 76) { std::vector<uint8_t> all = head; Read(all); return; }
    uint32_t rsmpOff = BE32(&head[60]);
    if (rsmpOff < 64 || (uint64_t)rsmpOff + 76 + 20 > length) { fullRead(); return; }

    // the rsmp chunk: header + data (usually right after the file header, i.e. inside head)
    auto bytesAt = [&](uint32_t off, uint32_t len, std::vector<uint8_t>& tmp) -> const uint8_t* {
        if ((uint64_t)off + len <= head.size()) return head.data() + off;
        readAt(off, len, tmp);
        return tmp.data();
    };
    std::vector<uint8_t> tmp;
    const uint8_t* rh = bytesAt(rsmpOff, 76, tmp);
    if (std::memcmp(rh, "rsmp", 4) != 0) { fullRead(); return; }
    uint32_t rsmpSize = BE32(rh + 4);
    if (rsmpSize < 76 + 20 || (uint64_t)rsmpOff + rsmpSize > length) { fullRead(); return; }
    std::vector<uint8_t> rdata;
    {
        std::vector<uint8_t> t2;
        const uint8_t* rd = bytesAt(rsmpOff + 76, rsmpSize - 76, t2);
        rdata.assign(rd, rd + (rsmpSize - 76));
    }
    // reserved(4) version(4) 'pmsr'(4) size(4) typeCount(4) { type(4, reversed) count(4) { offset(4) id(2) flags(2) name\0 (even) } }
    if (LE32(&rdata[4]) != 0 || std::memcmp(&rdata[8], "pmsr", 4) != 0) { fullRead(); return; }
    uint32_t typeCount = LE32(&rdata[16]);
    size_t p = 20;
    std::vector<RsmpEntry> entries;
    bool ok = true;
    for (uint32_t t = 0; t < typeCount && ok; t++) {
        if (p + 8 > rdata.size()) { ok = false; break; }
        std::string type = { (char)rdata[p + 3], (char)rdata[p + 2], (char)rdata[p + 1], (char)rdata[p] };
        uint32_t count = LE32(&rdata[p + 4]);
        p += 8;
        for (uint32_t i = 0; i < count; i++) {
            if (p + 8 > rdata.size()) { ok = false; break; }
            RsmpEntry e;
            e.Type = type;
            e.Offset = LE32(&rdata[p]);
            e.ID = LE16(&rdata[p + 4]);
            e.Flags = LE16(&rdata[p + 6]);
            p += 8;
            size_t z = p;
            while (z < rdata.size() && rdata[z] != 0) z++;
            if (z >= rdata.size()) { ok = false; break; }
            e.Label.assign((const char*)&rdata[p], z - p);
            size_t n = z - p + 1;
            p += (n + 1) & ~(size_t)1;
            entries.push_back(e);
        }
    }
    if (!ok || entries.empty()) { fullRead(); return; }
    std::sort(entries.begin(), entries.end(), [](const RsmpEntry& a, const RsmpEntry& b) { return a.Offset < b.Offset; });
    // bounds: chunks are contiguous; the rsmp chunk itself is not listed
    for (size_t i = 0; i < entries.size(); i++) {
        uint32_t next = i + 1 < entries.size() ? entries[i + 1].Offset : length;
        if (rsmpOff > entries[i].Offset && rsmpOff < next) next = rsmpOff;
        if (entries[i].Offset < 64 || next <= entries[i].Offset + 76 - 1 || next > length) { ok = false; break; }
        if (i + 1 < entries.size() && entries[i + 1].Offset == entries[i].Offset) { ok = false; break; }
        entries[i].Bound = next - entries[i].Offset;
    }
    if (!ok) { fullRead(); return; }

    // deferred: pictures and the chunks only read on demand (strings, catalogue text, pie menu labels, draw groups,
    // sounds, labels) -- ~95% of an object file
    auto heavy = [&](const std::string& t) {
        return deferHeavy && (t == "SPR2" || t == "SPR#" || t == "BMP_" || t == "STR#" || t == "CTSS" || t == "TTAs" ||
                              t == "DGRP" || t == "FWAV" || t == "TPRP" || t == "THMB");
    };
    if (onlyType != nullptr) {
        entries.erase(std::remove_if(entries.begin(), entries.end(), [&](const RsmpEntry& e) { return e.Type != onlyType; }), entries.end());
        // FIX(sims) memory: heavy types stay deferred with onlyType too (work.iff "STR#" for the job dialog tokens kept
        // the raw bytes of every career's strings: 4 MB; now one seek + read per job looked up)
    }
    auto& types = CHUNK_TYPES();
    // read the needed chunks in runs (adjacent chunks with small gaps share one read)
    struct Run { uint32_t Start, End; std::vector<uint8_t> Data; };
    std::vector<Run> runs;
    for (auto& e : entries) {
        if (types.find(e.Type) == types.end() || heavy(e.Type)) continue;
        uint32_t s = e.Offset, en = e.Offset + e.Bound;
        if (!runs.empty() && s <= runs.back().End + 8192) runs.back().End = en;
        else runs.push_back({ s, en, {} });
    }
    for (auto& r : runs) {
        if (r.End <= head.size()) continue; // inside head
        readAt(r.Start, r.End - r.Start, r.Data);
    }
    auto chunkBytes = [&](const RsmpEntry& e) -> const uint8_t* {
        if ((uint64_t)e.Offset + e.Bound <= head.size()) return head.data() + e.Offset;
        for (auto& r : runs) if (e.Offset >= r.Start && e.Offset + e.Bound <= r.End && !r.Data.empty()) return r.Data.data() + (e.Offset - r.Start);
        return nullptr;
    };
    // validate every chunk header we have before registering anything (any mismatch: plain full read)
    for (auto& e : entries) {
        if (types.find(e.Type) == types.end() || heavy(e.Type)) continue;
        const uint8_t* c = chunkBytes(e);
        if (c == nullptr || std::memcmp(c, e.Type.data(), 4) != 0 || BE32(c + 4) < 76 || BE32(c + 4) > e.Bound || BE16(c + 8) != e.ID) { fullRead(); return; }
    }
    for (auto& e : entries) {
        if (types.find(e.Type) == types.end()) continue;
        if (heavy(e.Type)) {
            IffChunk* ch = AddReadChunk(e.Type, e.ID, e.Flags, e.Label);
            ch->DeferredOffset = base + e.Offset;
            ch->SourceOffset = ch->DeferredOffset;
            ch->DeferredSize = e.Bound;
            DeferredChunks++;
            DeferredBytesSkipped += e.Bound;
            continue;
        }
        const uint8_t* c = chunkBytes(e);
        uint32_t size = BE32(c + 4);
        IffChunk* ch = AddReadChunk(e.Type, e.ID, BE16(c + 10), Label64(c + 12));
        ch->ChunkData.assign(c + 76, c + size);
        if (RETAIN_CHUNK_DATA)
        {
            ch->OriginalLabel = ch->ChunkLabel;
            ch->OriginalData = ch->ChunkData;
            ch->HasOriginalData = true;
        }
    }
}

std::unique_ptr<IffFile> IffFile::OpenReadOnly(const std::string& path)
{
    std::string err;
    int64_t size = FSO::Files::FilePool::Size(path, &err);
    if (size < 0) throw std::runtime_error("Could not open file: " + err);
    std::unique_ptr<IffFile> iff(new IffFile());
    iff->ReadFromFile(path, 0, (uint32_t)size, DEFER_HEAVY_CHUNKS);
    size_t slash = path.find_last_of("/\\");
    iff->SetFilename(slash == std::string::npos ? path : path.substr(slash + 1));
    return iff;
}

bool IffFile::LoadDeferred(IffChunk* chunk)
{
    if (chunk->DeferredOffset < 0) return false;
    std::vector<uint8_t> bytes;
    std::string err;
    int64_t off = chunk->DeferredOffset;
    chunk->DeferredOffset = -1;
    if (!FSO::Files::FilePool::ReadAt(SourcePath, off, chunk->DeferredSize, bytes, &err))
        throw std::runtime_error("Could not read " + err);
    if (bytes.size() < 76 || std::memcmp(bytes.data(), chunk->ChunkType.data(), 4) != 0)
        throw std::runtime_error("IFF: deferred chunk mismatch in " + SourcePath);
    uint32_t size = BE32(&bytes[4]);
    if (size < 76 || size > bytes.size()) throw std::runtime_error("IFF: bad deferred chunk size in " + SourcePath);
    chunk->ChunkLabel = Label64(&bytes[12]);
    chunk->ChunkData.assign(bytes.begin() + 76, bytes.begin() + size);
    DeferredLoads++;
    return true;
}

void IffFile::Write(st::Writer& stream)
{
    st::Writer& io = stream;
    io.ByteOrder(st::Endian::Big);
    {
        io.WriteCString(std::string("IFF FILE 2.5:TYPE FOLLOWED BY SIZE\0 JAMIE DOORNBOS & MAXIS 1", 60), 60);
        io.WriteUInt32(0); //todo: resource map offset

        auto chunks = ListAll();
        for (auto c : chunks)
        {
            //C#: CHUNK_TYPES.FirstOrDefault(x => x.Value == c.GetType()).Key
            std::string typeString;
            for (auto& kv : CHUNK_TYPES())
            {
                std::unique_ptr<IffChunk> probe(kv.second());
                if (std::string(probe->GetTypeKey()) == c->GetTypeKey()) { typeString = kv.first; break; }
            }

            io.WriteCString(typeString.empty() ? c->ChunkType : typeString, 4);

            std::vector<uint8_t> data;
            {
                st::Writer cstr(st::Endian::Big);
                if (c->Write(this, cstr)) data = cstr.ToArray();
                else data = c->OriginalData;
            }

            //todo: exporting PIFF as IFF SHOULD NOT DO THIS
            c->OriginalData = data; //if we revert, it is to the last save.
            c->HasOriginalData = true;
            c->AddedByPatch = false;
            c->RuntimeInfo = ChunkRuntimeState::Normal;

            io.WriteUInt32((uint32_t)data.size() + 76);
            io.WriteUInt16(c->ChunkID);
            io.WriteUInt16(c->ChunkFlags);
            io.WriteCString(c->ChunkLabel, 64);
            io.WriteBytes(data);
        }
    }
}

IffChunk* IffFile::prepare(IffChunk* input)
{
    IffChunk* chunk = input;
    if (chunk->ChunkProcessed != true)
    {
        //lock(chunk) dropped: single threaded on 3DS
        if (chunk->ChunkProcessed != true)
        {
            {
                if (chunk->DeferredOffset >= 0) LoadDeferred(chunk); // TS1(sims): see ReadFromFile
                st::Reader stream(chunk->ChunkData, st::Endian::Big);
                chunk->Read(this, stream);
                chunk->ChunkData.clear();
                chunk->ChunkData.shrink_to_fit(); //= null
                chunk->ChunkProcessed = true;
            }
        }
    }
    return input;
}

std::vector<IffChunk*> IffFile::ListAll()
{
    std::vector<IffChunk*> result;
    for (auto& key : ByChunkTypeOrder)
    {
        auto it = ByChunkType.find(key);
        if (it == ByChunkType.end()) continue;
        auto copy = it->second; //chunks may be re-prepared during iteration
        for (auto chunk : copy)
        {
            result.push_back(this->prepare(chunk));
        }
    }
    return result;
}

std::vector<IffChunk*> IffFile::SilentListAll()
{
    std::vector<IffChunk*> result;
    for (auto& key : ByChunkTypeOrder)
    {
        auto it = ByChunkType.find(key);
        if (it == ByChunkType.end()) continue;
        for (auto chunk : it->second)
        {
            result.push_back(chunk);
        }
    }
    return result;
}

IffChunk* IffFile::FindChunkByType(const std::string& typeKey, uint16_t id)
{
    auto it = ByChunkId.find(typeKey);
    if (it == ByChunkId.end()) return nullptr;
    auto cit = it->second.find(id);
    if (cit == it->second.end()) return nullptr;
    return cit->second;
}

void IffFile::RemoveChunk(IffChunk* chunk)
{
    std::string type = chunk->GetTypeKey();
    ByChunkId[type].erase(chunk->ChunkID);
    auto& list = ByChunkType[type];
    auto it = std::find(list.begin(), list.end(), chunk);
    if (it != list.end()) list.erase(it);
}

void IffFile::FullRemoveChunk(IffChunk* chunk)
{
    //register this chunk as one that has been hard removed
    if (!chunk->AddedByPatch) RemovedOriginal.push_back(chunk);
    RemoveChunk(chunk);
}

void IffFile::InitHash()
{
    if (ExecutableHash != 0) return;
    auto bhavIt = ByChunkType.find(BHAV::TYPE);
    if (bhavIt != ByChunkType.end())
    {
        std::vector<IffChunk*> executableTypes = bhavIt->second;
        auto bconIt = ByChunkType.find(BCON::TYPE);
        if (bconIt != ByChunkType.end()) executableTypes.insert(executableTypes.end(), bconIt->second.begin(), bconIt->second.end());
        Hash hash;
        hash.Init();
        for (IffChunk* chunk : executableTypes)
        {
            //C#: hash.Update(chunk.ChunkData ?? chunk.OriginalData, chunk.ChunkData.Length);
            const auto& data = (chunk->ChunkProcessed && chunk->ChunkData.empty()) ? chunk->OriginalData : chunk->ChunkData;
            hash.Update(data.data(), (int32_t)data.size());
        }
        ExecutableHash = hash.Digest();
    }
}

void IffFile::AddChunk(IffChunk* chunk)
{
    std::string type = chunk->GetTypeKey();
    chunk->ChunkParent = this;

    if (ByChunkType.find(type) == ByChunkType.end())
    {
        ByChunkType[type] = std::vector<IffChunk*>();
        ByChunkTypeOrder.push_back(type);
    }
    if (ByChunkId.find(type) == ByChunkId.end())
    {
        ByChunkId[type] = std::unordered_map<uint16_t, IffChunk*>();
    }

    auto& byId = ByChunkId[type];
    if (byId.find(chunk->ChunkID) != byId.end()) throw std::runtime_error("An item with the same key has already been added."); //Dictionary.Add
    byId[chunk->ChunkID] = chunk;
    ByChunkType[type].push_back(chunk);
}

void IffFile::MoveAndSwap(IffChunk* chunk, uint16_t targID)
{
    if (chunk->ChunkID == targID) return;
    std::string type = chunk->GetTypeKey();
    IffChunk* targ = nullptr;
    auto it = ByChunkId.find(type);
    if (it != ByChunkId.end())
    {
        auto cit = it->second.find(targID);
        if (cit != it->second.end()) targ = cit->second;
    }

    IffChunk* tChunk = targ;

    if (tChunk != nullptr) RemoveChunk(tChunk);
    auto oldID = chunk->ChunkID;
    RemoveChunk(chunk);
    chunk->ChunkID = targID;
    AddChunk(chunk);
    if (tChunk != nullptr)
    {
        tChunk->ChunkID = oldID;
        AddChunk(tChunk);
    }
}

void IffFile::Revert()
{
    //revert all iffs and rerun patches

    for (auto chunk : SilentListAll())
    {
        if (chunk->AddedByPatch) chunk->ChunkParent->RemoveChunk(chunk);
        else
        {
            if (chunk->HasOriginalData)
            {
                //revert if we have a state to revert to. for new chunks this is not really possible.
                chunk->ChunkData = chunk->OriginalData;
                chunk->ChunkParent->MoveAndSwap(chunk, chunk->OriginalID);
                chunk->Dispose();
                chunk->ChunkProcessed = false;
            }
        }
    }

    //add back removed chunks
    for (auto chunk : RemovedOriginal)
    {
        chunk->ChunkParent->AddChunk(chunk);
    }
    RemovedOriginal.clear();

    //rerun patches
    for (auto piff : RuntimeInfo.Patches)
    {
        Patch(piff);
    }

    for (auto& key : ByChunkTypeOrder)
    {
        auto it = ByChunkType.find(key);
        if (it == ByChunkType.end()) continue;
        auto copy = it->second;
        for (IffChunk* chunk : copy)
        {
            prepare(chunk);
        }
    }
}

void IffFile::Patch(IffFile* piffFile)
{
    if (RuntimeInfo.State == IffRuntimeState::ReadOnly) RuntimeInfo.State = IffRuntimeState::PIFFPatch;
    auto piff = piffFile->List<Chunks::PIFF>()[0];

    //patch existing chunks using the PIFF chunk
    //also delete chunks marked for deletion

    std::vector<IffChunk*> moveChunks;
    std::vector<uint16_t> newIDs;

    for (auto& e : piff->Entries)
    {
        //C#: var type = CHUNK_TYPES[e.Type]; (throws KeyNotFound when unknown)
        auto& types = CHUNK_TYPES();
        auto tit = types.find(e.Type);
        if (tit == types.end()) throw std::runtime_error("The given key was not present in the dictionary.");
        std::unique_ptr<IffChunk> probe(tit->second());
        std::string type = probe->GetTypeKey();

        auto chunksIt = ByChunkId.find(type);
        if (chunksIt == ByChunkId.end()) continue;
        auto objIt = chunksIt->second.find(e.ChunkID);
        if (objIt == chunksIt->second.end()) continue;

        auto chunk = objIt->second;
        if (e.Delete)
        {
            FullRemoveChunk(chunk); //removed by PIFF
        }
        else
        {
            chunk->ChunkData = e.Apply(chunk->ChunkData);
            if (e.ChunkLabel != "") chunk->ChunkLabel = e.ChunkLabel;
            chunk->RuntimeInfo = ChunkRuntimeState::Patched;

            if (e.ChunkID != e.NewChunkID)
            {
                moveChunks.push_back(chunk);
                newIDs.push_back(e.NewChunkID);
            }
        }
    }

    for (size_t i=0; i<moveChunks.size(); i++)
        MoveAndSwap(moveChunks[i], newIDs[i]);

    //add chunks present in the piff to the original file
    for (auto& key : piffFile->ByChunkTypeOrder)
    {
        if (key == Chunks::PIFF::TYPE) continue;
        auto it = piffFile->ByChunkType.find(key);
        if (it == piffFile->ByChunkType.end()) continue;
        for (auto res : it->second)
        {
            auto chunk = res;
            chunk->AddedByPatch = true;
            std::string ctype = chunk->GetTypeKey();
            if (ByChunkId.find(ctype) == ByChunkId.end() || ByChunkId[ctype].find(chunk->ChunkID) == ByChunkId[ctype].end()) this->AddChunk(chunk);
        }
    }
}

void IffFile::SetFilename(const std::string& filename)
{
    Filename = filename;
    auto piffs = PIFFRegistry::GetPIFFs(filename);
    RuntimeInfo.Patches.clear();
    if (piffs != nullptr)
    {
        //apply patches
        for (auto piff : *piffs)
        {
            Patch(piff);
            if (RETAIN_CHUNK_DATA) RuntimeInfo.Patches.push_back(piff);
        }
    }
}

}}}} // namespace FSO::Files::Formats::IFF
