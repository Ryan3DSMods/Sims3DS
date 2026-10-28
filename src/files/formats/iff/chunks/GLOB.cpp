/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/. 
 */
// Translated from FreeSims sims.files/formats/iff/chunks/GLOB.cs
#include "files/formats/iff/chunks/GLOB.h"
#include "files/formats/iff/IffFile.h"

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks {

void GLOB::Read(IffFile* iff, st::Reader& stream)
{
    (void)iff;
    st::Reader& io = stream;
    io.ByteOrder(st::Endian::Little);
    {
        std::string temp;
        auto num = io.ReadByte();
        if (num < 48)
        { //less than smallest ASCII value for valid filename character, so assume this is a pascal string
            temp += io.ReadCString(num);
        }
        else
        { //we're actually a null terminated string!
            temp += (char)num;
            while (stream.Position() < stream.Length())
            {
                char read = (char)io.ReadByte();
                if (read == 0) break;
                else temp += read;
            }
        }
        Name = temp;
    }
}

bool GLOB::Write(IffFile* iff, st::Writer& stream)
{
    (void)iff;
    st::Writer& io = stream;
    io.ByteOrder(st::Endian::Little);
    {
        io.WriteNullTerminatedString(Name);
    }
    return true;
}

}}}}} // namespace FSO::Files::Formats::IFF::Chunks
