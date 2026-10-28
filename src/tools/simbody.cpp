/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// simbody - render a TS1 Sim with the ported Vitaboy body system + software rasterizer (host tool).
//
//   simbody <installDir> <outDir> [options]
//     --char <User*.iff>          take body/head/hands/skin from a character file (STR#200 body strings)
//     --body <suit> --bodytex <t> body suit + texture (e.g. b002mafat_01 B002MAFatlgt_slob)
//     --head <suit> --headtex <t> head suit + texture (e.g. c010ma_baldbeard C010MAlgt_baldbeard01)
//     --hands <m|f|u>             default hand set (HmLO... / HfLO... / HuLO...) when not from --char
//     --idle <anim> --walk <anim> animation names (default a2o-standing-loop / a2o-walking-loop)
//     --frames <n>                walk frames to dump (default 8)
//
// Writes: idle_near.png (world Near zoom, with a fake "2D world" depth occluder to show the
// z-test), idle_closeup.png, walk_near_strip.png, walk_closeup_NN.png and walk_closeup_strip.png.
#include "vitaboy/Appearance.h"
#include "vitaboy/AvatarRenderer.h"
#include "vitaboy/content/TS1AvatarProvider.h"
#include "files/formats/iff/IffFile.h"
#include "files/formats/iff/chunks/STR.h"
#include "core/png.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace FSO::Vitaboy;
using FSO::Content::TS1::TS1AvatarContent;

struct Canvas {
    int w, h;
    std::vector<uint8_t> px;
    std::vector<float> z;
    Canvas(int W, int H) : w(W), h(H), px((size_t)W * H * 4), z((size_t)W * H, 1.0f) {}
    Framebuffer FB() { return Framebuffer(w, h, px.data()); }
    ZBuffer ZB() { return ZBuffer(w, h, z.data()); }
    void Fill(uint8_t r, uint8_t g, uint8_t b) {
        for (int i = 0; i < w * h; i++) { px[i*4] = r; px[i*4+1] = g; px[i*4+2] = b; px[i*4+3] = 255; }
        std::fill(z.begin(), z.end(), 1.0f);
    }
    void Put(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
        if (x < 0 || y < 0 || x >= w || y >= h) return;
        uint8_t* p = &px[((size_t)y * w + x) * 4]; p[0] = r; p[1] = g; p[2] = b; p[3] = 255;
    }
    void Line(float x0, float y0, float x1, float y1, uint8_t r, uint8_t g, uint8_t b) {
        int n = (int)std::max(std::abs(x1 - x0), std::abs(y1 - y0)) + 1;
        for (int i = 0; i <= n; i++) { float t = (float)i / n; Put((int)(x0 + (x1 - x0) * t), (int)(y0 + (y1 - y0) * t), r, g, b); }
    }
    bool Save(const std::string& path) { return st::writePNG(path, w, h, px.data()); }
};

// iso floor grid around the centre tile (the 2D world would draw floor sprites here)
static void DrawGrid(Canvas& c, const WorldCamera& cam, int radius, const Vector3& at) {
    int cx = (int)at.X, cy = (int)at.Y;
    for (int i = -radius; i <= radius + 1; i++) {
        auto a = cam.ProjectTile(Vector3((float)(cx + i), (float)(cy - radius), 0));
        auto b = cam.ProjectTile(Vector3((float)(cx + i), (float)(cy + radius + 1), 0));
        c.Line(a.X, a.Y, b.X, b.Y, 90, 120, 90);
        a = cam.ProjectTile(Vector3((float)(cx - radius), (float)(cy + i), 0));
        b = cam.ProjectTile(Vector3((float)(cx + radius + 1), (float)(cy + i), 0));
        c.Line(a.X, a.Y, b.X, b.Y, 90, 120, 90);
    }
}

