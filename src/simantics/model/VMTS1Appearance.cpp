/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// TS1(sims): see VMTS1Appearance.h
#include "simantics/model/VMTS1Appearance.h"
#include "files/formats/iff/chunks/STR.h"
#include <cstdlib>

namespace FSO { namespace SimAntics { namespace Model {

VMTS1BodyPart VMTS1BodyPart::Parse(const std::string& str)
{
    VMTS1BodyPart result;
    auto comma = str.find(',');
    result.Mesh = str.substr(0, comma);
    if (comma == std::string::npos) return result;
    auto rest = str.substr(comma + 1);
    auto eq = rest.find('=');
    result.Binding = rest.substr(0, eq);
    if (eq != std::string::npos) result.Texture = rest.substr(eq + 1);
    return result;
}

VMTS1Appearance VMTS1Appearance::FromBodyStrings(FSO::Files::Formats::IFF::Chunks::STR* data)
{
    VMTS1Appearance result;
    if (data == nullptr) return result;
    for (int32_t i = 0; i < data->Length(); i++) result.Raw.push_back(data->GetString(i));
    auto get = [&](size_t i) -> std::string { return (i < result.Raw.size()) ? result.Raw[i] : std::string(); };
    result.AgeType = get(0);
    result.Body = VMTS1BodyPart::Parse(get(1));
    result.Head = VMTS1BodyPart::Parse(get(2));
    result.HeadBone = get(3);
    result.HeadAttach = get(4);
    result.HandBone = get(5);
    result.HandAttach = get(6);
    result.Gender = get(12);
    result.Age = atoi(get(13).c_str());
    result.SkinTone = get(14);
    result.NakedBody = VMTS1BodyPart::Parse(get(15));
    result.Underwear = VMTS1BodyPart::Parse(get(16));
    for (size_t h = 0; h < 6; h++) result.Hands[h] = VMTS1BodyPart::Parse(get(17 + h));
    result.FormalBody = VMTS1BodyPart::Parse(get(27));
    result.Valid = !result.Body.Empty() || !result.Head.Empty();
    return result;
}

}}}
