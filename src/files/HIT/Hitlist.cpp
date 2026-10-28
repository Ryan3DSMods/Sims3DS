/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/HIT/Hitlist.cs
#include "files/HIT/Hitlist.h"
#include "files/HIT/HITText.h"
#include "core/io.h"

namespace FSO { namespace Files { namespace HIT {

Hitlist::Hitlist(const std::vector<uint8_t>& Filedata)
{
    Read(Filedata);
}

Hitlist::Hitlist(const std::string& Filepath)
{
    Read(Text::ReadAllBytes(Filepath));
}

void Hitlist::Populate(const std::string& str)
{
    auto commaSplit = Text::Split(str, ',');
    for (size_t i = 0; i < commaSplit.size(); i++)
    {
        auto dashSplit = Text::Split(commaSplit[i], '-');
        if (dashSplit.size() > 1)
        { //range, parse two values and fill in the gap
            auto min = Text::ToUInt32(dashSplit[0]);
            auto max = Text::ToUInt32(dashSplit[1]);
            for (uint32_t j = min; j <= max; j++)
            {
                IDs.push_back(j);
            }
        }
        else
        { //literal entry, add to list
            IDs.push_back(Text::ToUInt32(commaSplit[i]));
        }
    }
}

void Hitlist::Read(const std::vector<uint8_t>& data)
{
    st::Reader Reader(data, st::Endian::Little);

    IDs.clear();
    auto VerOrCount = Reader.u32();

    try
    {
        if (VerOrCount == 1) //binary format, no hitlist is ever going to have length 1... (i hope)
        {
            m_IDCount = Reader.u32();

            for (uint32_t i = 0; i < m_IDCount; i++)
                IDs.push_back(Reader.u32());
        }
        else
        {
            auto str = Reader.cstr(VerOrCount, false);
            Populate(Text::Trim(str));
        }
    }
    catch (std::exception&)
    {
        Reader.seek(4); //attempt 3rd mystery format, count+int32
        IDs.clear();
        for (uint32_t i = 0; i < VerOrCount && Reader.pos() + 4 <= Reader.size(); i++)
            IDs.push_back(Reader.u32());
    }
}

std::shared_ptr<Hitlist> Hitlist::HitlistFromString(const std::string& str)
{
    auto result = std::make_shared<Hitlist>();
    result->Populate(str);
    return result;
}

}}} // namespace FSO::Files::HIT
