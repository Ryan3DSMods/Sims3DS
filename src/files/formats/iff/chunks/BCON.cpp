/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/. 
 */
// Translated from FreeSims sims.files/formats/iff/chunks/BCON.cs
#include "files/formats/iff/chunks/BCON.h"
#include "files/formats/iff/IffFile.h"

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

void BCON::Read(IffFile* iff, st::Reader& stream)
{
    (void)iff;
    st::Reader& io = stream;
    io.ByteOrder(st::Endian::Little);
    {
        auto num = io.ReadByte();
        Flags = io.ReadByte();

        Constants = std::vector<uint16_t>(num);
        for (int i = 0; i < num; i++)
        {
            Constants[i] = io.ReadUInt16();
        }
    }
}

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
