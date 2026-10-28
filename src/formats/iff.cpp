/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
#include "iff.h"
#include "../core/io.h"
#include <cstdio>
#include <algorithm>

namespace st {

static std::vector<uint8_t> slurp(const std::string& path) {
    std::vector<uint8_t> v;
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return v;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n > 0) { v.resize((size_t)n); if (fread(v.data(),1,(size_t)n,f)!=(size_t)n) v.clear(); }
    fclose(f);
    return v;
}

bool IffFile::open(const std::string& path) {
    buf_ = slurp(path);
    if (buf_.empty()) return false;
    size_t slash = path.find_last_of("/\\");
    name_ = (slash==std::string::npos) ? path : path.substr(slash+1);
    return parse();
}

bool IffFile::openBytes(std::vector<uint8_t> bytes, const std::string& name) {
    buf_ = std::move(bytes); name_ = name;
    return parse();
}

bool IffFile::parse() {
    chunks_.clear(); index_.clear();
    try {
        Reader r(buf_, Endian::Big);
        std::string ident = r.cstr(60, false);
        ident.erase(std::remove(ident.begin(), ident.end(), '\0'), ident.end());
        const std::string A = "IFF FILE 2.5:TYPE FOLLOWED BY SIZE JAMIE DOORNBOS & MAXIS 1";
        const std::string B = "IFF FILE 2.0:TYPE FOLLOWED BY SIZE JAMIE DOORNBOS & MAXIS 1";
        if (ident != A && ident != B) return false;

        (void) r.u32(); // rsmp offset

        while (r.has_more()) {
            if (r.pos() + 76 > r.size()) break;
            Chunk c;
            c.type  = r.cstr(4, false);
            uint32_t size = r.u32();
            c.id    = r.u16();
            c.flags = r.u16();
            c.label = r.cstr(64, true);
            long bodySize = (long)size - 76;
            if (bodySize < 0) bodySize = 0;
            // Guard against malformed sizes (walls2.iff in the wild etc.)
            size_t avail = r.size() - r.pos();
            if ((size_t)bodySize > avail) bodySize = (long)avail;
            c.data = r.bytes((size_t)bodySize);

            auto key = std::make_pair(c.type, c.id);
            if (!index_.count(key)) index_[key] = chunks_.size();
            chunks_.push_back(std::move(c));
        }
        return true;
    } catch (...) {
        chunks_.clear(); index_.clear();
        return false;
    }
}

std::vector<const Chunk*> IffFile::byType(const std::string& type) const {
    std::vector<const Chunk*> out;
    for (auto& c : chunks_) if (c.type == type) out.push_back(&c);
    return out;
}

const Chunk* IffFile::get(const std::string& type, uint16_t id) const {
    auto it = index_.find(std::make_pair(type, id));
    if (it == index_.end()) return nullptr;
    return &chunks_[it->second];
}

int IffFile::count(const std::string& type) const {
    int n = 0; for (auto& c : chunks_) if (c.type == type) n++; return n;
}

// ---------------- PALT ----------------
bool Palt::parse(const Chunk& c) {
    try {
        Reader r(c.data, Endian::Little);
        (void) r.u32();               // version
        uint32_t num = r.u32();
        (void) r.bytes(8);            // reserved
        if (num > 256) num = 256;
        for (uint32_t i = 0; i < num; i++) {
            Color col; col.r = r.u8(); col.g = r.u8(); col.b = r.u8(); col.a = 255;
            colors[i] = col;
        }
        return true;
    } catch (...) { return false; }
}

