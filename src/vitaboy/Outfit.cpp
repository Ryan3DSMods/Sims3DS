/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.Vitaboy.Outfit (sims.common/rendering/vitaboy/Outfit.cs)
#include "vitaboy/Outfit.h"
#include "core/io.h"
#include "files/formats/iff/chunks/STR.h"

namespace FSO { namespace Vitaboy {

using FSO::Common::Content::ContentID;

void Outfit::PrepareDefaultHandgroup(const std::string& meshCode) {
    LiteralHandgroup = std::make_shared<FSO::Vitaboy::HandGroup>();
    LiteralHandgroup->TS1HandSet = true;
    auto& l = LiteralHandgroup->LightSkin;
    l.LeftHand.Idle.Name = "H" + meshCode + "LO.apr"; l.LeftHand.Idle.TexName = "huaolgt";
    l.LeftHand.Pointing.Name = "H" + meshCode + "LP.apr"; l.LeftHand.Pointing.TexName = "huaplgt";
    l.LeftHand.Fist.Name = "H" + meshCode + "LC.apr"; l.LeftHand.Fist.TexName = "huaclgt";
    l.RightHand.Idle.Name = "H" + meshCode + "RO.apr"; l.RightHand.Idle.TexName = "huaolgt";
    l.RightHand.Pointing.Name = "H" + meshCode + "RP.apr"; l.RightHand.Pointing.TexName = "huaplgt";
    l.RightHand.Fist.Name = "H" + meshCode + "RC.apr"; l.RightHand.Fist.TexName = "huaclgt";
}

ContentID Outfit::GetAppearance(AppearanceType type) const {
    if (!TS1AppearanceID.empty()) return ContentID(TS1AppearanceID);
    switch (type) {
        case AppearanceType::Light:
            return ContentID(LightAppearanceTypeID, LightAppearanceFileID);
        case AppearanceType::Medium:
            return ContentID(MediumAppearanceTypeID, MediumAppearanceFileID);
        case AppearanceType::Dark:
            return ContentID(DarkAppearanceTypeID, DarkAppearanceFileID);
    }
    return ContentID();
}

void Outfit::Read(const std::vector<uint8_t>& data) {
    st::Reader io(data, st::Endian::Big);
    auto version = io.ReadUInt32(); (void)version;
    auto unknown = io.ReadUInt32(); (void)unknown;

    LightAppearanceFileID = io.ReadUInt32();
    LightAppearanceTypeID = io.ReadUInt32();

    MediumAppearanceFileID = io.ReadUInt32();
    MediumAppearanceTypeID = io.ReadUInt32();

    DarkAppearanceFileID = io.ReadUInt32();
    DarkAppearanceTypeID = io.ReadUInt32();

    HandGroup = io.ReadUInt32();
    Region = io.ReadUInt32();
}

void Outfit::ReadHead(FSO::Files::Formats::IFF::Chunks::STR* bodyStrings) {
    ReadHead(bodyStrings->GetString(2));
}

void Outfit::ReadHead(const std::string& headString) {
    TS1AppearanceID = ToApr(headString);
    TS1TextureID = ToTex(headString);
}

void Outfit::Read(FSO::Files::Formats::IFF::Chunks::STR* bodyStrings) {
    std::string hands[6];
    for (int i = 0; i < 6; i++) hands[i] = bodyStrings->GetString(17 + i);
    Read(bodyStrings->GetString(1), hands);
}

void Outfit::Read(const std::string& bodyString, const std::string handStrings[6]) {
    // var bodies = bodyStrings.GetString(1).Split(';'); bodies.FirstOrDefault()
    auto first = bodyString.substr(0, bodyString.find(';'));
    TS1AppearanceID = ToApr(first);
    TS1TextureID = ToTex(first);

    LiteralHandgroup = std::make_shared<FSO::Vitaboy::HandGroup>();
    //TS1HandSet = true,
    auto& l = LiteralHandgroup->LightSkin;
    l.LeftHand.Idle.Name = ToApr(handStrings[0]); l.LeftHand.Idle.TexName = ToTex(handStrings[0]);         // 17
    l.LeftHand.Pointing.Name = ToApr(handStrings[2]); l.LeftHand.Pointing.TexName = ToTex(handStrings[2]); // 19
    l.LeftHand.Fist.Name = ToApr(handStrings[4]); l.LeftHand.Fist.TexName = ToTex(handStrings[4]);         // 21
    l.RightHand.Idle.Name = ToApr(handStrings[1]); l.RightHand.Idle.TexName = ToTex(handStrings[1]);         // 18
    l.RightHand.Pointing.Name = ToApr(handStrings[3]); l.RightHand.Pointing.TexName = ToTex(handStrings[3]); // 20
    l.RightHand.Fist.Name = ToApr(handStrings[5]); l.RightHand.Fist.TexName = ToTex(handStrings[5]);         // 22
}

std::string Outfit::ToApr(const std::string& input) {
    return input.substr(0, input.find(',')) + ".apr";
}

std::string Outfit::ToTex(const std::string& input) {
    //assume there is one texture bound to the appearance
    auto eqIdx = input.find('=');
    if (eqIdx == std::string::npos) return std::string();
    return input.substr(eqIdx + 1);
}

}}
