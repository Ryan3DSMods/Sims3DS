/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Host-side verification tool for the sims asset loaders.
//   simtool far   <archive.far>                 list FAR members
//   simtool iff   <file.iff>                     list IFF chunks
//   simtool strs  <file.iff>                     dump STR# chunks
//   simtool objd  <file.iff>                     dump OBJD headers
//   simtool sprite <file.iff> <sprId> <frame> <out.png>   decode one SPR2 frame
//   simtool sheet  <file.iff> <outdir> [max]     dump every SPR2 frame as PNG
#include "../formats/far1.h"
#include "../formats/iff.h"
#include "../sim/vm.h"
#include "../sim/autonomy.h"
#include "../core/png.h"
#include <cstdio>
#include <cstring>
#include <map>
#include <cmath>
#include <string>
#include <cstdlib>

using namespace st;

static int do_far(const char* path) {
    Far1 far;
    if (!far.open(path)) { printf("FAR: failed to open/parse %s\n", path); return 1; }
    printf("FAR1 OK: %zu entries\n", far.entries().size());
    int shown=0;
    for (auto& e : far.entries()) {
        printf("  %8u  %s\n", e.length, e.name.c_str());
        if (++shown >= 40) { printf("  ... (%zu total)\n", far.entries().size()); break; }
    }
    return 0;
}

static int do_iff(const char* path) {
    IffFile iff;
    if (!iff.open(path)) { printf("IFF: failed to open/parse %s\n", path); return 1; }
    printf("IFF OK: %s, %zu chunks\n", iff.filename().c_str(), iff.chunks().size());
    std::map<std::string,int> hist;
    for (auto& c : iff.chunks()) hist[c.type]++;
    for (auto& kv : hist) printf("  %-5s x%d\n", kv.first.c_str(), kv.second);
    return 0;
}

static int do_strs(const char* path) {
    IffFile iff;
    if (!iff.open(path)) { printf("IFF fail\n"); return 1; }
    for (auto* c : iff.byType("STR#")) {
        StrSet s;
        if (!s.parse(*c)) { printf("STR# id=%u parse fail\n", c->id); continue; }
        printf("STR# id=%u \"%s\" (%zu strings)\n", c->id, c->label.c_str(), s.strings.size());
        int n=0; for (auto& str : s.strings) { printf("    [%d] %s\n", n, str.c_str()); if(++n>=8){printf("    ...\n");break;} }
    }
    return 0;
}

static int do_objd(const char* path) {
    IffFile iff;
    if (!iff.open(path)) { printf("IFF fail\n"); return 1; }
    auto objs = iff.byType("OBJD");
    printf("%zu OBJD chunks\n", objs.size());
    int n=0;
    for (auto* c : objs) {
        Objd o; if (!o.parse(*c)) continue;
        printf("  OBJD id=%u ver=%u guid=%08X baseGfx=%u numGfx=%u price=%u  \"%s\"\n",
               c->id, o.version, o.guid, o.baseGraphicID, o.numGraphics, o.price, c->label.c_str());
        if (++n>=30){printf("  ...\n");break;}
    }
    return 0;
}

static int do_sprite(const char* path, int id, int frameIdx, const char* out) {
    IffFile iff;
    if (!iff.open(path)) { printf("IFF fail\n"); return 1; }
    const Chunk* c = iff.get("SPR2", (uint16_t)id);
    if (!c) { printf("no SPR2 id=%d\n", id); return 1; }
    Spr2 s;
    if (!s.parse(*c, iff)) { printf("SPR2 parse fail\n"); return 1; }
    printf("SPR2 id=%d frames=%zu defPalt=%u\n", id, s.frames.size(), s.defaultPalette);
    if (frameIdx < 0 || frameIdx >= (int)s.frames.size()) { printf("bad frame\n"); return 1; }
    auto& f = s.frames[frameIdx];
    printf("frame %d: %dx%d flags=%u palID=%u trans=%u pos=(%d,%d) pixels=%zu\n",
           frameIdx, f.width, f.height, f.flags, f.paletteID, f.transparentIndex, f.posX, f.posY, f.pixels.size());
    if (f.width<=0||f.height<=0||f.pixels.empty()) { printf("empty frame\n"); return 1; }
    std::vector<uint8_t> rgba((size_t)f.width*f.height*4);
    for (size_t i=0;i<f.pixels.size();i++){ rgba[i*4]=f.pixels[i].r; rgba[i*4+1]=f.pixels[i].g; rgba[i*4+2]=f.pixels[i].b; rgba[i*4+3]=f.pixels[i].a; }
    if (!writePNG(out, f.width, f.height, rgba.data())) { printf("png write fail\n"); return 1; }
    printf("wrote %s\n", out);
    return 0;
}

// Composite one DGRP image (a set of sprite references) into an RGBA canvas.
struct Canvas { int w=0,h=0,ox=0,oy=0; std::vector<Color> px; };
static bool compositeImage(IffFile& iff, const DgrpImage& im, Canvas& out) {
    // Resolve each sprite ref to its decoded SPR2 frame, gather bbox.
    struct Placed { const Spr2Frame* f; int x, y; bool flip; };
    std::vector<Spr2> owned; owned.reserve(im.sprites.size());
    std::vector<Placed> placed;
    int minx=1<<30, miny=1<<30, maxx=-(1<<30), maxy=-(1<<30);
    for (auto& s : im.sprites) {
        const Chunk* c = iff.get("SPR2", (uint16_t)s.spriteID);
        if (!c) continue;
        owned.emplace_back();
        Spr2& sp = owned.back();
        if (!sp.parse(*c, iff)) continue;
        if (s.frameIndex >= sp.frames.size()) continue;
        const Spr2Frame& f = sp.frames[s.frameIndex];
        if (f.width<=0 || f.height<=0) continue;
        // Placement: DGRP offset plus the frame's own stored position.
        int x = s.offX + f.posX;
        int y = s.offY + f.posY;
        placed.push_back({&f, x, y, s.flip()});
        minx=std::min(minx,x); miny=std::min(miny,y);
        maxx=std::max(maxx,x+f.width); maxy=std::max(maxy,y+f.height);
    }
    if (placed.empty()) return false;
    out.w = maxx-minx; out.h = maxy-miny; out.ox=minx; out.oy=miny;
    if (out.w<=0||out.h<=0||out.w>4096||out.h>4096) return false;
    out.px.assign((size_t)out.w*out.h, Color{0,0,0,0});
    for (auto& p : placed) {
        const Spr2Frame& f = *p.f;
        for (int yy=0; yy<f.height; yy++) for (int xx=0; xx<f.width; xx++) {
            const Color& src = f.pixels[(size_t)yy*f.width + xx];
            if (src.a == 0) continue;
            int dx = (p.x - minx) + (p.flip ? (f.width-1-xx) : xx);
            int dy = (p.y - miny) + yy;
            if (dx<0||dy<0||dx>=out.w||dy>=out.h) continue;
            out.px[(size_t)dy*out.w + dx] = src;  // painter's order
        }
    }
    return true;
}