// ---------------- STR# ----------------
// The Sims STR# has several formats; format code is a signed int16.
bool StrSet::parse(const Chunk& c) {
    try {
        Reader r(c.data, Endian::Little);
        int16_t format = r.s16();
        auto readPascal = [&](Reader& rr) {
            uint8_t len = rr.u8();
            return rr.cstr(len, false);
        };
        if (format == 0) {
            uint16_t n = r.u16();
            for (uint16_t i=0;i<n;i++) strings.push_back(readPascal(r));
        } else if (format == -1) {
            uint16_t n = r.u16();
            for (uint16_t i=0;i<n;i++){ strings.push_back(readPascal(r)); readPascal(r); }
        } else if (format == -2) { // null-terminated pairs
            uint16_t n = r.u16();
            auto readNull = [&](Reader& rr){ std::string s; while(rr.has_more()){char ch=(char)rr.u8(); if(!ch)break; s+=ch;} return s; };
            for (uint16_t i=0;i<n;i++){ readNull(r); strings.push_back(readNull(r)); }
        } else if (format == -3) { // lang-indexed
            uint16_t n = r.u16();
            auto readNull = [&](Reader& rr){ std::string s; while(rr.has_more()){char ch=(char)rr.u8(); if(!ch)break; s+=ch;} return s; };
            for (uint16_t i=0;i<n;i++){ r.u8(); strings.push_back(readNull(r)); readNull(r); }
        } else {
            return false;
        }
        return true;
    } catch (...) { return false; }
}

// ---------------- OBJD ----------------
bool Objd::parse(const Chunk& c) {
    try {
        Reader r(c.data, Endian::Little);
        version = r.u16();
        // field layout per FreeSims OBJD.cs (version-dependent length; we read
        // the stable leading header only).
        stackSize     = r.u16();
        baseGraphicID = r.u16();
        numGraphics   = r.u16();
        r.u16(); // BHAV_MainID
        r.u16(); // BHAV_GardeningID
        r.u16(); // TreeTableID
        r.s16(); // InteractionGroupID
        r.u16(); // ObjectType
        r.u16(); // MasterID
        r.s16(); // SubIndex
        r.u16(); // BHAV_WashHandsID
        r.u16(); // AnimationTableID
        guid = r.u32();
        r.u16(); // Disabled
        r.u16(); // BHAV_Portal
        price = r.u16();
        return true;
    } catch (...) { return false; }
}

// ---------------- DGRP ----------------
bool Dgrp::parse(const Chunk& c) {
    try {
        Reader r(c.data, Endian::Little);
        uint16_t version = r.u16();
        uint32_t imageCount = (version < 20003) ? r.u16() : r.u32();
        images.clear();
        for (uint32_t i=0;i<imageCount;i++) {
            DgrpImage im;
            uint32_t spriteCount;
            if (version < 20003) {
                spriteCount  = r.u16();
                im.direction = r.u8();
                im.zoom      = r.u8();
            } else {
                im.direction = r.u32();
                im.zoom      = r.u32();
                spriteCount  = r.u32();
            }
            for (uint32_t s=0;s<spriteCount;s++) {
                DgrpSprite sp;
                if (version < 20003) {
                    (void) r.u16();                 // type (ignored)
                    sp.spriteID   = r.u16();
                    sp.frameIndex = r.u16();
                    sp.flags      = r.u16();
                    sp.offX       = r.s16();
                    sp.offY       = r.s16();
                    if (version == 20001) (void) r.u32(); // ObjectOffset.Z float
                } else {
                    sp.spriteID   = r.u32();
                    sp.frameIndex = r.u32();
                    sp.offX       = r.s32();
                    sp.offY       = r.s32();
                    (void) r.u32();                 // ObjectOffset.Z float
                    sp.flags      = r.u32();
                    if (version == 20004) { (void) r.u32(); (void) r.u32(); } // X,Y floats
                }
                im.sprites.push_back(sp);
            }
            images.push_back(std::move(im));
        }
        return true;
    } catch (...) { return false; }
}

const DgrpImage* Dgrp::get(uint32_t direction, uint32_t zoom) const {
    for (auto& im : images) if (im.direction==direction && im.zoom==zoom) return &im;
    return nullptr;
}