// A fake 2D-world object in front of the Sim: a box-shaped region whose depth is written the way
// the 2D world batch writes sprite depth (camera depth of the object's world position).
static void DrawOccluder(Canvas& c, const WorldCamera& cam, const Vector3& tileCorner, float sizeTiles, float heightTiles) {
    // depth of the front face plane: use the nearest corner, like a flat sprite at that tile
    auto p0 = cam.ProjectTile(Vector3(tileCorner.X, tileCorner.Y, 0));
    auto p1 = cam.ProjectTile(Vector3(tileCorner.X + sizeTiles, tileCorner.Y + sizeTiles, heightTiles));
    auto pl = cam.ProjectTile(Vector3(tileCorner.X, tileCorner.Y + sizeTiles, 0));
    auto pr = cam.ProjectTile(Vector3(tileCorner.X + sizeTiles, tileCorner.Y, 0));
    int x0 = (int)std::min(pl.X, pr.X), x1 = (int)std::max(pl.X, pr.X);
    int y0 = (int)std::min(p0.Y, p1.Y) - 1, y1 = (int)std::max({ p0.Y, pl.Y, pr.Y, cam.ProjectTile(Vector3(tileCorner.X + sizeTiles, tileCorner.Y + sizeTiles, 0)).Y });
    float d = cam.DepthOfTile(Vector3(tileCorner.X + sizeTiles, tileCorner.Y + sizeTiles, 0)); // nearest corner
    for (int y = std::max(0, y0); y < std::min(c.h, y1); y++)
        for (int x = std::max(0, x0); x < std::min(c.w, x1); x++) {
            uint8_t* p = &c.px[((size_t)y * c.w + x) * 4];
            p[0] = 150; p[1] = 100; p[2] = 60; p[3] = 255;
            c.z[(size_t)y * c.w + x] = d;
        }
}

// CenterTile that puts the point `heightTiles` above pos at the middle of the view (TopLeft rotation:
// moving the centre by (-k,-k) tiles moves the view up by 64k px at Near; z is 78.4 px per tile).
static Vector2 CenterOn(const Vector3& pos, float heightTiles) {
    float k = heightTiles * 78.38f / 64.0f;
    return Vector2(pos.X - k, pos.Y - k);
}

static void Pose(SimAvatar& avatar, Animation* anim, float frame) {
    avatar.ResetPose();
    if (anim) Animator::RenderFrame(&avatar, anim, (int)frame, frame - (int)frame, 1.0f);
    avatar.ReloadSkeleton();
}