static bool savePNG(const char* path, const Canvas& c) {
    std::vector<uint8_t> rgba((size_t)c.w*c.h*4);
    for (size_t i=0;i<c.px.size();i++){ rgba[i*4]=c.px[i].r; rgba[i*4+1]=c.px[i].g; rgba[i*4+2]=c.px[i].b; rgba[i*4+3]=c.px[i].a; }
    return writePNG(path, c.w, c.h, rgba.data());
}

static int do_object(const char* farpath, const char* member, const char* outdir) {
    Far1 far;
    if (!far.open(farpath)) { printf("FAR open fail\n"); return 1; }
    auto bytes = far.read(member);
    if (bytes.empty()) { printf("member '%s' not found\n", member); return 1; }
    IffFile iff;
    if (!iff.openBytes(std::move(bytes), member)) { printf("IFF parse fail\n"); return 1; }

    auto objds = iff.byType("OBJD");
    auto dgrps = iff.byType("DGRP");
    printf("%s: OBJD x%zu, DGRP x%zu, SPR2 x%d, PALT x%d\n",
           member, objds.size(), dgrps.size(), iff.count("SPR2"), iff.count("PALT"));
    for (auto* c : objds) { Objd o; if(o.parse(*c)) printf("  OBJD id=%u guid=%08X baseGfx=%u numGfx=%u price=%u \"%s\"\n",
                                                           c->id,o.guid,o.baseGraphicID,o.numGraphics,o.price,c->label.c_str()); }

    int written=0;
    for (auto* c : dgrps) {
        Dgrp d; if (!d.parse(*c)) continue;
        // pick the near (largest) zoom, front-right direction if present
        uint32_t bestZoom=0; for (auto& im : d.images) bestZoom=std::max(bestZoom, im.zoom);
        const DgrpImage* best=nullptr;
        for (uint32_t dir : {0x04u,0x10u,0x01u,0x40u}) { best=d.get(dir,bestZoom); if(best) break; }
        if (!best && !d.images.empty()) best=&d.images.front();
        if (!best) continue;
        Canvas cv;
        if (!compositeImage(iff, *best, cv)) continue;
        char name[512]; snprintf(name,sizeof name,"%s/dgrp%u.png", outdir, c->id);
        if (savePNG(name, cv)) { written++; if(written<=6) printf("  DGRP id=%u -> %dx%d (%zu sprites)\n", c->id, cv.w, cv.h, best->sprites.size()); }
    }
    printf("composited %d DGRP images\n", written);
    return 0;
}

// ---- isometric lot renderer ----
struct Scene {
    int w,h; std::vector<Color> px;
    Scene(int W,int H):w(W),h(H),px((size_t)W*H,Color{30,30,40,255}){}
    void blend(int x,int y,Color c){ if(x<0||y<0||x>=w||y>=h||c.a==0)return;
        Color& d=px[(size_t)y*w+x];
        if(c.a==255){d=c;return;}
        float a=c.a/255.f; d.r=(uint8_t)(c.r*a+d.r*(1-a)); d.g=(uint8_t)(c.g*a+d.g*(1-a)); d.b=(uint8_t)(c.b*a+d.b*(1-a)); d.a=255; }
};
// filled iso diamond centered at (cx,cy), half-width hw, half-height hh
static void diamond(Scene& s,int cx,int cy,int hw,int hh,Color fill,Color edge){
    for(int dy=-hh;dy<=hh;dy++){
        float t=1.f-(float)std::abs(dy)/hh; int span=(int)(hw*t);
        for(int dx=-span;dx<=span;dx++){
            bool border=(span-std::abs(dx)<=1)||(std::abs(dy)>=hh-1);
            s.blend(cx+dx,cy+dy,border?edge:fill);
        }
    }
}

// Draw a raised wall along a floor edge from (ax,ay) to (bx,by), height h.
static void wallSeg(Scene& s,int ax,int ay,int bx,int by,int h,Color face,Color cap){
    int dx=bx-ax, dy=by-ay; int steps=std::max(std::abs(dx),std::abs(dy)); if(steps<1)steps=1;
    for(int i=0;i<=steps;i++){
        int x=ax+dx*i/steps, y=ay+dy*i/steps;
        for(int k=0;k<=h;k++) s.blend(x,y-k, k>=h-1?cap:face);
    }
}

