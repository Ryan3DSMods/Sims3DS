/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.Vitaboy.Outfit (sims.common/rendering/vitaboy/Outfit.cs)
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "vitaboy/AppearanceType.h"
#include "vitaboy/ContentID.h"
#include "vitaboy/HandGroup.h"

namespace FSO { namespace Files { namespace Formats { namespace IFF { namespace Chunks { class STR; }}}}}

namespace FSO { namespace Vitaboy {

/// <summary>
/// Outfits collect together the light-, medium-, and dark-skinned versions of an
/// appearance and associate them collectively with a hand group and a body region (head or body).
/// TS1: built from a person's body strings (STR#200: "suit,BODY=texture").
/// </summary>
class Outfit {
public:
    uint32_t LightAppearanceFileID = 0;
    uint32_t LightAppearanceTypeID = 0;

    uint32_t MediumAppearanceFileID = 0;
    uint32_t MediumAppearanceTypeID = 0;

    uint32_t DarkAppearanceFileID = 0;
    uint32_t DarkAppearanceTypeID = 0;

    uint32_t HandGroup = 0;
    uint32_t Region = 0;

    std::string TS1AppearanceID; // empty == C# null
    std::string TS1TextureID;    // empty == C# null
    std::shared_ptr<FSO::Vitaboy::HandGroup> LiteralHandgroup;

    // 3DS addition: the uint64 id the VM uses for this outfit (VMAvatar.BodyOutfit/HeadOutfit).
    uint64_t ID = 0;
    std::string Name;

    void PrepareDefaultHandgroup(const std::string& meshCode = "m");

    /// <summary>
    /// Gets the ContentID for the appearances referenced by this Outfit.
    /// </summary>
    FSO::Common::Content::ContentID GetAppearance(AppearanceType type) const;

    /// <summary>
    /// Gets the ContentID for the Handgroup referenced by this Outfit.
    /// </summary>
    FSO::Common::Content::ContentID GetHandgroup() const { return FSO::Common::Content::ContentID((uint32_t)18, HandGroup); }

    /// <summary>
    /// Reads a TSO Outfit (.oft, big endian).
    /// </summary>
    void Read(const std::vector<uint8_t>& data);

    /// TS1 head from body strings (string 2).
    void ReadHead(FSO::Files::Formats::IFF::Chunks::STR* bodyStrings);
    /// TS1 body + hands from body strings (string 1; hands 17..22).
    void Read(FSO::Files::Formats::IFF::Chunks::STR* bodyStrings);

    /// The string-level forms of the above (also used by tools without an IFF).
    void ReadHead(const std::string& headString);
    void Read(const std::string& bodyString, const std::string handStrings[6]);

    static std::string ToApr(const std::string& input);
    static std::string ToTex(const std::string& input);
};

}}