static std::string Arg(int argc, char** argv, const char* name, const std::string& def) {
    for (int i = 1; i + 1 < argc; i++) if (strcmp(argv[i], name) == 0) return argv[i + 1];
    return def;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: simbody <installDir> <outDir> [--char User00000.iff] [--body suit --bodytex tex] [--head suit --headtex tex]\n");
        return 1;
    }
    std::string base = argv[1], out = argv[2];
    TS1AvatarContent::SetBasePath(base);
    auto content = TS1AvatarContent::Get();
    if (!content->EnsureInit()) { fprintf(stderr, "no TS1 avatar data under %s\n", base.c_str()); return 1; }
    printf("index: %zu skeletons, %zu suits, %zu skills\n", content->BCF.SkelHostBCF.size(), content->BCF.SkinHostBCF.size(), content->BCF.AnimHostBCF.size());

    std::string charFile = Arg(argc, argv, "--char", "");
    std::string bodyStr, headStr, hands[6], skin = "lgt", type = "adult";
    if (!charFile.empty()) {
        FSO::Files::Formats::IFF::IffFile iff(charFile);
        auto str = iff.Get<FSO::Files::Formats::IFF::Chunks::STR>(200);
        if (!str) { fprintf(stderr, "%s: no STR#200 body strings\n", charFile.c_str()); return 1; }
        type = str->GetString(0);
        bodyStr = str->GetString(1);
        headStr = str->GetString(2);
        for (int i = 0; i < 6; i++) hands[i] = str->GetString(17 + i);
        skin = str->GetString(14);
        printf("%s: type=%s body='%s' head='%s' skin=%s\n", charFile.c_str(), type.c_str(), bodyStr.c_str(), headStr.c_str(), skin.c_str());
    }
    auto body = Arg(argc, argv, "--body", ""), head = Arg(argc, argv, "--head", "");
    if (!body.empty()) bodyStr = body + ",BODY=" + Arg(argc, argv, "--bodytex", "");
    if (!head.empty()) headStr = head + ",HEAD-HEAD=" + Arg(argc, argv, "--headtex", "");
    if (bodyStr.empty()) bodyStr = "b002mafat_01,BODY=B002MAFatlgt_slob";
    if (headStr.empty()) headStr = "c010ma_baldbeard,HEAD-HEAD=C010MAlgt_baldbeard01";
    if (hands[0].empty()) {
        std::string m = Arg(argc, argv, "--hands", "m");
        const char* codes[6] = { "LO", "RO", "LP", "RP", "LC", "RC" };
        const char* tex[6] = { "huaolgt", "huaolgt", "huaplgt", "huaplgt", "huaclgt", "huaclgt" };
        for (int i = 0; i < 6; i++) hands[i] = "H" + m + codes[i] + ",HAND=" + tex[i];
    }

    auto skel = content->BCF.GetSkeleton(type == "child" ? "child.skel" : "adult.skel");
    if (!skel) { fprintf(stderr, "skeleton missing\n"); return 1; }
    SimAvatar avatar(skel);
    Outfit headOft, bodyOft;
    headOft.ReadHead(headStr);
    bodyOft.Read(bodyStr, hands);
    avatar.Head = &headOft;
    avatar.Body = &bodyOft;
    avatar.Handgroup = &bodyOft;
    printf("bindings: %zu\n", avatar.Bindings.size());
    for (auto& b : avatar.Bindings)
        printf("  mesh %-44s verts %4zu faces %4d tex %s\n", b->Mesh->SkinName.c_str(), b->Mesh->VertexBuffer.size(), b->Mesh->NumPrimitives,
               b->Texture ? (b->Texture->Name + " " + std::to_string(b->Texture->Width) + "x" + std::to_string(b->Texture->Height)).c_str() : "(missing)");

    auto idle = content->BCF.GetAnimation(Arg(argc, argv, "--idle", "a2o-standing-loop") + ".anim");
    auto walk = content->BCF.GetAnimation(Arg(argc, argv, "--walk", "a2o-walking-loop") + ".anim");
    printf("idle: %s (%d frames)  walk: %s (%d frames, %.0f ms)\n", idle ? idle->Name.c_str() : "missing", idle ? idle->NumFrames() : 0,
           walk ? walk->Name.c_str() : "missing", walk ? walk->NumFrames() : 0, walk ? walk->Duration : 0.f);
    float dir = (float)atof(Arg(argc, argv, "--dir", "0.6").c_str()); // radians (AvatarComponent.RadianDirection)
    Vector3 pos(10.5f, 10.5f, 0.0f); // tile centre

    // ---- idle at the world's Near zoom, with a 2D-world occluder in front (depth test) ----
    {
        Canvas c(256, 256);
        c.Fill(60, 70, 60);
        WorldCamera cam;
        cam.Zoom = WorldZoom::Near;
        cam.CenterTile = CenterOn(pos, 0.75f);
        cam.ViewDimensions = Vector2((float)c.w, (float)c.h);
        DrawGrid(c, cam, 2, pos);
        DrawOccluder(c, cam, Vector3(10.8f, 10.8f, 0), 0.4f, 0.9f);
        Pose(avatar, idle, 0);
        auto fb = c.FB(); auto zb = c.ZB();
        auto r = DrawAvatar(fb, zb, cam, avatar, pos, dir);
        printf("near: %d tris, %d drawn, %d px, bounds (%d,%d)-(%d,%d)\n", r.Triangles, r.TrianglesDrawn, r.Pixels, r.MinX, r.MinY, r.MaxX, r.MaxY);
        c.Save(out + "/idle_near.png");
    }
    // ---- idle close-up (PreciseZoom 4, bilinear like XNA's Linear sampler) ----
    {
        Canvas c(400, 480);
        c.Fill(40, 44, 52);
        WorldCamera cam;
        cam.Zoom = WorldZoom::Near;
        cam.PreciseZoom = 3.0f;
        cam.CenterTile = CenterOn(pos, 0.9f);
        cam.ViewDimensions = Vector2((float)c.w, (float)c.h);
        DrawGrid(c, cam, 1, pos);
        Pose(avatar, idle, 0);
        AvatarDrawOptions o; o.Bilinear = true;
        auto fb = c.FB(); auto zb = c.ZB();
        DrawAvatar(fb, zb, cam, avatar, pos, dir, o);
        c.Save(out + "/idle_closeup.png");
    }
    // ---- walk cycle ----
    int frames = atoi(Arg(argc, argv, "--frames", "8").c_str());
    if (walk && frames > 0) {
        int n = walk->NumFrames();
        Canvas strip(128 * frames, 192);
        strip.Fill(60, 70, 60);
        Canvas cstrip(200 * frames, 300);
        cstrip.Fill(40, 44, 52);
        for (int i = 0; i < frames; i++) {
            float f = (float)i * n / frames;
            Pose(avatar, walk, f);
            {
                Canvas c(128, 192); c.Fill(60, 70, 60);
                WorldCamera cam; cam.Zoom = WorldZoom::Near; cam.CenterTile = CenterOn(pos, 0.9f);
                cam.ViewDimensions = Vector2((float)c.w, (float)c.h);
                DrawGrid(c, cam, 1, pos);
                auto fb = c.FB(); auto zb = c.ZB();
                DrawAvatar(fb, zb, cam, avatar, pos, dir);
                for (int y = 0; y < c.h; y++) memcpy(&strip.px[((size_t)y * strip.w + i * 128) * 4], &c.px[(size_t)y * c.w * 4], 128 * 4);
            }
            {
                Canvas c(200, 300); c.Fill(40, 44, 52);
                WorldCamera cam; cam.Zoom = WorldZoom::Near; cam.PreciseZoom = 2.0f; cam.CenterTile = CenterOn(pos, 0.9f);
                cam.ViewDimensions = Vector2((float)c.w, (float)c.h);
                DrawGrid(c, cam, 1, pos);
                AvatarDrawOptions o; o.Bilinear = true;
                auto fb = c.FB(); auto zb = c.ZB();
                DrawAvatar(fb, zb, cam, avatar, pos, dir, o);
                char name[64]; snprintf(name, sizeof(name), "/walk_closeup_%02d.png", i);
                c.Save(out + name);
                for (int y = 0; y < c.h; y++) memcpy(&cstrip.px[((size_t)y * cstrip.w + i * 200) * 4], &c.px[(size_t)y * c.w * 4], 200 * 4);
            }
        }
        strip.Save(out + "/walk_near_strip.png");
        cstrip.Save(out + "/walk_closeup_strip.png");
    }
    // ---- --bench N: time pose + skin + raster at Near zoom (400x240, a 3DS top screen) ----
    int bench = atoi(Arg(argc, argv, "--bench", "0").c_str());
    if (bench > 0 && walk) {
        Canvas c(400, 240);
        WorldCamera cam; cam.Zoom = WorldZoom::Near; cam.CenterTile = CenterOn(pos, 0.9f);
        cam.ViewDimensions = Vector2((float)c.w, (float)c.h);
        auto fb = c.FB(); auto zb = c.ZB();
        auto t0 = std::chrono::steady_clock::now();
        long px = 0;
        for (int i = 0; i < bench; i++) {
            Pose(avatar, walk, (float)(i % walk->NumFrames()));
            zb.Clear();
            px += DrawAvatar(fb, zb, cam, avatar, pos, dir).Pixels;
        }
        auto t1 = std::chrono::steady_clock::now();
        printf("bench: %.3f ms per pose+draw (%ld px/draw)\n", std::chrono::duration<double, std::milli>(t1 - t0).count() / bench, px / bench);
    }
    printf("texture memory: %zu KB, BCF files parsed: %zu\n", content->AvatarTextures.MemoryBytes() / 1024, content->BCF.LoadedFiles());
    return 0;
}