static int do_lot(const char* housePath, const char* farPath, const char* outPath) {
    IffFile house;
    if(!house.open(housePath)){printf("house open fail\n");return 1;}
    // floor = ARRY id 1 (ground floor), altitude = ARRY id 0 (per blueprint map)
    Arry floor, alt; bool haveFloor=false, haveAlt=false;
    Arry walls; bool haveWalls=false;
    if(const Chunk* fc=house.get("Arry",1)) haveFloor=floor.parse(*fc);
    if(const Chunk* ac=house.get("Arry",0)) haveAlt=alt.parse(*ac);
    if(const Chunk* wc=house.get("Arry",2)) haveWalls=walls.parse(*wc);
    if(!haveFloor){printf("no floor ARRY (id=1) found\n");return 1;}
    auto wallSegAt=[&](int x,int y)->uint8_t{
        if(!haveWalls) return 0; size_t o=(size_t)(y*walls.width+x)*8; return o<walls.data.size()?walls.data[o]:0; };
    // crop to the used region
    int minx=floor.width,miny=floor.height,maxx=0,maxy=0,used=0;
    for(int y=0;y<floor.height;y++)for(int x=0;x<floor.width;x++)
        if(floor.floorAt(x,y)!=0){used++;minx=std::min(minx,x);miny=std::min(miny,y);maxx=std::max(maxx,x);maxy=std::max(maxy,y);}
    printf("floor grid %dx%d, %d floored tiles, bbox (%d,%d)-(%d,%d)\n",floor.width,floor.height,used,minx,miny,maxx,maxy);
    if(used==0){minx=miny=0;maxx=floor.width-1;maxy=floor.height-1;}
    // pad bbox a little
    minx=std::max(0,minx-1);miny=std::max(0,miny-1);maxx=std::min(floor.width-1,maxx+1);maxy=std::min(floor.height-1,maxy+1);
    int gw=maxx-minx+1, gh=maxy-miny+1;

    const int TW=32, TH=16; // half-tile w/h for a compact lot overview
    int originX = (gh)*TW + 40;
    int W = (gw+gh)*TW + 80;
    int H = (gw+gh)*TH + 160;
    Scene s(W,H);

    // load two objects to place
    Far1 far; IffFile clk, shelf; bool haveClk=false, haveShelf=false;
    if(far.open(farPath)){
        auto b1=far.read("AlarmClock.iff"); if(!b1.empty()) haveClk=clk.openBytes(std::move(b1),"AlarmClock.iff");
        auto b2=far.read("Bookcases.iff"); if(!b2.empty()) haveShelf=shelf.openBytes(std::move(b2),"Bookcases.iff");
    }
    auto compositeFirst=[&](IffFile& iff, Canvas& cv)->bool{
        for(auto* c: iff.byType("DGRP")){ Dgrp d; if(!d.parse(*c))continue;
            uint32_t z=0; for(auto&im:d.images)z=std::max(z,im.zoom);
            const DgrpImage* best=nullptr; for(uint32_t dir:{0x04u,0x10u,0x01u,0x40u}){best=d.get(dir,z);if(best)break;}
            if(best && compositeImage(iff,*best,cv)) return true; }
        return false; };
    Canvas clkC, shelfC; bool clkOk=haveClk&&compositeFirst(clk,clkC); bool shelfOk=haveShelf&&compositeFirst(shelf,shelfC);

    // draw floor diamonds back-to-front
    for(int sum=0; sum<=(gw-1)+(gh-1); sum++)
      for(int gx=0; gx<gw; gx++){ int gy=sum-gx; if(gy<0||gy>=gh)continue;
        int fx=minx+gx, fy=miny+gy;
        uint8_t fi=floor.floorAt(fx,fy);
        int a = haveAlt? alt.altAt(fx,fy):0; if(a<-100||a>100)a=0;
        int sx=originX+(gx-gy)*TW;
        int sy=60+(gx+gy)*TH - a;
        Color fill,edge;
        if(fi==0){ fill={74,122,56,255}; edge={64,108,50,255}; }          // grass
        else { // distinct room colour per floor index
            uint8_t r=(uint8_t)(150+ (fi*53)%90);
            uint8_t g=(uint8_t)(120+ (fi*97)%100);
            uint8_t b=(uint8_t)(90 + (fi*29)%120);
            fill={r,g,b,255}; edge={(uint8_t)(r*0.7),(uint8_t)(g*0.7),(uint8_t)(b*0.7),255};
        }
        diamond(s,sx,sy,TW,TH,fill,edge);

        // walls on this tile's top-left / top-right edges (raised)
        uint8_t seg=wallSegAt(fx,fy);
        const int WALLH=34;
        // vertices
        int topx=sx,       topy=sy-TH;
        int leftx=sx-TW,   lefty=sy;
        int rightx=sx+TW,  righty=sy;
        if(seg & 0x01) wallSeg(s,leftx,lefty, topx,topy, WALLH, Color{150,142,126,255}, Color{196,190,176,255}); // TopLeft
        if(seg & 0x02) wallSeg(s,topx,topy, rightx,righty, WALLH, Color{120,113,99,255},  Color{196,190,176,255}); // TopRight
      }

    // place objects on a couple of floored tiles
    auto place=[&](Canvas& cv,int gx,int gy){ if(cv.w<=0)return;
        int sx=originX+(gx-gy)*TW; int sy=60+(gx+gy)*TH;
        // anchor object bottom-centre to tile centre
        int px0=sx-cv.w/2; int py0=sy - cv.h + TH;
        for(int yy=0;yy<cv.h;yy++)for(int xx=0;xx<cv.w;xx++){Color c=cv.px[(size_t)yy*cv.w+xx]; if(c.a)s.blend(px0+xx,py0+yy,c);} };
    // place objects near the centroid of the floored area (building interior)
    long cxg=0,cyg=0,cn=0;
    for(int gy=0;gy<gh;gy++)for(int gx=0;gx<gw;gx++) if(floor.floorAt(minx+gx,miny+gy)!=0){cxg+=gx;cyg+=gy;cn++;}
    int placed=0;
    if(cn){ int ccx=(int)(cxg/cn), ccy=(int)(cyg/cn);
        // nearest floored tile to centroid, and one offset by +1 in x
        auto floored=[&](int gx,int gy){ return gx>=0&&gy>=0&&gx<gw&&gy<gh&&floor.floorAt(minx+gx,miny+gy)!=0; };
        if(shelfOk && floored(ccx,ccy)){ place(shelfC,ccx,ccy); placed++; }
        if(clkOk){ for(int dx=1;dx<6;dx++) if(floored(ccx+dx,ccy)){ place(clkC,ccx+dx,ccy); placed++; break; } }
    }

    std::vector<uint8_t> rgba((size_t)W*H*4);
    for(size_t i=0;i<s.px.size();i++){rgba[i*4]=s.px[i].r;rgba[i*4+1]=s.px[i].g;rgba[i*4+2]=s.px[i].b;rgba[i*4+3]=255;}
    if(!writePNG(outPath,W,H,rgba.data())){printf("png fail\n");return 1;}
    printf("wrote %s (%dx%d), placed %d objects\n",outPath,W,H,placed);
    return 0;
}

