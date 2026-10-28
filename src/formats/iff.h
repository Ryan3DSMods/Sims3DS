/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// IFF container reader (The Sims 1 "IFF FILE 2.5" / 2.0).
// Reference: FreeSims sims.files/formats/iff/IffFile.cs and chunk classes.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <map>

namespace st {

struct Color { uint8_t r=0,g=0,b=0,a=255; };

struct Chunk {
    std::string type;     // 4 chars, e.g. "SPR2", "PALT", "OBJD"
    uint16_t id = 0;
    uint16_t flags = 0;
    std::string label;
    std::vector<uint8_t> data;  // raw body (big-endian container stripped)
};

// ---- Typed views (parsed on demand from a Chunk's raw body) ----

struct Palt {
    Color colors[256];
    bool parse(const Chunk& c);
};

struct Spr2Frame {
    int width = 0, height = 0;
    uint32_t flags = 0;
    uint16_t paletteID = 0;
    uint16_t transparentIndex = 0;
    int posX = 0, posY = 0;
    std::vector<Color> pixels;   // width*height RGBA, valid after decode()
    std::vector<uint8_t> zbuf;   // optional
    std::vector<uint8_t> palIdx; // palette indices
};

struct Spr2 {
    uint32_t defaultPalette = 0;
    std::vector<Spr2Frame> frames;
    // Decodes all frames using palettes found in `owner`.
    bool parse(const Chunk& c, class IffFile& owner);
};

struct StrSet {
    std::vector<std::string> strings;
    bool parse(const Chunk& c);
};

struct DgrpSprite {
    uint32_t spriteID = 0;       // SPR2 chunk id
    uint32_t frameIndex = 0;
    int32_t  offX = 0, offY = 0; // pixel offset within the tile
    uint32_t flags = 0;          // bit0 = flip horizontally
    bool flip() const { return (flags & 0x1) != 0; }
};
struct DgrpImage {               // one drawgroup image (per direction+zoom)
    uint32_t direction = 0;      // 0x01/0x04/0x10/0x40
    uint32_t zoom = 0;           // 0=far 1=mid 2=near (small..large)
    std::vector<DgrpSprite> sprites;
};
struct Dgrp {
    std::vector<DgrpImage> images;
    bool parse(const Chunk& c);
    const DgrpImage* get(uint32_t direction, uint32_t zoom) const;
};

struct Objd {                    // just the header fields we need so far
    uint16_t version = 0;
    uint16_t stackSize = 0;
    uint16_t baseGraphicID = 0;
    uint16_t numGraphics = 0;
    uint32_t guid = 0;
    uint16_t price = 0;
    std::string name;            // from the file's STR# where available
    bool parse(const Chunk& c);
};

// BHAV: SimAntics behaviour bytecode.
struct BhavInstruction {
    uint16_t opcode = 0;
    uint8_t  truePtr = 0;
    uint8_t  falsePtr = 0;
    uint8_t  operand[8] = {0,0,0,0,0,0,0,0};
};
struct Bhav {
    uint8_t  type = 0, args = 0;
    uint16_t locals = 0, flags = 0;
    std::vector<BhavInstruction> insns;
    bool parse(const Chunk& c);
};

// TTAB: the object's interaction (pie-menu) table.
struct MotiveAd { int16_t min = 0, max = 0; }; // advertised effect range for a motive
struct TtabInteraction {
    uint16_t actionBHAV = 0;   // tree to run when selected
    uint16_t testBHAV = 0;     // guard tree (availability)
    uint32_t flags = 0;
    uint32_t ttaIndex = 0;     // name index into the TTAs string table
    uint32_t autonomyThreshold = 0;
    std::vector<MotiveAd> motives;   // indexed by motive id (Hunger=7, Energy=5, ...)
};
struct Ttab {
    std::vector<TtabInteraction> interactions;
    bool fieldEncoded = false; // rare v9/10 bit-packed form (not decoded here)
    bool parse(const Chunk& c);
};

// ARRY: RLE-compressed lot grid (floors/walls/altitude/objects).
enum class ArryType { Floor=1, Objects=2, Altitude=4, Walls=8 };
struct Arry {
    int width=0, height=0;
    int type=0;                  // element byte size == (int)type
    std::vector<uint8_t> data;   // width*height*type bytes, row-major
    bool parse(const Chunk& c);
    // floor index (type==Floor) at cell
    uint8_t floorAt(int x,int y) const { return data[(size_t)(y*width+x)]; }
    // altitude (type==Altitude) low byte at cell
    int32_t altAt(int x,int y) const {
        size_t o=(size_t)(y*width+x)*4; return (int32_t)(data[o]|(data[o+1]<<8)|(data[o+2]<<16)|(data[o+3]<<24)); }
};

class IffFile {
public:
    bool open(const std::string& path);
    bool openBytes(std::vector<uint8_t> bytes, const std::string& name = "");

    const std::vector<Chunk>& chunks() const { return chunks_; }
    std::vector<const Chunk*> byType(const std::string& type) const;
    const Chunk* get(const std::string& type, uint16_t id) const;
    int count(const std::string& type) const;

    const std::string& filename() const { return name_; }

private:
    bool parse();
    std::vector<uint8_t> buf_;
    std::vector<Chunk> chunks_;
    std::map<std::pair<std::string,uint16_t>, size_t> index_;
    std::string name_;
};

} // namespace st
