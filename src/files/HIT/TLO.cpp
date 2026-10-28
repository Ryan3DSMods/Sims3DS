/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/HIT/TLO.cs
#include "files/HIT/TLO.h"
#include "files/HIT/HITText.h"
#include "core/io.h"

namespace FSO { namespace Files { namespace HIT {

TLO::TLO(const std::vector<uint8_t>& Filedata)
{
    Read(Filedata);
}

TLO::TLO(const std::string& Filepath)
{
    Read(Text::ReadAllBytes(Filepath));
}

// The two C# constructors carry the same body; it is shared here.
void TLO::Read(const std::vector<uint8_t>& Filedata)
{
    st::Reader Reader(Filedata, st::Endian::Little);

    Reader.ReadBytes(4); //Reserved.
    m_Count = Reader.u32();

    for (uint32_t i = 0; i < m_Count; i++)
    {
        TLOSection Section;
        Section.Name = Reader.cstr((size_t)Reader.s32(), false);
        Section.GroupID1 = Reader.u32();
        Section.FileID1 = Reader.u32();
        Section.GroupID2 = Reader.u32();
        Section.FileID1 = Reader.u32(); // (sic) the C# assigns FileID1 twice and never FileID2
        Section.TypeID = Reader.u32();
        Section.GroupID3 = Reader.u32();
        Section.FileID3 = Reader.u32();

        Sections.push_back(Section);
    }
}

}}} // namespace FSO::Files::HIT