static int do_sheet(const char* path, const char* outdir, int maxn) {
    IffFile iff;
    if (!iff.open(path)) { printf("IFF fail\n"); return 1; }
    int written=0, emptyf=0;
    for (auto* c : iff.byType("SPR2")) {
        Spr2 s; if (!s.parse(*c, iff)) continue;
        for (size_t fi=0; fi<s.frames.size(); fi++) {
            auto& f = s.frames[fi];
            if (f.width<=0||f.height<=0||f.pixels.empty()) { emptyf++; continue; }
            std::vector<uint8_t> rgba((size_t)f.width*f.height*4);
            for (size_t i=0;i<f.pixels.size();i++){ rgba[i*4]=f.pixels[i].r; rgba[i*4+1]=f.pixels[i].g; rgba[i*4+2]=f.pixels[i].b; rgba[i*4+3]=f.pixels[i].a; }
            char name[512]; snprintf(name,sizeof name,"%s/spr%u_f%zu.png", outdir, c->id, fi);
            if (writePNG(name,f.width,f.height,rgba.data())) written++;
            if (maxn>0 && written>=maxn) { printf("wrote %d PNGs (capped), %d empty frames\n", written, emptyf); return 0; }
        }
    }
    printf("wrote %d PNGs, %d empty frames\n", written, emptyf);
    return 0;
}


static int do_arry(const char* path){
    IffFile iff; if(!iff.open(path)){printf("open fail\n");return 1;}
    int idx=0;
    for(auto* c: iff.byType("Arry")){
        Arry a; bool ok=a.parse(*c);
        size_t nz=0; for(auto b:a.data) if(b)nz++;
        printf("ARRY[%d] id=%u ok=%d type=%d %dx%d bytes=%zu nonzero=%zu\n",idx++,c->id,ok,a.type,a.width,a.height,a.data.size(),nz);
    }
    return 0;
}

// load an object IFF either from a FAR member or a direct .iff path
static bool loadObject(const char* arg1, const char* arg2, IffFile& iff) {
    std::string a1=arg1;
    if (a1.size()>4 && a1.substr(a1.size()-4)==".far") {
        Far1 far; if(!far.open(arg1)) return false;
        auto b=far.read(arg2); if(b.empty()) return false;
        return iff.openBytes(std::move(b), arg2);
    }
    return iff.open(arg1);
}

static int do_bhavlist(const char* a1, const char* a2){
    IffFile iff; if(!loadObject(a1,a2,iff)){printf("load fail\n");return 1;}
    auto bs=iff.byType("BHAV");
    printf("%zu BHAV trees in %s\n", bs.size(), iff.filename().c_str());
    int n=0; for(auto* c: bs){ Bhav b; if(!b.parse(*c))continue;
        printf("  BHAV %u: %zu insns, args=%u locals=%u  \"%s\"\n", c->id, b.insns.size(), b.args, b.locals, c->label.c_str());
        if(++n>=40){printf("  ...\n");break;} }
    return 0;
}

static int do_bhav(const char* a1,const char* a2,int id){
    IffFile iff; if(!loadObject(a1,a2,iff)){printf("load fail\n");return 1;}
    const Chunk* c=iff.get("BHAV",(uint16_t)id); if(!c){printf("no BHAV %d\n",id);return 1;}
    Bhav b; if(!b.parse(*c)){printf("parse fail\n");return 1;}
    printf("BHAV %d \"%s\": %zu insns, args=%u locals=%u flags=%u\n",id,c->label.c_str(),b.insns.size(),b.args,b.locals,b.flags);
    for(size_t i=0;i<b.insns.size();i++){ auto&in=b.insns[i];
        char extra[128]=""; if(in.opcode==2) snprintf(extra,sizeof extra," [%s]", st::disassembleExpr(in.operand).c_str());
        auto pstr=[&](uint8_t p,char*o){ if(p==255)strcpy(o,"ret:F");else if(p==254)strcpy(o,"ret:T");else if(p==253)strcpy(o,"err");else sprintf(o,"%d",p); };
        char tp[8],fp[8]; pstr(in.truePtr,tp); pstr(in.falsePtr,fp);
        printf("  %2zu: %-22s T=%-5s F=%-5s%s\n", i,
               in.opcode>=256? (std::string("call tree ")+std::to_string(in.opcode)).c_str() : st::opcodeName(in.opcode),
               tp, fp, extra);
    }
    return 0;
}

