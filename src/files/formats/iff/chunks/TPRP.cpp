/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/formats/iff/chunks/TPRP.cs
#include "files/formats/iff/chunks/TPRP.h"
#include "files/formats/iff/IffFile.h"

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

void TPRP::Read(IffFile* iff, st::Reader& stream)
{
    (void)iff;
    st::Reader& io = stream;
    io.ByteOrder(st::Endian::Little);
    {
        auto zero = io.ReadInt32();
        (void)zero;
        auto version = io.ReadInt32();
        auto name = io.ReadCString(4); //"PRPT", or randomly 4 null characters for no good reason
        (void)name;

        auto pCount = io.ReadInt32();
        auto lCount = io.ReadInt32();
        ParamNames = std::vector<std::string>(pCount < 0 ? 0 : pCount);
        LocalNames = std::vector<std::string>(lCount < 0 ? 0 : lCount);
        for (int i = 0; i < pCount; i++)
        {
            ParamNames[i] = (version == 5) ? io.ReadPascalString() : io.ReadNullTerminatedString();
        }
        for (int i = 0; i < lCount; i++)
        {
            LocalNames[i] = (version == 5) ? io.ReadPascalString() : io.ReadNullTerminatedString();
        }

        //TODO: flags and unknowns
    }
}

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
