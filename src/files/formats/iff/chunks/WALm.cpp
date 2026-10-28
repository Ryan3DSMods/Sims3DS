/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/formats/iff/chunks/WALm.cs
#include "files/formats/iff/chunks/WALm.h"
#include "files/formats/iff/IffFile.h"

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

void WALm::Read(IffFile* iff, st::Reader& stream)
{
    (void)iff;
    st::Reader& io = stream;
    io.ByteOrder(st::Endian::Little);
    {
        auto zero = io.ReadInt32();
        (void)zero;
        auto version = io.ReadInt32(); //should be 0
        (void)version;
        auto walm = io.ReadInt32(); //mLAW/mRLF
        (void)walm;

        auto count = io.ReadInt32();
        Entries.clear();

        for (int i=0; i<count; i++)
        {
            //size of fields depends on chunk id.
            Entries.push_back(WALmEntry(io, ChunkID));
        }
    }
}

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