// ---------------- BHAV ----------------
bool Bhav::parse(const Chunk& c) {
    try {
        Reader r(c.data, Endian::Little);
        uint16_t version = r.u16();
        uint32_t count = 0;
        if (version == 0x8000)      { count = r.u16(); r.skip(8); }
        else if (version == 0x8001) { count = r.u16(); r.skip(8); }
        else if (version == 0x8002) { count = r.u16(); type=r.u8(); args=r.u8(); locals=r.u16(); flags=r.u16(); r.skip(2); }
        else if (version == 0x8003) { type=r.u8(); args=r.u8(); locals=r.u8(); r.skip(2); flags=r.u16(); count=r.u32(); }
        else return false;
        insns.clear(); insns.reserve(count);
        for (uint32_t i=0;i<count;i++) {
            BhavInstruction ins;
            ins.opcode  = r.u16();
            ins.truePtr = r.u8();
            ins.falsePtr= r.u8();
            for (int k=0;k<8;k++) ins.operand[k]=r.u8();
            insns.push_back(ins);
        }
        return true;
    } catch (...) { return false; }
}

// ---------------- TTAB ----------------
// MSB-first bit reader for the field-encoded (v9/10) TTAB form.
namespace { struct BitReader {
    Reader& r; uint8_t cur=0, bitPos=0;
    BitReader(Reader& rr):r(rr){ cur = r.has_more()? r.u8() : 0; }
    uint8_t bit(){ uint8_t b=(cur>>(7-bitPos))&1; if(++bitPos>7){bitPos=0; cur = r.has_more()? r.u8() : 0;} return b; }
    uint32_t bits(int n){ uint32_t t=0; for(int i=0;i<n;i++) t += (uint32_t)bit() << ((n-i)-1); return t; }
    long field(bool big){ if(bit()==0) return 0; uint32_t code=bits(2);
        static const uint8_t w[4]={5,8,13,16}, w2[4]={6,11,21,32};
        uint8_t width = big? w2[code]:w[code];
        long v = bits(width); v |= -(v & (1L<<(width-1))); return v; }
    uint16_t u16(){ return (uint16_t)field(false); }
    int16_t  i16(){ return (int16_t)field(false); }
    uint32_t u32(){ return (uint32_t)field(true); }
    int32_t  i32(){ return (int32_t)field(true); }
}; }

bool Ttab::parse(const Chunk& c) {
    try {
        Reader r(c.data, Endian::Little);
        uint16_t count = r.u16();
        interactions.clear();
        if (count == 0) return true;
        uint16_t version = r.u16();
        bool enc = (version==9 || version==10);
        BitReader* br = nullptr;
        if (enc) { fieldEncoded = true; uint8_t cc=r.u8(); if(cc!=1) return false; br = new BitReader(r); }

        auto U16=[&](){ return enc? br->u16() : r.u16(); };
        auto U32=[&](){ return enc? br->u32() : r.u32(); };
        auto I16=[&](){ return enc? br->i16() : r.s16(); };
        auto I32=[&](){ return enc? br->i32() : r.s32(); };

        for (uint16_t i=0;i<count;i++) {
            TtabInteraction t;
            t.actionBHAV = U16();
            t.testBHAV   = U16();
            uint32_t motiveCount = U32();
            t.flags      = U32();
            t.ttaIndex   = U32();
            if (version > 6) (void) U32();     // attenuation code
            (void) U32();                      // attenuation value (float)
            t.autonomyThreshold = U32();       // autonomy threshold
            (void) I32();                      // joining index
            if (motiveCount > 64) motiveCount = 0; // guard against desync
            for (uint32_t j=0;j<motiveCount;j++) {
                MotiveAd m;
                if (version > 6) m.min = I16();
                m.max = I16();
                if (version > 6) (void) U16();  // personality modifier
                t.motives.push_back(m);
            }
            if (version > 9) (void) U32();     // flags2
            interactions.push_back(t);
        }
        delete br;
        return true;
    } catch (...) { interactions.clear(); return false; }
}

