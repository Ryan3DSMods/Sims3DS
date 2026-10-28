/*
* This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
* If a copy of the MPL was not distributed with this file, You can obtain one at
* http://mozilla.org/MPL/2.0/. 
*/
// Translated from FreeSims sims.files/formats/iff/chunks/BHAV.cs
#include "files/formats/iff/chunks/BHAV.h"
#include "files/formats/iff/IffFile.h"

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

void BHAV::Read(IffFile* iff, st::Reader& stream)
{
    (void)iff;
    st::Reader& io = stream;
    io.ByteOrder(st::Endian::Little);
    {
        auto version = io.ReadUInt16();
        uint32_t count = 0;

        if (version == 0x8000)
        {
            count = io.ReadUInt16();
            io.Skip(8);
        }
        else if (version == 0x8001)
        {
            count = io.ReadUInt16();
            auto unknown = io.ReadBytes(8);
            (void)unknown;
        }
        else if (version == 0x8002)
        {
            count = io.ReadUInt16();
            this->Type = io.ReadByte();
            this->Args = io.ReadByte();
            this->Locals = io.ReadUInt16();
            this->Flags = io.ReadUInt16();
            io.Skip(2);
        }
        else if (version == 0x8003)
        {
            this->Type = io.ReadByte();
            this->Args = io.ReadByte();
            this->Locals = io.ReadByte();
            io.Skip(2);
            this->Flags = io.ReadUInt16();
            count = io.ReadUInt32();
        }

        Instructions = std::vector<BHAVInstruction>(count);
        for (uint32_t i = 0; i < count; i++)
        {
            BHAVInstruction instruction;
            instruction.Opcode = io.ReadUInt16();
            instruction.TruePointer = io.ReadByte();
            instruction.FalsePointer = io.ReadByte();
            auto operand = io.ReadBytes(8);
            for (size_t j = 0; j < 8 && j < operand.size(); j++) instruction.Operand[j] = operand[j];
            Instructions[i] = instruction;
        }
    }
}

bool BHAV::Write(IffFile* iff, st::Writer& stream)
{
    (void)iff;
    st::Writer& io = stream;
    io.ByteOrder(st::Endian::Little);
    {
        io.WriteUInt16(0x8003);
        io.WriteByte(Type);
        io.WriteByte(Args);
        io.WriteByte((uint8_t)Locals);
        io.WriteBytes(std::vector<uint8_t>{ 0, 0 });
        io.WriteUInt16(Flags);
        io.WriteUInt32((uint16_t)Instructions.size());

        for (auto& inst : Instructions)
        {
            io.WriteUInt16(inst.Opcode);
            io.WriteByte(inst.TruePointer);
            io.WriteByte(inst.FalsePointer);
            io.WriteBytes(inst.Operand.data(), inst.Operand.size());
        }
    }
    return true;
}

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
