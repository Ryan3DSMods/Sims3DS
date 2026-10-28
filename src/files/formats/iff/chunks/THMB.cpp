/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/formats/iff/chunks/THMB.cs
#include "files/formats/iff/chunks/THMB.h"
#include "files/formats/iff/IffFile.h"

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

void THMB::Read(IffFile* iff, st::Reader& stream)
{
    (void)iff;
    st::Reader& io = stream;
    io.ByteOrder(st::Endian::Little);
    {
        Width = io.ReadInt32();
        Height = io.ReadInt32();
        BaseYOff = io.ReadInt32();
        XOff = io.ReadInt32(); //0 in all cases i've found, pretty much?
        AddYOff = io.ReadInt32();
    }
}

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
