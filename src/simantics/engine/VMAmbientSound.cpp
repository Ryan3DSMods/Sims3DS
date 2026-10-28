/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Engine.VMAmbientSound
#include "simantics/engine/VMAmbientSound.h"
namespace FSO { namespace SimAntics { namespace Engine {

const std::map<uint32_t, Ambience>& VMAmbientSound::AmbienceByGUID() {
    static const std::map<uint32_t, Ambience> table = {
        {0x3dd887a6, Ambience("sounddata/ambience/daybirds/daybirds.fsc", false)},
        {0x3dd887aa, Ambience("sounddata/ambience/explosions/explosions.fsc", false)},
        {0x7dd887ad, Ambience("sounddata/ambience/farmanimals/farmanimals.fsc", false)},
        {0x9dd887af, Ambience("sounddata/ambience/gunshots/gunshots.fsc", false)},
        {0xddd887b3, Ambience("sounddata/ambience/planes/planes.fsc", false)},
        {0xfdd887b5, Ambience("sounddata/ambience/thunder/thunder.fsc", false)},
        {0x1e128187, Ambience("sounddata/ambience/breeze/breeze.fsc", false)},
        {0xfe128189, Ambience("sounddata/ambience/construction/construction.fsc", false)},
        {0x5e12818c, Ambience("sounddata/ambience/dog/dog.fsc", false)},
        {0xbe12818d, Ambience("sounddata/ambience/driveby/driveby.fsc", false)},
        {0xde12818f, Ambience("sounddata/ambience/howlingwind/howlingwind.fsc", false)},
        {0x1e128190, Ambience("sounddata/ambience/indust/indust.fsc", false)},
        {0x3e128192, Ambience("sounddata/ambience/insect/insect.fsc", false)},
        {0xbe128196, Ambience("sounddata/ambience/jungle/jungle.fsc", false)},
        {0xde128198, Ambience("sounddata/ambience/office/office.fsc", false)},
        {0x3e12819a, Ambience("sounddata/ambience/restaurant/restaurant.fsc", false)},
        {0xbe12819c, Ambience("sounddata/ambience/scibleeps/scibleeps.fsc", false)},
        {0x1e1281ac, Ambience("sounddata/ambience/siren/siren.fsc", false)},
        {0x1e1281ad, Ambience("sounddata/ambience/wolf/wolf.fsc", false)},
        {0xbe19bb2d, Ambience("sounddata/ambience/seabirds/seabirds.fsc", false)},
        {0xde19bb31, Ambience("sounddata/ambience/raindrops/raindrops.fsc", false)},
        {0xbe1a033e, Ambience("sounddata/ambience/magic/magic.fsc", false)},
        {0xa9b9652a, Ambience("sounddata/ambience/smallmachines/smallmachines.fsc", false)},
        {0xa9b96536, Ambience("sounddata/ambience/screams/screams.fsc", false)},
        {0xa9b96539, Ambience("sounddata/ambience/nightbirds/nightbirds.fsc", false)},
        {0xa9b9653c, Ambience("sounddata/ambience/gym/gym.fsc", false)},
        {0xa9b9653e, Ambience("sounddata/ambience/ghost/ghost.fsc", false)},
        {0x9e0bc19a, Ambience("sounddata/ambience/loops/brook_lp.xa", true)},
        {0xfe0bc1a1, Ambience("sounddata/ambience/loops/crowd_lp.xa", true)},
        {0x1e0bc1a3, Ambience("sounddata/ambience/loops/heartbeat_lp.xa", true)},
        {0x5e0bc1a4, Ambience("sounddata/ambience/loops/indoor_lp.xa", true)},
        {0x5e0bc1a6, Ambience("sounddata/ambience/loops/insect_lp.xa", true)},
        {0xbe0bc1a9, Ambience("sounddata/ambience/loops/ocean_lp.xa", true)},
        {0x1e0bc1ab, Ambience("sounddata/ambience/loops/outdoor_lp.xa", true)},
        {0xde0bc1ad, Ambience("sounddata/ambience/loops/rain_lp.xa", true)},
        {0x3e0bc2af, Ambience("sounddata/ambience/loops/scifi_lp.xa", true)},
        {0x1e0bc2b2, Ambience("sounddata/ambience/loops/storm_lp.xa", true)},
        {0x3e0bc2b4, Ambience("sounddata/ambience/loops/traffic_lp.xa", true)},
        {0x1e0bc2b5, Ambience("sounddata/ambience/loops/wind_lp.xa", true)},
    };
    return table;
}

const std::vector<VMCategorisedAmb>& VMAmbientSound::SoundByBitField() {
    static const std::vector<VMCategorisedAmb> table = {
        VMCategorisedAmb(0x3dd887a6, 0, "AnimalsSongBirds"),
        VMCategorisedAmb(0x3dd887aa, 1, "MechanicalExplosions"),
        VMCategorisedAmb(0x7dd887ad, 0, "AnimalsFarm"),
        VMCategorisedAmb(0x9dd887af, 1, "MechanicalGunshot"),
        VMCategorisedAmb(0xddd887b3, 1, "MechanicalPlanes"),
        VMCategorisedAmb(0xfdd887b5, 2, "WeatherLightingThunder"),
        VMCategorisedAmb(0x9e0bc19a, 4, "LoopBrook"),
        VMCategorisedAmb(0xfe0bc1a1, 4, "LoopCrowd"),
        VMCategorisedAmb(0x1e0bc1a3, 4, "LoopHeartbeat"),
        VMCategorisedAmb(0x5e0bc1a4, 4, "LoopIndoor"),
        VMCategorisedAmb(0x5e0bc1a6, 4, "LoopInsects"),
        VMCategorisedAmb(0xbe0bc1a9, 4, "LoopOcean"),
        VMCategorisedAmb(0x1e0bc1ab, 4, "LoopOutdoor"),
        VMCategorisedAmb(0xde0bc1ad, 4, "LoopRain"),
        VMCategorisedAmb(0x3e0bc2af, 4, "LoopTechno"),
        VMCategorisedAmb(0x1e0bc2b2, 4, "LoopStorm"),
        VMCategorisedAmb(0x3e0bc2b4, 4, "LoopTraffic"),
        VMCategorisedAmb(0x1e0bc2b5, 4, "LoopWind"),
        VMCategorisedAmb(0x1e128187, 2, "WeatherBreeze"),
        VMCategorisedAmb(0xfe128189, 1, "MechanicalConstruction"),
        VMCategorisedAmb(0x5e12818c, 0, "AnimalsDog"),
        VMCategorisedAmb(0xbe12818d, 1, "MechanicalDriveBy"),
        VMCategorisedAmb(0xde12818f, 2, "WeatherHowlingWind"),
        VMCategorisedAmb(0x1e128190, 1, "MechanicalIndustrial"),
        VMCategorisedAmb(0x3e128192, 0, "AnimalsInsects"),
        VMCategorisedAmb(0xbe128196, 0, "AnimalsJungle"),
        VMCategorisedAmb(0xde128198, 3, "PeopleOffice"),
        VMCategorisedAmb(0x3e12819a, 3, "PeopleRestaurant"),
        VMCategorisedAmb(0xbe12819c, 1, "MechanicalSciBleeps"),
        VMCategorisedAmb(0x1e1281ac, 1, "MechanicalSirens"),
        VMCategorisedAmb(0x1e1281ad, 0, "AnimalsWolf"),
        VMCategorisedAmb(0xbe19bb2d, 0, "AnimalsSeaBirds"),
        VMCategorisedAmb(0xde19bb31, 2, "WeatherRainDrops"),
        VMCategorisedAmb(0xbe1a033e, 3, "PeopleMagic"),
        VMCategorisedAmb(0xa9b9652a, 1, "MechanicalSmallMachines"),
        VMCategorisedAmb(0xa9b96536, 3, "PeopleScreams"),
        VMCategorisedAmb(0xa9b96539, 0, "AnimalsNightBirds"),
        VMCategorisedAmb(0xa9b9653c, 3, "PeopleGym"),
        VMCategorisedAmb(0xa9b9653e, 3, "PeopleGhost"),
    };
    return table;
}

uint8_t VMAmbientSound::GetAmbienceFromGUID(uint32_t GUID) {
    auto& list = SoundByBitField();
    for (uint8_t i = 0; i < list.size(); i++) {
        if (list[i].GUID == GUID) return i;
    }
    return 0;
}

void VMAmbientSound::SetAmbience(uint8_t id, bool active) {
    if (active) {
        if (!ActiveSounds.count(id)) {
            if (id >= SoundByBitField().size()) return;
            auto& cat = SoundByBitField()[id];
            auto it = AmbienceByGUID().find(cat.GUID);
            if (it == AmbienceByGUID().end()) return;
            ActiveSounds[id].reset(new AmbiencePlayer(it->second));
        }
    } else {
        auto it = ActiveSounds.find(id);
        if (it != ActiveSounds.end()) {
            it->second->Kill();
            ActiveSounds.erase(it);
        }
    }
}

void VMAmbientSound::Kill() {
    for (auto& sound : ActiveSounds) {
        sound.second->Kill();
    }
    ActiveSounds.clear();
}
}}}
