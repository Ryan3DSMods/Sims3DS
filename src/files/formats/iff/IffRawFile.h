/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// TS1(sims): an IFF file as its raw chunk list, for writing game files back "like the original".
// IffFile (translated from FreeSims/FreeSO) parses chunks into classes and its Write() re-serialises only the classes
// it knows, in its own order, with no resource map (FreeSO: "todo: resource map offset"). Saving a TS1 house or
// character must instead keep every chunk the engine does not change byte for byte, in the original order, and keep
// the "rsmp" resource map the game writes valid. IffRawFile reads the chunk headers and payloads unchanged, lets the
// saver replace or add chunk payloads, and writes the file with a regenerated rsmp (same type and entry order as the
// original map; new chunks appended).
//
// rsmp layout (version 0, little endian, as written by The Sims 1):
//   u32 0 | u32 version (0) | "pmsr" | u32 size (0 in house files) | u32 type count
//   per type: 4 bytes type code (reversed, e.g. "IMIS") | u32 count
//     per chunk: u32 file offset of the chunk header | u16 id | u16 flags | label (NUL terminated, padded to an even
//     length with 0xA3)
// Version 1 maps (u32 ids, Pascal labels) exist in later files; they are regenerated in the same version.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace FSO { namespace Files { namespace Formats { namespace IFF {

class IffRawFile
{
public:
    struct Chunk {
        std::string Type;   // 4 characters
        uint16_t ID = 0;
        uint16_t Flags = 0x10;
        std::string Label;  // up to 64 bytes (the NUL padded label field)
        std::vector<uint8_t> LabelRaw; // the original 64-byte label field (kept when Label is unchanged)
        std::vector<uint8_t> Data;
    };

    std::vector<uint8_t> Header; // the 64-byte file header (60-byte signature + rsmp offset)
    std::vector<Chunk> Chunks;   // file order, without the rsmp chunk
    bool HadRsmp = false;
    int RsmpIndex = -1;          // position of the rsmp chunk in the original chunk order (it is written there again)
    uint16_t RsmpID = 0, RsmpFlags = 0x10;
    uint32_t RsmpVersion = 0;
    uint32_t RsmpSizeField = 0;
    std::vector<uint8_t> RsmpLabelRaw;
    /// original map order: (type, id) entries grouped by type in the order the map listed them
    std::vector<std::pair<std::string, std::vector<uint16_t>>> RsmpOrder;
    std::vector<uint8_t> TrailingBytes; // anything after the last chunk

    bool Load(const std::string& path, std::string* error = nullptr);
    bool Parse(const std::vector<uint8_t>& data, std::string* error = nullptr);

    Chunk* Find(const std::string& type, uint16_t id);
    /// Replaces the payload of an existing chunk, or appends a new chunk (before the rsmp position).
    Chunk* Set(const std::string& type, uint16_t id, const std::vector<uint8_t>& data, const std::string& label = std::string(),
               uint16_t flags = 0x10);
    void Remove(const std::string& type, uint16_t id);

    std::vector<uint8_t> Write() const;
    /// Writes to path via a temporary file and rename (a power cut during the save keeps the old file).
    bool Save(const std::string& path, std::string* error = nullptr) const;
};

}}}} // namespace FSO::Files::Formats::IFF
