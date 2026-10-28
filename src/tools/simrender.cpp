/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// simrender — renders a TS1 lot with the translated FreeSims 2D world renderer (src/world) into a PNG.
//   simrender <gameDataDir> <cacheDir> <House.iff> <out.png> [zoom far|medium|near] [rotation 0-3] [w h] [centerTileX centerTileY]
// centerTileX = "house" centres the view on the walls of the house.
// gameDataDir is the folder that contains GameData/ (floors.iff, walls.iff, Build.iff, Floors/, Walls/,
// Objects/Objects.far, Global/Global.far).
// Environment:
//   SIMRENDER_WALLS = cut (default: walls cutaway around every indoor room, like TS1's "walls cutaway"),
//                     up, down
//   SIMRENDER_TICKS = VM ticks to run after loading (default 60)
//   SIMRENDER_LEVEL = floor to view (default 1)
//   SIMRENDER_FRAMES = frames to draw (default 2, the 2nd shows steady-state cost)
//   SIMRENDER_SIMS   = "x,y[,dir];..." move the lot's Sims to these tiles (level 1), e.g. to test occlusion
//   SIMRENDER_CUSTOM = "wallId,floorId" repaint every wall side / indoor floor of level 1 (tests custom .wll/.flr ids >= 256)
//   SIMRENDER_HOUR   = override the lot's clock hour (0-23) before ticking (outside light colour)
// Host only (uses zlib for the PNG).
#include "simantics/utils/VMTS1FamilyLoader.h"
#include "content/ContentManager.h"
#include "content/TS1NeighbourProvider.h"
#include "simantics/VM.h"
#include "simantics/VMContext.h"
#include "simantics/VMArchitecture.h"
#include "simantics/utils/VMWorldActivator.h"
#include "simantics/utils/VMArchitectureTools.h"
#include "simantics/entities/VMEntity.h"
#include "simantics/entities/VMAvatar.h"
#include "files/formats/iff/IffFile.h"
#include "world/World.h"
#include "core/png.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <chrono>
#include <unordered_set>
#include <algorithm>
#include <map>
#include <array>

using namespace FSO::SimAntics;
using namespace FSO::LotView;
using FSO::Files::Formats::IFF::IffFile;

static double NowMs() {
    using namespace std::chrono;
    return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
}

