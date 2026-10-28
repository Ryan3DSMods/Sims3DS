/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/formats/iff/chunks/CARR.cs
#include "files/formats/iff/chunks/CARR.h"
#include "files/formats/iff/IffFile.h"
#include <stdexcept>

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

void CARR::Read(IffFile* iff, st::Reader& stream)
{
    (void)iff;
    st::Reader& io = stream;
    io.ByteOrder(st::Endian::Little);
    {
        io.ReadUInt32(); //pad
        auto version = io.ReadUInt32();
        (void)version;

        auto MjbO = io.ReadUInt32();
        (void)MjbO;

        auto compressionCode = io.ReadByte();
        if (compressionCode != 1) throw std::runtime_error("hey what!!");

        Name = io.ReadNullTerminatedString();
        if (Name.size() % 2 == 1) io.ReadByte();
        IffFieldEncode iop(&io);


        auto numLevels = iop.ReadInt32();

        JobLevels.clear();
        JobLevels.reserve(numLevels < 0 ? 0 : numLevels);
        for (int i=0; i<numLevels; i++)
        {
            JobLevels.emplace_back(iop);
        }
    }
}

int32_t CARR::GetJobData(int32_t level, int32_t data)
{
    auto& entry = JobLevels[level];
    switch (data)
    {
        case 0: //number of levels
            return (int32_t)JobLevels.size();
        case 1: //salary
            return entry.Salary;
        case 12: //start hour
            return entry.StartTime;
        case 13:
            return entry.EndTime;
        case 21:
            return entry.CarType;
        case 22:
            return 0;
        default:
            if (data < 12)
                return entry.MinRequired[data-2];
            else
                return entry.MotiveDelta[data-14];
    }
}

JobLevel::JobLevel(IffFieldEncode& iop)
{
    for (size_t i=0; i<sizeof(MinRequired)/sizeof(MinRequired[0]); i++)
        MinRequired[i] = iop.ReadInt32();
    for (size_t i = 0; i < sizeof(MotiveDelta)/sizeof(MotiveDelta[0]); i++)
        MotiveDelta[i] = iop.ReadInt32();
    Salary = iop.ReadInt32();
    StartTime = iop.ReadInt32();
    EndTime = iop.ReadInt32();
    CarType = iop.ReadInt32();

    JobName = iop.ReadString(false);
    MaleUniformMesh = iop.ReadString(false);
    FemaleUniformMesh = iop.ReadString(false);
    UniformSkin = iop.ReadString(false);
    unknown = iop.ReadString(true);
}

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
