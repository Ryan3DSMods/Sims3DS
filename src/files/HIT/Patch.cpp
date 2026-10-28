/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/HIT/Patch.cs
#include "files/HIT/Patch.h"
#include "files/HIT/HITText.h"
#include <algorithm>

namespace FSO { namespace Files { namespace HIT {

Patch::Patch(uint32_t id)
{
    FileID = id;
    TSO = true;
}

Patch::Patch(const std::string& patchString)
{
    auto elems = Text::Split(patchString, ',');
    if (elems.size() > 1) Name = elems[1];
    if (elems.size() > 2 && elems[2].size() >= 2)
    {
        Filename = elems[2].substr(1, elems[2].size() - 2);
        std::replace(Filename.begin(), Filename.end(), '\\', '/');
    }
    if (elems.size() > 3) Looped = elems[3] != "0";
    if (elems.size() > 4) Piano = elems[4] != "0";
}

}}} // namespace FSO::Files::HIT