// ---------------- ARRY ----------------
// Ported from ARRY.cs: a mix of raw runs and padded/copy runs.
bool Arry::parse(const Chunk& c) {
    try {
        Reader r(c.data, Endian::Little);
        (void) r.s32();           // zero
        width  = r.s32();
        height = r.s32();
        type   = r.s32();         // == element byte size
        if (width<=0 || height<=0 || type<=0 || width>4096 || height>4096) return false;
        size_t total = (size_t)width*height*type;
        data.assign(total, 0);
        (void) r.s32();           // unknown

        size_t pos = 0;
        while (r.has_more()) {
            uint16_t flre = r.u16();
            if (flre == 0) break;
            bool rawFill = (flre & 0x8000) == 0;
            if (rawFill) {
                for (int i=0;i<flre;i++){ if(!r.has_more()) return true; if(pos<total) data[pos++]=r.u8(); else return true; }
                if (flre & 1) { if(r.has_more()) r.u8(); }
                continue;
            }
            size_t lastPos = pos;
            pos += (flre & 0x7FFF);
            if (total) pos %= total;
            if (pos == 0) return true;
            if (!r.has_more()) return true;
            uint8_t pad = r.u8();
            if (r.has_more()) r.u8();     // 16-bit pad
            while (lastPos < pos && lastPos < total) data[lastPos++] = pad;
            if (!r.has_more()) return true;
            int16_t size = r.s16();
            if (size & 0x8000) { r.seek(r.pos()-2); continue; }
            for (int i=0;i<size;i++){ if(!r.has_more())return true; data[pos++]=r.u8(); if(total)pos%=total; }
            if (size & 1) { if(r.has_more()) r.u8(); }
            if (total) pos %= total;
        }
        return true;
    } catch (...) { return false; }
}

// ---------------- SPR2 ----------------
// Decode one frame: header + run-length body. Ported from SPR2.cs Decode().
static void decodeFrame(Spr2Frame& f, Reader& io, uint32_t version,
                        uint32_t defaultPalette, IffFile& owner) {
    f.width  = io.u16();
    f.height = io.u16();
    f.flags  = io.u32();
    f.paletteID = io.u16();
    if (version == 1000 || f.paletteID == 0 || f.paletteID == 0xA3A3)
        f.paletteID = (uint16_t)defaultPalette;
    f.transparentIndex = io.u16();
    int16_t y0 = io.s16();
    int16_t x0 = io.s16();
    f.posY = y0; f.posX = x0;

    // palette
    Palt palt;
    const Chunk* pc = owner.get("PALT", f.paletteID);
    bool havePalt = pc && palt.parse(*pc);
    Color transparent = havePalt ? palt.colors[f.transparentIndex] : Color{};
    transparent.a = 0;

    const bool hasPixels  = (f.flags & 0x01) != 0;
    const bool hasZBuffer = (f.flags & 0x02) != 0;
    const bool hasAlpha   = (f.flags & 0x04) != 0;

    int numPixels = f.width * f.height;
    if (numPixels < 0 || numPixels > 4096*4096) { f.width=f.height=0; return; }
    if (hasPixels)  { f.pixels.assign(numPixels, Color{}); f.palIdx.assign(numPixels, 0); }
    if (hasZBuffer) f.zbuf.assign(numPixels, 0);

    auto px = [&](int x,int yy)->int { return yy*f.width + x; };

    int y = 0; bool end = false;
    while (!end && io.has_more()) {
        uint16_t marker = io.u16();
        int command = marker >> 13;
        int count = marker & 0x1FFF;
        switch (command) {
            case 0x00: { // row of pixel commands, `count` bytes incl. this marker
                int bytes = count - 2;
                int x = 0;
                while (bytes > 0 && io.has_more()) {
                    uint16_t pxm = io.u16(); bytes -= 2;
                    int pxCmd = pxm >> 13;
                    int pxCnt = pxm & 0x1FFF;
                    if (pxCmd == 0x01 || pxCmd == 0x02) {
                        bool withAlpha = (pxCmd == 0x02);
                        for (int col=0; col<pxCnt; col++) {
                            uint8_t z = io.u8();
                            uint8_t pv = io.u8();
                            bytes -= 2;
                            Color col2 = havePalt ? palt.colors[pv] : Color{};
                            if (withAlpha) { uint8_t a = io.u8(); col2.a = (uint8_t)(a * 8.2258064516); bytes--; }
                            if (hasPixels && x < f.width) { int o=px(x,y); f.pixels[o]=col2; f.palIdx[o]=pv; }
                            if (hasZBuffer && x < f.width) f.zbuf[px(x,y)] = z;
                            x++;
                        }
                        if (withAlpha && (pxCnt*3)%2 != 0) { io.u8(); bytes--; }
                    } else if (pxCmd == 0x03) {
                        for (int col=0; col<pxCnt; col++) {
                            if (hasPixels && x<f.width){int o=px(x,y); f.pixels[o]=transparent; f.palIdx[o]=(uint8_t)f.transparentIndex;}
                            if (hasZBuffer && x<f.width) f.zbuf[px(x,y)]=255;
                            x++;
                        }
                    } else if (pxCmd == 0x06) {
                        for (int col=0; col<pxCnt; col++) {
                            uint8_t pv = io.u8(); bytes--;
                            Color col2 = havePalt ? palt.colors[pv] : Color{};
                            if (hasPixels && x<f.width){int o=px(x,y); f.pixels[o]=col2; f.palIdx[o]=pv;}
                            if (hasZBuffer && x<f.width) f.zbuf[px(x,y)]=0;
                            x++;
                        }
                        if (pxCnt % 2 != 0) { io.u8(); bytes--; }
                    } else {
                        // unknown sub-command; stop this row to avoid runaway
                        break;
                    }
                }
                while (x < f.width) { if (hasZBuffer) f.zbuf[px(x,y)]=255; x++; }
                break;
            }
            case 0x04: { // `count` fully transparent rows
                for (int row=0; row<count; row++) {
                    for (int col=0; col<f.width; col++) {
                        int o = px(col, y+row);
                        if (o < 0 || o >= numPixels) continue;
                        if (hasPixels) { f.pixels[o]=transparent; f.palIdx[o]=(uint8_t)f.transparentIndex; }
                        if (hasZBuffer) f.zbuf[o]=255;
                    }
                }
                y += count - 1;
                break;
            }
            case 0x05: end = true; break;
            default: break;
        }
        y++;
    }
}

