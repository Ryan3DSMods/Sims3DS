/*
* This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
* If a copy of the MPL was not distributed with this file, You can obtain one at
* http://mozilla.org/MPL/2.0/. 
*/
// Translated from FreeSims sims.files/formats/iff/chunks/BHAV.cs
#pragma once
#include "files/formats/iff/AbstractIffChunk.h"
#include <array>

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

class BHAVInstruction 
{
public:
    uint16_t Opcode = 0;
    uint8_t TruePointer = 0;
    uint8_t FalsePointer = 0;
    std::array<uint8_t, 8> Operand = {{0,0,0,0,0,0,0,0}};
    bool Breakpoint = false; //only used at runtime
};

/// <summary>
/// This chunk type holds Behavior code in SimAntics.
/// </summary>
class BHAV : public IffChunk
{
public:
    IFF_CHUNK_TYPE("BHAV")

    std::vector<BHAVInstruction> Instructions;
    uint8_t Type = 0;
    uint8_t Args = 0;
    uint16_t Locals = 0;
    uint16_t Flags = 0;

    uint32_t RuntimeVer = 0;

    /// <summary>
    /// Reads a BHAV from a stream.
    /// </summary>
    /// <param name="iff">Iff instance.</param>
    /// <param name="stream">A Stream instance holding a BHAV chunk.</param>
    void Read(IffFile* iff, st::Reader& stream) override;

    bool Write(IffFile* iff, st::Writer& stream) override;
};

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