// Hand-assembled tree: temp0=5; temp1=7; temp0+=temp1; if(temp0>10) SysLog, return TRUE else FALSE
static void mkExpr(BhavInstruction& in,int16_t ld,int16_t rd,uint8_t op,uint8_t lo,uint8_t ro,uint8_t t,uint8_t f){
    in.opcode=2; in.truePtr=t; in.falsePtr=f;
    in.operand[0]=ld&0xff; in.operand[1]=(ld>>8)&0xff;
    in.operand[2]=rd&0xff; in.operand[3]=(rd>>8)&0xff;
    in.operand[4]=0; in.operand[5]=op; in.operand[6]=lo; in.operand[7]=ro;
}
static int do_vmtest(){
    Bhav b; b.locals=4; b.insns.resize(5);
    mkExpr(b.insns[0], 0,5, 5,8,7, 1,255);  // Temp[0] = 5
    mkExpr(b.insns[1], 1,7, 5,8,7, 2,255);  // Temp[1] = 7
    mkExpr(b.insns[2], 0,1, 3,8,8, 3,255);  // Temp[0] += Temp[1]
    mkExpr(b.insns[3], 0,10,0,8,7, 4,255);  // Temp[0] > 10 ?
    b.insns[4].opcode=30; b.insns[4].truePtr=254; b.insns[4].falsePtr=254; // SysLog -> return TRUE
    VM vm; vm.trace=true;
    auto r=vm.run(b);
    printf("=== VM self-test (hand-assembled tree) ===\n");
    for(auto&l:vm.log) printf("%s\n",l.c_str());
    printf("result: %s   temp0=%d temp1=%d\n", r==VM::R_TRUE?"TRUE":r==VM::R_FALSE?"FALSE":"ERROR", vm.temps[0], vm.temps[1]);
    bool ok = (r==VM::R_TRUE && vm.temps[0]==12 && vm.temps[1]==7);
    printf("self-test %s (expected TRUE, temp0=12, temp1=7)\n", ok?"PASSED":"FAILED");
    return ok?0:1;
}

static int do_vmrun(const char* a1,const char* a2,int id){
    IffFile iff; if(!loadObject(a1,a2,iff)){printf("load fail\n");return 1;}
    const Chunk* c=iff.get("BHAV",(uint16_t)id); if(!c){printf("no BHAV %d\n",id);return 1;}
    Bhav b; if(!b.parse(*c)){printf("parse fail\n");return 1;}
    VM vm; vm.obj=&iff; vm.trace=true;
    if(getenv("SIM_ENERGY")) vm.simMotives[5]=atoi(getenv("SIM_ENERGY"));
    printf("=== running BHAV %d \"%s\" (%zu insns) on real object bytecode ===\n", id, c->label.c_str(), b.insns.size());
    auto r=vm.run(b);
    for(auto&l:vm.log) printf("%s\n",l.c_str());
    printf("result: %s   (temps: %d %d %d %d)\n", r==VM::R_TRUE?"TRUE":r==VM::R_FALSE?"FALSE":"ERROR", vm.temps[0],vm.temps[1],vm.temps[2],vm.temps[3]);
    return 0;
}

static int do_interactions(const char* a1,const char* a2){
    IffFile iff; if(!loadObject(a1,a2,iff)){printf("load fail\n");return 1;}
    // object name from CTSS/OBJD
    auto objds=iff.byType("OBJD");
    printf("=== %s ===\n", iff.filename().c_str());
    for(auto* c: objds){ Objd o; if(o.parse(*c)) { printf("object: \"%s\" (guid %08X)\n", c->label.c_str(), o.guid); break; } }

    auto ttabs=iff.byType("TTAB");
    if(ttabs.empty()){printf("no TTAB (no interactions)\n");return 0;}
    // TTAs string table (pie-menu names)
    StrSet names; bool haveNames=false;
    for(auto* c: iff.byType("TTAs")){ if(names.parse(*c)){haveNames=true;break;} }

    for(auto* tc: ttabs){
        Ttab tt; if(!tt.parse(*tc)) continue;
        if(tt.interactions.empty()) continue;
        printf("TTAB %u: %zu interactions%s\n", tc->id, tt.interactions.size(), tt.fieldEncoded?" (field-encoded)":"");
        for(auto& in : tt.interactions){
            std::string nm = (haveNames && in.ttaIndex < names.strings.size()) ? names.strings[in.ttaIndex] : "(?)";
            // evaluate the guard tree through the VM
            const char* avail="-";
            if(in.testBHAV){
                const Chunk* bc=iff.get("BHAV",(uint16_t)in.testBHAV);
                if(bc){ Bhav b; if(b.parse(*bc)){ VM vm; vm.obj=&iff; auto r=vm.run(b); avail = r==VM::R_TRUE?"available":"unavailable"; } }
            } else avail="always";
            printf("  \"%-24s  action=BHAV %u  guard=%s%s\n",
                   (nm+"\"").c_str(), in.actionBHAV,
                   in.testBHAV? ("BHAV "+std::to_string(in.testBHAV)+" -> ").c_str() : "none ", avail);
            // advertised motives (non-zero ranges), by motive id
            static const char* MOT[16]={"HappyLife","HappyWeek","HappyDay","Mood","?4","Energy","Comfort","Hunger","Hygiene","Bladder","?10","SleepState","?12","Room","Social","Fun"};
            std::string ads;
            for(size_t m=0;m<in.motives.size()&&m<16;m++){ auto& a=in.motives[m]; if(a.min||a.max){ char b[48]; snprintf(b,sizeof b,"%s %d..%d  ",MOT[m],a.min,a.max); ads+=b; } }
            if(!ads.empty()) printf("      advertises: %s(threshold %u)\n", ads.c_str(), in.autonomyThreshold);
        }
    }
    return 0;
}

