/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Engine.VMAmbientSound — handles ambient sound in lots.
// The ambience tables are translated; STUB(3ds): AmbiencePlayer (HIT/XA playback) does nothing yet.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>
namespace FSO { namespace SimAntics { namespace Engine {

// STUB(3ds): TSO.HIT.Ambience / AmbiencePlayer
struct Ambience {
    std::string Path;
    bool Loop = false;
    Ambience() {}
    Ambience(const std::string& path, bool loop) : Path(path), Loop(loop) {}
};
class AmbiencePlayer {
public:
    explicit AmbiencePlayer(const Ambience& amb) : Amb(amb) {}
    Ambience Amb;
    void Kill() {} // STUB(3ds)
};

struct VMCategorisedAmb {
    uint32_t GUID;
    uint8_t Category;
    std::string Name;
    VMCategorisedAmb(uint32_t guid, uint8_t cat, const std::string& name) : GUID(guid), Category(cat), Name(name) {}
};

class VMAmbientSound {
public:
    static const std::map<uint32_t, Ambience>& AmbienceByGUID(); //may want to load this from ambience.ini in future...
    static const std::vector<VMCategorisedAmb>& SoundByBitField();

    std::map<uint8_t, std::unique_ptr<AmbiencePlayer>> ActiveSounds;

    /// Handles ambient sound in lots.
    VMAmbientSound() {}
    bool AmbienceActive(uint8_t id) { return ActiveSounds.count(id) > 0; }
    uint8_t GetAmbienceFromGUID(uint32_t GUID);
    void SetAmbience(uint8_t id, bool active);
    void Kill();
};
}}}
