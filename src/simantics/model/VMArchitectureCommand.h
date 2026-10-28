/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// FSO.SimAntics.Model.VMArchitectureCommand (serialization dropped)
#pragma once
#include <cstdint>
namespace FSO { namespace SimAntics { namespace Model {
enum class VMArchitectureCommandType : uint8_t {
    WALL_LINE, WALL_DELETE, WALL_RECT, PATTERN_DOT, PATTERN_FILL, FLOOR_RECT, FLOOR_FILL,
    TERRAIN_RAISE, TERRAIN_FLATTEN, // TS1(sims): FreeSO
    GRASS_DOT
};
struct VMArchitectureCommand {
    VMArchitectureCommandType Type = VMArchitectureCommandType::WALL_LINE;
    uint32_t CallerUID = 0;
    int32_t x = 0;
    int32_t y = 0;
    int8_t level = 0;
    int32_t x2 = 0; //for RECT: width and height. for LINE: length and direction. Not important for fill.
    int32_t y2 = 0;
    //note: for pattern dot x2 is "side". 0-5, 0-3 for normal walls and 4-5 for diagonal sides
    uint16_t pattern = 0;
    uint16_t style = 0; //for walls, obvious. maybe means something else for floors on diagonals
};
}}}