int main(int argc, char** argv) {
    if (argc < 5) {
        printf("usage: simrender <gameDataDir> <cacheDir> <House.iff> <out.png> [zoom far|medium|near] [rotation 0-3] [w h] [centerTileX centerTileY]\n");
        return 2;
    }
    std::string zoomName = argc > 5 ? argv[5] : "medium";
    int rotation = argc > 6 ? atoi(argv[6]) : 0;
    int width = argc > 8 ? atoi(argv[7]) : 1024;
    int height = argc > 8 ? atoi(argv[8]) : 768;
    bool hasCenter = argc > 10;
    float cx = hasCenter ? (float)atof(argv[9]) : 0, cy = hasCenter ? (float)atof(argv[10]) : 0;
    const char* wallsEnv = getenv("SIMRENDER_WALLS");
    std::string walls = wallsEnv ? wallsEnv : "cut";
    int ticks = getenv("SIMRENDER_TICKS") ? atoi(getenv("SIMRENDER_TICKS")) : 60;
    int level = getenv("SIMRENDER_LEVEL") ? atoi(getenv("SIMRENDER_LEVEL")) : 1;
    int frames = getenv("SIMRENDER_FRAMES") ? atoi(getenv("SIMRENDER_FRAMES")) : 2;

    WorldZoom zoom = WorldZoom::Medium;
    if (zoomName == "far") zoom = WorldZoom::Far;
    else if (zoomName == "near") zoom = WorldZoom::Near;

    double t0 = NowMs();
    FSO::Content::Content::Init(argv[1], argv[2]);
    if (const char* hood = getenv("SIMRENDER_HOOD")) { // TS1(sims): neighbourhood index; SIMRENDER_USERROOT = save area
        auto c = FSO::Content::Content::Get();
        if (getenv("SIMRENDER_USERROOT")) c->Neighborhood->UserDataRoot = getenv("SIMRENDER_USERROOT");
        c->Neighborhood->InitSpecific(atoi(hood));
        c->Neighborhood->LoadCharacters();
        c->WorldObjects->RebuildIndexes();
    }
    auto content = FSO::Content::Content::Get();
    printf("content: %zu wall patterns/refs, %zu floor refs, %zu custom walls, %zu custom floors (%.0f ms)\n",
           content->WorldWalls->Entries.size(), content->WorldFloors->Entries.size(),
           content->WorldWalls->DynamicWallFromID.size(), content->WorldFloors->DynamicFloorFromID.size(), NowMs() - t0);

    World world(width, height);
    if (getenv("SIMRENDER_SB")) world.SetScrollBuffer(atoi(getenv("SIMRENDER_SB")));
    VM::UseWorld(true);
    VMContext ctx(&world);
    FSO::SimAntics::Model::VMNullHeadlineProvider hp;
    VM vm(&ctx, &hp);
    vm.Init();
    vm.OnDialog = [](FSO::SimAntics::Model::VMDialogInfo&) {};
    IffFile house(argv[3]);
    VMWorldActivator act(&vm, &world);
    auto blueprint = act.LoadFromIff(&house);
    if (const char* hn = getenv("SIMRENDER_FAMILY")) {   // move the house's family in (TS1 neighbourhood)
        VMTS1FamilyLoader loader(&vm);
        auto sims = loader.LoadFamily((int16_t)atoi(hn));
        printf("family: %zu sims loaded\n", sims.size());
    }
    if (getenv("SIMRENDER_HOUR")) { ctx.Clock->Hours = atoi(getenv("SIMRENDER_HOUR")); ctx.Clock->Minutes = 0; }
    for (int i = 0; i < ticks; i++) vm.Update();
    if (const char* sims = getenv("SIMRENDER_SIMS")) {
        // move the lot's existing Sims (those with a body) to the given tiles, in entity order
        std::vector<VMAvatar*> avatars;
        for (auto e : vm.Entities) if (auto a = dynamic_cast<VMAvatar*>(e)) if (a->Avatar) avatars.push_back(a);
        std::string spec = sims;
        size_t p = 0, n = 0;
        while (p < spec.size() && n < avatars.size()) {
            size_t e = spec.find(';', p); if (e == std::string::npos) e = spec.size();
            int tx = 0, ty = 0, dir = 4;
            if (sscanf(spec.substr(p, e - p).c_str(), "%d,%d,%d", &tx, &ty, &dir) >= 2) {
                auto ava = avatars[n++];
                ava->SetPosition(FSO::LotView::Model::LotTilePos::FromBigTile((int16_t)tx, (int16_t)ty, 1), (FSO::LotView::Model::Direction)(1 << (dir & 7)), &ctx);
                printf("sim #%d moved to %d,%d -> pos %d,%d\n", ava->ObjectID, tx, ty, ava->Position().x, ava->Position().y);
            }
            p = e + 1;
        }
    }
    auto arch = ctx.Architecture();
    printf("clock %02d:%02d, outside colour %d,%d,%d\n", (int)ctx.Clock->Hours, (int)ctx.Clock->Minutes, blueprint->OutsideColor.R, blueprint->OutsideColor.G, blueprint->OutsideColor.B);
    printf("lot %dx%d, entities %zu, objects in blueprint %zu (%.0f ms)\n", arch->Width, arch->Height, vm.Entities.size(),
           blueprint->Objects.size(), NowMs() - t0);

    world.State.Zoom(zoom);
    world.State.Rotation((WorldRotation)(rotation & 3));
    world.State.Level((int8_t)level);
    if (!hasCenter) { cx = arch->Width / 2.0f; cy = arch->Height / 2.0f; }
    {   // report the house extent (tiles with walls on this level); "house" as the centre uses it
        int minX = 9999, minY = 9999, maxX = -1, maxY = -1;
        auto& wl = arch->Walls[(size_t)(level - 1)];
        for (int y = 0; y < arch->Height; y++) for (int x = 0; x < arch->Width; x++)
            if ((int)wl[(size_t)(y * arch->Width + x)].Segments != 0) { minX = std::min(minX, x); minY = std::min(minY, y); maxX = std::max(maxX, x); maxY = std::max(maxY, y); }
        if (maxX >= 0) {
            printf("walls on level %d span tiles x %d..%d, y %d..%d\n", level, minX, maxX, minY, maxY);
            if (hasCenter && argv[9][0] == 'h') { cx = (minX + maxX + 1) / 2.0f; cy = (minY + maxY + 1) / 2.0f; }
        }
    }
    world.State.CenterTile(st::Vector2(cx, cy));

    if (getenv("SIMRENDER_ROOF")) {   // TS1 "Roof" view: top story, walls up, roofs drawn
        world.State.DrawRoofs = true;
        int top = 1;
        for (int l = 1; l <= (int)arch->Floors.size(); l++) for (auto& f : arch->Floors[(size_t)(l - 1)]) if (f.Pattern != 0) { top = std::max(top, l); break; }
        world.State.Level((int8_t)top);
        walls = "up";
    }
    if (walls == "down") {
        blueprint->Cutaway.assign((size_t)(arch->Width * arch->Height), true);
    } else if (walls == "cut") {
        std::unordered_set<uint32_t> rooms;
        for (size_t i = 1; i < ctx.RoomInfo.size(); i++) if (!ctx.RoomInfo[i].Room.IsOutside) rooms.insert((uint32_t)i);
        blueprint->Cutaway = FSO::SimAntics::Utils::VMArchitectureTools::GenerateRoomCut(arch, (int8_t)level, world.State.Rotation(), rooms);
        world.State.DynamicCutaway = true;
    } else {
        blueprint->Cutaway.assign((size_t)(arch->Width * arch->Height), false);
    }
    blueprint->Damage.push_back(FSO::LotView::Model::BlueprintDamage(FSO::LotView::Model::BlueprintDamageType::WALL_CUT_CHANGED));

    if (const char* cust = getenv("SIMRENDER_CUSTOM")) {
        int wid = 0, fid = 0;
        sscanf(cust, "%d,%d", &wid, &fid);
        for (auto& w : arch->Walls[(size_t)(level - 1)]) {
            if ((int)w.Segments == 0) continue;
            if (wid) { if (w.TopLeftPattern) w.TopLeftPattern = (uint16_t)wid; if (w.TopRightPattern) w.TopRightPattern = (uint16_t)wid;
                       if (w.BottomLeftPattern) w.BottomLeftPattern = (uint16_t)wid; if (w.BottomRightPattern) w.BottomRightPattern = (uint16_t)wid; }
        }
        if (fid) for (auto& f : arch->Floors[(size_t)(level - 1)]) if (f.Pattern != 0 && f.Pattern < 65534 && f.Pattern != 10 && f.Pattern != 11 && f.Pattern != 12) f.Pattern = (uint16_t)fid;
        blueprint->SignalWallChange();
        printf("custom wall %d: %s, custom floor %d: %s\n", wid, content->WorldWalls->Entries.count((uint16_t)wid) ? content->WorldWalls->Entries[(uint16_t)wid].Name.c_str() : "?",
               fid, content->WorldFloors->Entries.count((uint16_t)fid) ? content->WorldFloors->Entries[(uint16_t)fid].Name.c_str() : "?");
    }
    if (getenv("SIMRENDER_FLOORS")) {   // histogram of floor patterns on the viewed level (debug)
        std::map<int, int> hist; std::map<int, std::array<int, 4>> box;
        auto& fl = arch->Floors[(size_t)(level - 1)];
        for (size_t i = 0; i < fl.size(); i++) { int pt = (int)fl[i].Pattern; int x = (int)(i % arch->Width), y = (int)(i / arch->Width);
            if (!hist[pt]++) box[pt] = {x, y, x, y};
            auto& b = box[pt]; b[0] = std::min(b[0], x); b[1] = std::min(b[1], y); b[2] = std::max(b[2], x); b[3] = std::max(b[3], y); }
        for (auto& h : hist) printf("floor %d: %d tiles (%d,%d-%d,%d) %s\n", h.first, h.second, box[h.first][0], box[h.first][1], box[h.first][2], box[h.first][3],
            content->WorldFloors->Entries.count((uint16_t)h.first) ? content->WorldFloors->Entries[(uint16_t)h.first].Name.c_str() : "");
    }
    Framebuffer fb(width, height);
    // SIMRENDER_SEQ="every,count": time-lapse — tick `every` VM updates between frames, write <out>_NNN.png
    if (const char* seq = getenv("SIMRENDER_SEQ")) {
        int every = 600, count = 60; sscanf(seq, "%d,%d", &every, &count);
        std::string base = argv[4]; if (base.size() > 4 && base.substr(base.size() - 4) == ".png") base.resize(base.size() - 4);
        std::vector<uint8_t> rgba((size_t)width * height * 4);
        for (int f = 0; f < count; f++) {
            for (int i = 0; i < every; i++) vm.Update();
            world.Draw(fb);
            for (size_t i = 0; i < fb.Color.size(); i++) { uint32_t c = fb.Color[i];
                rgba[i*4] = (uint8_t)c; rgba[i*4+1] = (uint8_t)(c >> 8); rgba[i*4+2] = (uint8_t)(c >> 16); rgba[i*4+3] = 255; }
            char nm[1024]; snprintf(nm, sizeof nm, "%s_%03d.png", base.c_str(), f);
            st::writePNG(nm, width, height, rgba.data());
            char tm[64]; snprintf(tm, sizeof tm, "%s_%03d.txt", base.c_str(), f);
            if (FILE* t = fopen(tm, "w")) { fprintf(t, "%02d:%02d\n", ctx.Clock->Hours, ctx.Clock->Minutes); fclose(t); }
        }
        printf("wrote %d time-lapse frames\n", count);
        return 0;
    }
    // SIMRENDER_SCROLL="dxTiles,dyTiles,frames": scroll the view a little every frame (static layer strip redraws)
    if (const char* sc = getenv("SIMRENDER_SCROLL")) {
        float dx = 0.1f, dy = 0.0f; int n = 30; sscanf(sc, "%f,%f,%d", &dx, &dy, &n);
        world.Draw(fb);
        double total = 0, worst = 0;
        int full0 = world.Get2DWorld().StaticRedraws, strips0 = world.Get2DWorld().StripRedraws; long long px0 = world.Get2DWorld().StripPixels;
        for (int f = 0; f < n; f++) {
            world.State.CenterTile(world.State.CenterTile() + st::Vector2(dx, dy));
            double t1 = NowMs();
            world.Draw(fb);
            double t = NowMs() - t1; total += t; worst = std::max(worst, t);
        }
        printf("scroll: %d frames, avg %.2f ms, worst %.2f ms, %d full layer redraws, %d strip redraws (%lld px)\n", n, total / n, worst,
               world.Get2DWorld().StaticRedraws - full0, world.Get2DWorld().StripRedraws - strips0, (long long)(world.Get2DWorld().StripPixels - px0));
    }
    // SIMRENDER_LIVE="ticksPerFrame": run the VM between frames (Sims animate, objects change state)
    int liveTicks = getenv("SIMRENDER_LIVE") ? atoi(getenv("SIMRENDER_LIVE")) : 0;
    for (int f = 0; f < frames; f++) {
        for (int i = 0; i < liveTicks; i++) vm.Update();
        double t1 = NowMs();
        int before = world.Get2DWorld().StaticRedraws;
        world.Draw(fb);
        if (f < 3 || f == frames - 1 || getenv("SIMRENDER_VERBOSE"))
        printf("frame %d: %.1f ms (%d static layer redraws, %d dynamic sprites)\n", f, NowMs() - t1,
               world.Get2DWorld().StaticRedraws - before, world.Batch().SpritesDrawn);
    }
    int dynamicObjs = 0;
    for (auto o : blueprint->Objects) if (o->renderInfo.Layer == WorldObjectRenderLayer::DYNAMIC) dynamicObjs++;
    printf("objects on dynamic layer: %d; static layer full redraws %d, strip %d, object-area %d (%lld px)\n", dynamicObjs,
           world.Get2DWorld().StaticRedraws, world.Get2DWorld().StripRedraws, world.Get2DWorld().DirtyRedraws, (long long)world.Get2DWorld().StripPixels);
    if (world.State.DrawRoofs) printf("roof: style %u pitch %.2f, %zu rects, %d triangles, %d px\n", blueprint->RoofComp.Style, blueprint->RoofComp.Pitch,
        blueprint->RoofComp.RectCount(), blueprint->RoofComp.LastStats.Triangles, blueprint->RoofComp.LastStats.Pixels);

    // Framebuffer colour is R,G,B,A bytes in memory on little-endian hosts
    std::vector<uint8_t> rgba((size_t)width * height * 4);
    for (size_t i = 0; i < fb.Color.size(); i++) {
        uint32_t c = fb.Color[i];
        rgba[i * 4 + 0] = (uint8_t)c; rgba[i * 4 + 1] = (uint8_t)(c >> 8); rgba[i * 4 + 2] = (uint8_t)(c >> 16); rgba[i * 4 + 3] = 255;
    }
    if (!st::writePNG(argv[4], width, height, rgba.data())) { printf("png write failed\n"); return 1; }
    printf("wrote %s (%dx%d, zoom %s, rotation %d, walls %s)\n", argv[4], width, height, zoomName.c_str(), rotation & 3, walls.c_str());
    return 0;
}
