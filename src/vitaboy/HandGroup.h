/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.Vitaboy.HandGroup / HandSet / Hand / Gesture (sims.common/rendering/vitaboy/HandGroup.cs)
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "vitaboy/ContentID.h"

namespace FSO { namespace Vitaboy {

/// <summary>
/// A gesture points to a purchasable outfit, which can be used to access the mesh and
/// texture for the gesture. TS1: Name is the appearance file ("HmLO.apr"), TexName the texture.
/// </summary>
class Gesture {
public:
    uint32_t FileID = 0;
    uint32_t TypeID = 0;
    std::string Name;    // empty == C# null
    std::string TexName; // empty == C# null

    FSO::Common::Content::ContentID ID() const {
        // TS1 gestures are addressed by name (FreeSO: ContentID(Name)); TSO by type/file.
        if (!Name.empty()) return FSO::Common::Content::ContentID(Name);
        return FSO::Common::Content::ContentID(TypeID, FileID);
    }
};

/// <summary>
/// A hand is a collection of gestures.
/// </summary>
class Hand {
public:
    Gesture Idle;
    Gesture Fist;
    Gesture Pointing;
};

/// <summary>
/// A hand set is a set of hands (left and right) for an appearance (light, dark or medium).
/// </summary>
class HandSet {
public:
    Hand LeftHand;
    Hand RightHand;
};

/// <summary>
/// Hand groups collect together the various hand gesture appearances of the game.
/// There are always exactly 18 appearances referenced in a hand group (3 skin colors x 2 hands x 3 gestures).
/// </summary>
class HandGroup {
public:
    bool TS1HandSet = false;
    HandSet LightSkin;
    HandSet MediumSkin;
    HandSet DarkSkin;

    /// <summary>
    /// Reads a TSO Handgroup (.hag, big endian).
    /// </summary>
    void Read(const std::vector<uint8_t>& data);
private:
    uint32_t m_Version = 0;
};

}}
