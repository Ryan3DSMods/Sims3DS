/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// TS1(sims): writing The Sims 1 "ObjM" chunk (the saved state of every object on a lot).
// FreeSO only reads OBJM (tso.files/Formats/IFF/Chunks/OBJM.cs; translated in OBJM.h/.cpp). The writer below is the
// exact mirror of that reader: every Write* function writes the fields its Read counterpart reads, in the same order,
// through IffFieldEncodeWriter. Bits the reader leaves uninterpreted at the end of a record (FreeSO "UnhandledData")
// are carried over unchanged. Re-encoding every record of the shipped houses reproduces them byte for byte (checked
// by `simsave objm`, see STUBS_save.md).
//
// Chunk layout (little endian, offsets relative to the byte after the magic = "offset base"):
//   u32 pad (0) | u32 version (0x33..0x49) | "MjbO" | u8 compression (1)
//   field encoded (object id, OBJT type id) pairs, terminated by id 0, then byte aligned
//   per object (in table order): s32 offset of the end of this record | the record (field encoded, see OBJMInstance)
//   tail (zeros, then 0xA3 fill in the shipped houses; kept byte for byte)
#pragma once
#include "files/formats/iff/chunks/OBJM.h"
#include "files/formats/iff/IffFieldEncodeWriter.h"
#include <string>
#include <utility>
#include <vector>

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

void WriteOBJMMultitile(const OBJMMultitile& m, IffFieldEncodeWriter& iop);
void WriteOBJMFootprint(const OBJMFootprint& f, IffFieldEncodeWriter& iop);
void WriteOBJMMotiveDelta(const OBJMMotiveDelta& d, IffFieldEncodeWriter& iop);
void WriteOBJMInteraction(const OBJMInteraction& i, IffFieldEncodeWriter& iop);
void WriteOBJMAccessory(const OBJMAccessory& a, IffFieldEncodeWriter& iop);
void WriteOBJMObjectUse(const OBJMObjectUse& u, IffFieldEncodeWriter& iop);
void WriteOBJMPerson(const OBJMPerson& p, uint32_t version, IffFieldEncodeWriter& iop);
void WriteOBJMStackFrame(const OBJMStackFrame& f, IffFieldEncodeWriter& iop);
void WriteOBJMRelationshipEntry(const OBJMRelationshipEntry& r, IffFieldEncodeWriter& iop);

/// One object record. hasLinkedSlot = false writes a slot count of 0 (no tile list entry and no container slots);
/// unhandledBits are the bits after the last field FreeSO reads ("0101|..." as IffFieldEncode::BitDebug gives them).
void WriteOBJMInstance(const OBJMInstance& inst, uint32_t version, IffFieldEncodeWriter& iop,
                       bool hasLinkedSlot = true, const std::string& unhandledBits = std::string());

/// The OBJM chunk split into its parts without interpreting the records (for writing back what is not changed).
struct OBJMRawChunk {
    uint32_t Pad = 0;
    uint32_t Version = 0;
    uint32_t Magic = 0x4f626a4d; // "MjbO" read little endian
    uint8_t Compression = 1;
    std::vector<std::pair<uint16_t, uint16_t>> Table; // (object id, OBJT type id), file order
    struct Record {
        int32_t Offset = 0;        // relative to the offset base: position of the record's first byte
        std::vector<uint8_t> Data; // the field encoded record
    };
    std::vector<Record> Records;   // same order as Table
    std::vector<uint8_t> Tail;     // bytes after the last record

    bool Parse(const std::vector<uint8_t>& chunkData, std::string& error);
};

/// A record to write: either the original bytes (Raw) or a decoded/modified instance.
struct OBJMRecordOut {
    uint16_t ObjectID = 0;
    uint16_t TypeID = 0;
    bool UseRaw = false;
    std::vector<uint8_t> Raw;          // UseRaw: written as is (its first byte must keep the parity it had: see below)
    int32_t RawOffsetParity = 0;       // UseRaw: parity of the record's original offset
    OBJMInstance Instance;             // !UseRaw
    bool HasLinkedSlot = true;
    std::string UnhandledBits;
};

/// Builds the chunk data. Records copied raw need their original offset parity (strings inside are padded relative
/// to the offset base); when the parity differs, the record is re-encoded from `Instance` (which the caller then must
/// provide) - see VMTS1LotSaver.
std::vector<uint8_t> WriteOBJMChunk(uint32_t pad, uint32_t version, const std::vector<OBJMRecordOut>& records,
                                    const std::vector<uint8_t>& tail);

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
