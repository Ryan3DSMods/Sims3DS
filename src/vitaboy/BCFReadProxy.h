/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// IOProxy / BCFReadProxy / CMXReadProxy
//
// NEW (not in the FreeSims tree; modelled on later FreeSO's FSO.Vitaboy.BCFReadProxy /
// CMXReadProxy). The Sims 1 ships the same Vitaboy data in two encodings:
//   * binary: .bcf (skeletons/suits/skills), .bmf (meshes)   - little endian, 1-byte pascal strings
//   * text:   .cmx (same content as .bcf), .skn (same as .bmf) - one value / string per line,
//             vectors written as "| x y z |", comment lines starting with "//", "version 300" header.
// Both are read through this interface so Skeleton/Appearance/Animation/Mesh have a single reader
// (the field order of the text files is identical to the binary ones, verified against the
// shipped GameData; the only exception is the SKN blend list, see Mesh.cpp).
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "core/io.h"

namespace FSO { namespace Vitaboy {

class IOProxy {
public:
    virtual ~IOProxy() {}
    virtual int32_t ReadInt32() = 0;
    virtual uint32_t ReadUInt32() { return (uint32_t)ReadInt32(); }
    virtual int16_t ReadInt16() = 0;
    virtual uint8_t ReadByte() = 0;
    virtual float ReadFloat() = 0;
    virtual std::string ReadPascalString() = 0;
    virtual std::string ReadLongPascalString() = 0;
    virtual bool HasMore() = 0;
    /// True for the text (CMX/SKN) encoding.
    virtual bool IsText() const { return false; }
};

/// Binary proxy over st::Reader (FSO.Files.Utils.IoBuffer). Used for .bcf/.bmf (little endian)
/// and for the TSO big-endian .anim/.skel/.mesh/.apr/.oft/.bnd formats.
class BinaryReadProxy : public IOProxy {
public:
    BinaryReadProxy(const uint8_t* data, size_t size, st::Endian e) : io(data, size, e) {}
    explicit BinaryReadProxy(const std::vector<uint8_t>& data, st::Endian e = st::Endian::Little) : io(data, e) {}
    int32_t ReadInt32() override { return io.ReadInt32(); }
    uint32_t ReadUInt32() override { return io.ReadUInt32(); }
    int16_t ReadInt16() override { return io.ReadInt16(); }
    uint8_t ReadByte() override { return io.ReadByte(); }
    float ReadFloat() override { return io.ReadFloat(); }
    std::string ReadPascalString() override { return io.ReadPascalString(); }
    std::string ReadLongPascalString() override { return io.ReadLongPascalString(); }
    bool HasMore() override { return io.has_more(); }
    st::Reader io;
};

/// Text proxy for TS1 .cmx / .skn files.
/// Numbers are whitespace separated tokens (may share a line, e.g. "17 3 5" faces or
/// "| 0.1 0.2 0.3 |" vectors; '|' tokens are skipped). Strings are whole lines.
class CMXReadProxy : public IOProxy {
public:
    /// skipHeader: consume leading "//" comment lines and a "version N" line (CMX files).
    CMXReadProxy(const std::vector<uint8_t>& data, bool skipHeader);
    int32_t ReadInt32() override;
    int16_t ReadInt16() override { return (int16_t)ReadInt32(); }
    uint8_t ReadByte() override { return (uint8_t)ReadInt32(); }
    float ReadFloat() override;
    std::string ReadPascalString() override;
    std::string ReadLongPascalString() override { return ReadPascalString(); }
    bool HasMore() override;
    bool IsText() const override { return true; }
    int32_t Version = 0;
private:
    std::string NextToken();
    bool NextLine(std::string& out);
    std::vector<std::string> Lines;
    size_t LineIndex = 0;
    std::string Current; // remainder of the line currently being tokenised
    bool InLine = false;
};

}}
