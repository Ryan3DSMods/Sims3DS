/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/formats/iff/chunks/HOUS.cs
#include "files/formats/iff/chunks/HOUS.h"
#include "files/formats/iff/IffFile.h"

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

void HOUS::Read(IffFile* iff, st::Reader& stream)
{
    (void)iff;
    st::Reader& io = stream;
    io.ByteOrder(st::Endian::Little);
    {
        auto zero = io.ReadInt32();
        (void)zero;
        Version = io.ReadInt32();
        auto suoh = io.ReadCString(4);
        (void)suoh;
        UnknownFlag = io.ReadInt32();
        UnknownOne = io.ReadInt32();
        UnknownNumber = io.ReadInt32();
        UnknownNegative = io.ReadInt32();
        CameraDir = io.ReadInt16();
        UnknownOne2 = io.ReadInt16();
        UnknownFlag2 = io.ReadInt16();
        GUID = io.ReadUInt32();
        RoofName = io.ReadNullTerminatedString();
    }
}

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
