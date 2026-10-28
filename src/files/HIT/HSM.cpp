/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/HIT/HSM.cs
#include "files/HIT/HSM.h"
#include "files/HIT/HITText.h"

namespace FSO { namespace Files { namespace HIT {

HSM::HSM(const std::vector<uint8_t>& Filedata)
{
    ReadFile(Filedata);
}

HSM::HSM(const std::string& Filepath)
{
    ReadFile(Text::ReadAllBytes(Filepath));
}

void HSM::ReadFile(const std::vector<uint8_t>& stream)
{
    Constants.clear();

    for (auto& line : Text::ReadLines(stream))
    {
        auto Values = Text::Split(line, ' ');
        // C#: an empty/short line threw (IndexOutOfRange / FormatException); such lines are skipped here.
        if (Values.size() < 2) continue;
        int32_t value = 0;
        if (!Text::TryParseInt(Values[1], value)) continue;

        auto name = Text::ToLower(Values[0]);
        if (Constants.find(name) == Constants.end()) Constants.emplace(name, value); //the repeats are just labels for locations (usually called gotit)
    }
}

}}} // namespace FSO::Files::HIT