// Evaluate an interaction guard against a given attribute state (non-destructive).
static bool guardPasses(IffFile& iff, const std::map<int,int32_t>& state, uint16_t testBHAV){
    if(!testBHAV) return true;
    const Chunk* bc=iff.get("BHAV",testBHAV); if(!bc) return false;
    Bhav b; if(!b.parse(*bc)) return false;
    VM vm; vm.obj=&iff; vm.attrs=state;           // seed, discard changes
    return vm.run(b)==VM::R_TRUE;
}
static void printMenu(IffFile& iff, Ttab& tt, StrSet& names, bool haveNames, const std::map<int,int32_t>& state){
    for(auto& in : tt.interactions){
        std::string nm=(haveNames && in.ttaIndex<names.strings.size())? names.strings[in.ttaIndex] : "(?)";
        bool ok=guardPasses(iff,state,in.testBHAV);
        printf("    %s %-20s %s\n", ok?"[*]":"[ ]", nm.c_str(), ok?"available":"");
    }
}
static int do_simulate(const char* a1,const char* a2){
    IffFile iff; if(!loadObject(a1,a2,iff)){printf("load fail\n");return 1;}
    std::string objname="object"; for(auto*c:iff.byType("OBJD")){objname=c->label;break;}
    // gather the first TTAB with interactions + the names
    Ttab tt; bool haveTt=false;
    for(auto* c: iff.byType("TTAB")){ Ttab t; if(t.parse(*c)&&!t.interactions.empty()){tt=t;haveTt=true;break;} }
    if(!haveTt){printf("no interactions\n");return 1;}
    StrSet names; bool haveNames=false; for(auto*c:iff.byType("TTAs")){if(names.parse(*c)){haveNames=true;break;}}

    std::map<int,int32_t> state;  // the object's live attributes (all 0 initially)
    printf("=== %s — interactive simulation ===\n\n", objname.c_str());
    printf("pie menu (initial state):\n"); printMenu(iff,tt,names,haveNames,state);

    // find first available interaction and "click" it
    const TtabInteraction* chosen=nullptr; std::string chosenName;
    for(auto& in: tt.interactions){ if(guardPasses(iff,state,in.testBHAV)){ chosen=&in; chosenName=(haveNames&&in.ttaIndex<names.strings.size())?names.strings[in.ttaIndex]:"(?)"; break; } }
    if(!chosen){printf("\n(no available interaction to run)\n");return 0;}

    printf("\n>>> clicking \"%s\" (runs action BHAV %u)\n", chosenName.c_str(), chosen->actionBHAV);
    const Chunk* ac=iff.get("BHAV",chosen->actionBHAV);
    if(ac){ Bhav b; if(b.parse(*ac)){
        VM vm; vm.obj=&iff; vm.attrs=state;
        auto r=vm.run(b);
        // report attribute changes
        printf("    action returned %s\n", r==VM::R_TRUE?"TRUE":r==VM::R_FALSE?"FALSE":"ERROR");
        bool any=false;
        for(auto& kv: vm.attrs){ int32_t before = state.count(kv.first)?state[kv.first]:0; if(kv.second!=before){ printf("    attribute[%d]: %d -> %d\n", kv.first, before, kv.second); any=true; } }
        if(!any) printf("    (no attribute changes)\n");
        state = vm.attrs;  // commit new object state
        printf("\npie menu (after the action):\n"); printMenu(iff,tt,names,haveNames,state);
        if(any) printf("\n-> the object's state changed and its menu responded, driven by the VM.\n");
        else if(r==VM::R_ERROR) printf("\n-> the action tree reached a primitive the minimal VM doesn't implement yet.\n");
    }}
    return 0;
}

// autonomy <Objects.far> <csv-out> <obj1.iff> [obj2.iff ...]
static int do_autonomy(int argc, char** argv){
    const char* farPath=argv[2]; const char* csvPath=argv[3];
    Far1 far; if(!far.open(farPath)){printf("FAR open fail\n");return 1;}
    std::vector<IffFile> objs; objs.reserve(argc-4);
    std::vector<LotInteraction> lot;
    for(int i=4;i<argc;i++){
        auto b=far.read(argv[i]); if(b.empty()){printf("missing %s\n",argv[i]);continue;}
        objs.emplace_back(); IffFile& iff=objs.back();
        if(!iff.openBytes(std::move(b),argv[i])){printf("parse fail %s\n",argv[i]);objs.pop_back();continue;}
        std::string nm=argv[i]; for(auto*c:iff.byType("OBJD")){nm=c->label;break;}
        collectInteractions(iff,nm,lot);
    }
    std::vector<std::string> objNames; for(auto& o: objs){ std::string nm=o.filename(); for(auto*c:o.byType("OBJD")){nm=c->label;break;} objNames.push_back(nm); }
    printf("=== lot: %zu advertised interactions from %zu objects ===\n", lot.size(), objs.size());
    for(auto& it: lot){ std::string ads; for(size_t m=0;m<it.ads.size()&&m<16;m++) if(it.ads[m].min||it.ads[m].max){char b[40];snprintf(b,sizeof b,"%s %+d ",motiveName((int)m),it.ads[m].max?it.ads[m].max:it.ads[m].min);ads+=b;}
        printf("  %-22s %-16s -> %s\n", it.object.c_str(), it.name.c_str(), ads.c_str()); }

    Sim sim; sim.name="Bob";
    // start the day with a few needs already low so choices are interesting
    sim.motives[Hunger]=-20; sim.motives[Energy]=40; sim.motives[Fun]=10; sim.motives[Hygiene]=30; sim.motives[Comfort]=20; sim.motives[Bladder]=20; sim.motives[Social]=0;
    FILE* csv=fopen(csvPath,"w"); if(csv) fprintf(csv,"hour,Hunger,Energy,Fun,Hygiene,Comfort,Bladder,Social,action\n");

    printf("\n=== a day for %s (needs-driven autonomy, game scoring) ===\n", sim.name.c_str());
    printf("hour  Hunger Energy Fun  Hyg  Comf Blad | decision\n");
    for(int hour=0; hour<24; hour++){
        sim.decay(1);
        // guards depend on the Sim's state (e.g. Sleep needs Energy<50): re-evaluate the lot each hour
        lot.clear(); for(size_t k=0;k<objs.size();k++) collectInteractions(objs[k],objNames[k],lot,&sim);
        int score=0; int pick=findBestAction(sim,lot,&score);
        std::string act="(idle)";
        if(pick>=0){ int bm=-1; scoreInteraction(sim,lot[pick],&bm);
            char b[128]; snprintf(b,sizeof b,"%s: %s  (score %d, for %s)", lot[pick].object.c_str(), lot[pick].name.c_str(), score, motiveName(bm)); act=b;
            performInteraction(sim,lot[pick]); }
        printf("%02d:00 %6d %6d %4d %4d %5d %5d | %s\n", hour, sim.motives[Hunger],sim.motives[Energy],sim.motives[Fun],sim.motives[Hygiene],sim.motives[Comfort],sim.motives[Bladder], act.c_str());
        if(csv) fprintf(csv,"%d,%d,%d,%d,%d,%d,%d,%d,%s\n",hour,sim.motives[Hunger],sim.motives[Energy],sim.motives[Fun],sim.motives[Hygiene],sim.motives[Comfort],sim.motives[Bladder],sim.motives[Social], pick>=0?lot[pick].name.c_str():"");
    }
    if(csv) fclose(csv);
    return 0;
}