bool Spr2::parse(const Chunk& c, IffFile& owner) {
    try {
        Reader r(c.data, Endian::Little);
        uint32_t version = r.u32();
        uint32_t spriteCount = 0;
        std::vector<uint32_t> offsets;

        if (version == 1000) {
            spriteCount = r.u32();
            defaultPalette = r.u32();
            offsets.resize(spriteCount);
            for (uint32_t i=0;i<spriteCount;i++) offsets[i] = r.u32();
        } else if (version == 1001) {
            defaultPalette = r.u32();
            spriteCount = r.u32();
        } else {
            return false;
        }

        frames.clear();
        frames.reserve(spriteCount);

        for (uint32_t i=0;i<spriteCount;i++) {
            Spr2Frame f;
            if (version == 1001) {
                (void) r.u32();                 // per-sprite version
                uint32_t spriteSize = r.u32();
                size_t start = r.pos();
                if (start + spriteSize > c.data.size()) break;
                Reader body(c.data.data()+start, spriteSize, Endian::Little);
                decodeFrame(f, body, version, defaultPalette, owner);
                r.seek(start + spriteSize);
            } else { // 1000
                uint32_t off = offsets[i];
                uint32_t nextOff = (i+1<offsets.size()) ? offsets[i+1] : (uint32_t)c.data.size();
                if (off > c.data.size() || nextOff > c.data.size() || nextOff < off) break;
                Reader body(c.data.data()+off, nextOff-off, Endian::Little);
                decodeFrame(f, body, version, defaultPalette, owner);
            }
            frames.push_back(std::move(f));
        }
        return true;
    } catch (...) { return false; }
}

} // namespace st