// living <House.iff> <Objects.far> <outdir> : simulate a day on the rendered lot, one frame per step
struct PlacedObj { std::string file, label; int gx, gy; Canvas cv; IffFile iff; };
static int do_living(const char* housePath,const char* farPath,const char* outdir){
    IffFile house; if(!house.open(housePath)){printf("house fail\n");return 1;}
    Arry floor, walls; bool hf=false,hw=false;
    if(const Chunk*c=house.get("Arry",1)) hf=floor.parse(*c);
    if(const Chunk*c=house.get("Arry",2)) hw=walls.parse(*c);
    if(!hf) return 1;
    int minx=64,miny=64,maxx=0,maxy=0; for(int y=0;y<64;y++)for(int x=0;x<64;x++) if(floor.floorAt(x,y)){minx=std::min(minx,x);miny=std::min(miny,y);maxx=std::max(maxx,x);maxy=std::max(maxy,y);}
    // crop to the house itself (skip the road rows at the bottom of the plan)
    maxy=std::min(maxy, miny+ (maxy-miny)*2/3);
    minx=std::max(0,minx-1);miny=std::max(0,miny-1);maxx=std::min(63,maxx+1);maxy=std::min(63,maxy+1);
    int gw=maxx-minx+1, gh=maxy-miny+1;
    const int TW=20, TH=10; int originX=gh*TW+30, W=(gw+gh)*TW+60, H=(gw+gh)*TH+200;
    auto wallAt=[&](int x,int y)->uint8_t{ if(!hw)return 0; size_t o=(size_t)(y*walls.width+x)*8; return o<walls.data.size()?walls.data[o]:0; };
    auto toScreen=[&](float gx,float gy,int&sx,int&sy){ sx=originX+(int)((gx-gy)*TW); sy=60+(int)((gx+gy)*TH); };

    // household objects on interior floored tiles (positions chosen; OBJM placement isn't decoded)
    Far1 far; if(!far.open(farPath)) return 1;
    std::vector<PlacedObj> objs;
    const char* files[]={"Fridges.iff","Toilets.iff","ShowerC.iff","Beds.iff","Stereos.iff"};
    // pick distinct floored tiles spread through the house
    // interior = floored tiles inside the bounding box of the walls (excludes the road strip)
    int wx0=64,wy0=64,wx1=0,wy1=0; for(int y=0;y<64;y++)for(int x=0;x<64;x++) if(wallAt(x,y)&3){wx0=std::min(wx0,x);wy0=std::min(wy0,y);wx1=std::max(wx1,x);wy1=std::max(wy1,y);}
    std::vector<std::pair<int,int>> tiles; for(int gy=0;gy<gh;gy++)for(int gx=0;gx<gw;gx++){ int fx=minx+gx,fy=miny+gy; if(floor.floorAt(fx,fy)&&fx>wx0&&fx<wx1&&fy>wy0&&fy<wy1) tiles.push_back({gx,gy}); }
    if(tiles.size()<5) return 1;
    size_t step=tiles.size()/6;
    for(int i=0;i<5;i++){ PlacedObj p; p.file=files[i]; auto b=far.read(p.file); if(b.empty()) continue; if(!p.iff.openBytes(std::move(b),p.file)) continue;
        p.label=p.file; for(auto*c:p.iff.byType("OBJD")){p.label=c->label;break;}
        auto t=tiles[step*(i+1)]; p.gx=t.first; p.gy=t.second;
        for(auto*c:p.iff.byType("DGRP")){Dgrp d; if(!d.parse(*c))continue; uint32_t z=0; for(auto&im:d.images)z=std::max(z,im.zoom); const DgrpImage*best=nullptr; for(uint32_t dir:{0x04u,0x10u,0x01u,0x40u}){best=d.get(dir,z);if(best)break;} if(best&&compositeImage(p.iff,*best,p.cv))break;}
        objs.push_back(std::move(p)); }
    std::vector<std::string> names; for(auto&o:objs) names.push_back(o.label);

    Sim sim; sim.name="Bob"; sim.motives[Hunger]=-20; sim.motives[Energy]=40; sim.motives[Fun]=10; sim.motives[Hygiene]=30; sim.motives[Comfort]=20; sim.motives[Bladder]=20;
    float simx=tiles[tiles.size()/2].first, simy=tiles[tiles.size()/2].second;
    int frame=0; std::string status="waking up";
    auto render=[&](){
        Scene s(W,H);
        for(int sum=0;sum<=(gw-1)+(gh-1);sum++)for(int gx=0;gx<gw;gx++){int gy=sum-gx; if(gy<0||gy>=gh)continue; int fx=minx+gx,fy=miny+gy; uint8_t fi=floor.floorAt(fx,fy);
            int sx,sy; toScreen(gx,gy,sx,sy); Color fill,edge;
            if(!fi){fill={74,122,56,255};edge={64,108,50,255};} else {uint8_t r=150+(fi*53)%90,g=120+(fi*97)%100,b=90+(fi*29)%120; fill={r,g,b,255}; edge={(uint8_t)(r*0.7),(uint8_t)(g*0.7),(uint8_t)(b*0.7),255};}
            diamond(s,sx,sy,TW,TH,fill,edge);
            uint8_t seg=wallAt(fx,fy); const int WH=22; int tx=sx,ty=sy-TH,lx=sx-TW,ly=sy,rx=sx+TW,ry=sy;
            if(seg&1) wallSeg(s,lx,ly,tx,ty,WH,Color{150,142,126,255},Color{196,190,176,255});
            if(seg&2) wallSeg(s,tx,ty,rx,ry,WH,Color{120,113,99,255},Color{196,190,176,255});
            // objects + sim drawn in depth order with their tile
            for(auto&o:objs) if(o.gx==gx&&o.gy==gy&&o.cv.w>0){ float sc=0.55f; int ow=(int)(o.cv.w*sc),oh=(int)(o.cv.h*sc); int px0=sx-ow/2,py0=sy-oh+TH; for(int yy=0;yy<oh;yy++)for(int xx=0;xx<ow;xx++){Color c=o.cv.px[(size_t)(yy/sc)*o.cv.w+(size_t)(xx/sc)]; if(c.a)s.blend(px0+xx,py0+yy,c);} }
            if((int)(simx+0.5f)==gx&&(int)(simy+0.5f)==gy){ int px,py; toScreen(simx,simy,px,py);
                for(int yy=-26;yy<=0;yy++)for(int xx=-5;xx<=5;xx++){ bool head=yy<-18; bool body=!head&&std::abs(xx)<=4; if(head&&(xx*xx+(yy+22)*(yy+22)<=16)) s.blend(px+xx,py+yy,Color{240,200,170,255}); else if(body) s.blend(px+xx,py+yy,Color{60,110,200,255}); } }
        }
        // HUD: motive bars
        const char* L[5]={"Hunger","Energy","Bladder","Hygiene","Fun"}; int M[5]={Hunger,Energy,Bladder,Hygiene,Fun};
        int hx=16, hy=H-150;
        for(int i=0;i<5;i++){ int v=sim.motives[M[i]]; int bw=140, fillw=(v+100)*bw/200; Color col = v<-50?Color{220,80,80,255}:v<0?Color{230,170,70,255}:Color{90,200,110,255};
            for(int yy=0;yy<14;yy++)for(int xx=0;xx<bw;xx++) s.blend(hx+60+xx,hy+i*22+yy, xx<fillw?col:Color{50,54,66,255}); }
        std::vector<uint8_t> rgba((size_t)W*H*4); for(size_t i=0;i<s.px.size();i++){rgba[i*4]=s.px[i].r;rgba[i*4+1]=s.px[i].g;rgba[i*4+2]=s.px[i].b;rgba[i*4+3]=255;}
        char nm[512]; snprintf(nm,sizeof nm,"%s/frame%03d.png",outdir,frame); writePNG(nm,W,H,rgba.data());
        char meta[512]; snprintf(meta,sizeof meta,"%s/frame%03d.txt",outdir,frame); FILE*f=fopen(meta,"w"); if(f){fprintf(f,"%s\n",status.c_str());fclose(f);}
        frame++; };

    for(int hour=0;hour<24;hour++){
        sim.decay(1);
        std::vector<LotInteraction> lot; for(size_t k=0;k<objs.size();k++) collectInteractions(objs[k].iff,objs[k].label,lot,&sim);
        int score=0; int pick=findBestAction(sim,lot,&score);
        char st[160];
        if(pick<0){ snprintf(st,sizeof st,"%02d:00  idle",hour); status=st; render(); continue; }
        // find the object for this interaction and walk to it
        PlacedObj* target=nullptr; for(auto&o:objs) if(o.label==lot[pick].object){target=&o;break;}
        snprintf(st,sizeof st,"%02d:00  going to %s: %s",hour,lot[pick].object.c_str(),lot[pick].name.c_str()); status=st;
        if(target){ float tx=target->gx, ty=target->gy; for(int k=0;k<6;k++){ simx+= (tx-simx)/(6-k); simy+=(ty-simy)/(6-k); render(); } simx=tx; simy=ty; }
        performInteraction(sim,lot[pick]);
        snprintf(st,sizeof st,"%02d:00  %s: %s  (score %d)",hour,lot[pick].object.c_str(),lot[pick].name.c_str(),score); status=st; render(); render();
    }
    printf("rendered %d frames to %s\n",frame,outdir);
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) { printf("usage: simtool <far|iff|strs|objd|sprite|sheet|object|lot|arry|bhavlist|bhav|vmtest|vmrun> ...\n"); return 2; }
    std::string cmd = argv[1];
    if (cmd=="vmtest") return do_vmtest();
    if (argc < 3) { printf("usage: simtool <cmd> <file> ...\n"); return 2; }
    if (cmd=="far")  return do_far(argv[2]);
    if (cmd=="iff")  return do_iff(argv[2]);
    if (cmd=="strs") return do_strs(argv[2]);
    if (cmd=="objd") return do_objd(argv[2]);
    if (cmd=="sprite" && argc>=6) return do_sprite(argv[2], atoi(argv[3]), atoi(argv[4]), argv[5]);
    if (cmd=="sheet" && argc>=4)  return do_sheet(argv[2], argv[3], argc>=5?atoi(argv[4]):0);
    if (cmd=="object" && argc>=5) return do_object(argv[2], argv[3], argv[4]);
    if (cmd=="lot" && argc>=5) return do_lot(argv[2], argv[3], argv[4]);
    if (cmd=="arry" && argc>=3) return do_arry(argv[2]);
    if (cmd=="bhavlist" && argc>=4) return do_bhavlist(argv[2], argv[3]);
    if (cmd=="bhav" && argc>=5) return do_bhav(argv[2], argv[3], atoi(argv[4]));
    if (cmd=="vmtest") return do_vmtest();
    if (cmd=="vmrun" && argc>=5) return do_vmrun(argv[2], argv[3], atoi(argv[4]));
    if (cmd=="interactions" && argc>=4) return do_interactions(argv[2], argv[3]);
    if (cmd=="simulate" && argc>=4) return do_simulate(argv[2], argv[3]);
    if (cmd=="autonomy" && argc>=5) return do_autonomy(argc, argv);
    if (cmd=="living" && argc>=5) return do_living(argv[2], argv[3], argv[4]);
    printf("bad args\n"); return 2;
}
// (debug command appended)
