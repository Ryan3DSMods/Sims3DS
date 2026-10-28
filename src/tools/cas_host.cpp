/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// cas_host — host driver for Create-A-Family / Create-A-Sim (src/app/cas), like sim3ds_host for the live game:
// runs CASScreen with scripted input and writes both screens (top 400x240 above the 320x240 touch screen).
//
//   cas_host <dataRoot> <outDir> script <file|->     scripted CAS session (commands below)
//   cas_host <dataRoot> <outDir> catalog             catalogue statistics
//   cas_host <dataRoot> <outDir> movein F H         FAMI F moves into house H (Neighborhood.iff)
//   cas_host <dataRoot> <outDir> roundtrip           NBRS read/write round trip of <dataRoot>/UserData*/Neighborhood.iff
//
// Script commands (one per line, '#' comments; 30 frames per second of simulated time):
//   frames N | tap X Y | drag X0 Y0 X1 Y1 N | press BTN [N] | circle DX DY N
//   text STRING        answer the next keyboard request with STRING (host stand-in for the 3DS swkbd)
//   button ID          tap the CAS control ID (see CASScreen::ScriptButtonPos)
//   seed N             random seed for the session
//   shot NAME          write <outDir>/NAME.png
//   expect done|cancel fail (exit 3) unless the screen finished that way
//   log                print the CAS log
#include "app/cas/CASScreen.h"
#include "app/cas/CASCatalog.h"
#include "app/cas/CASFamilyWriter.h"
#include "core/png.h"
#include "core/io.h"
#include "files/formats/iff/chunks/NBRS.h"
#include "vitaboy/content/TS1AvatarProvider.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace simapp;
using namespace simapp::cas;

static bool SavePair(const Canvas& top, const Canvas& bot, const std::string& path) {
    int W = 400, H = 480;
    std::vector<uint8_t> rgba((size_t)W * H * 4, 0);
    auto put = [&](const Canvas& c, int ox, int oy) {
        for (int y = 0; y < c.Height; y++)
            for (int x = 0; x < c.Width; x++) {
                uint32_t p = c.Px[(size_t)y * c.Width + x];
                uint8_t* d = &rgba[((size_t)(oy + y) * W + ox + x) * 4];
                d[0] = (uint8_t)p; d[1] = (uint8_t)(p >> 8); d[2] = (uint8_t)(p >> 16); d[3] = 255;
            }
    };
    for (size_t i = 0; i < rgba.size(); i += 4) { rgba[i] = 18; rgba[i + 1] = 18; rgba[i + 2] = 22; rgba[i + 3] = 255; }
    put(top, 0, 0);
    put(bot, 40, 240);
    return st::writePNG(path, W, H, rgba.data());
}

static uint32_t ButtonBit(const std::string& n) {
    static const char* names[] = { "A", "B", "X", "Y", "L", "R", "ZL", "ZR", "START", "SELECT", "UP", "DOWN", "LEFT", "RIGHT" };
    for (int i = 0; i < 14; i++) if (n == names[i]) return 1u << i;
    return 0;
}

static int RoundTrip(const std::string& root) {
    using FSO::Files::Formats::IFF::Chunks::NBRS;
    int fails = 0;
    for (auto ud : { "UserData", "UserData2" }) {
        std::string path = root + "/" + ud + "/Neighborhood.iff";
        std::vector<uint8_t> bytes;
        std::vector<CASRawChunk> chunks;
        if (!CASFamilyWriter::ReadFile(path, bytes) || !CASFamilyWriter::ReadIff(bytes, chunks)) { printf("%s: missing\n", path.c_str()); continue; }
        for (auto& c : chunks) {
            if (c.Type != "NBRS") continue;
            NBRS a, b;
            st::Reader r(c.Data, st::Endian::Big);
            a.Read(nullptr, r);
            st::Writer w(st::Endian::Big);
            a.Write(nullptr, w);
            auto out = w.ToArray();
            st::Reader r2(out, st::Endian::Big);
            b.Read(nullptr, r2);
            bool same = out.size() == c.Data.size() && a.Entries.size() == b.Entries.size();
            for (auto& kv : a.NeighbourByID) {
                auto it = b.NeighbourByID.find(kv.first);
                if (it == b.NeighbourByID.end() || it->second->Name != kv.second->Name || it->second->GUID != kv.second->GUID ||
                    it->second->PersonData != kv.second->PersonData || it->second->Relationships != kv.second->Relationships) same = false;
            }
            size_t diff = 0;
            for (size_t i = 0; i < std::min(out.size(), c.Data.size()); i++) if (out[i] != c.Data[i]) diff++;
            printf("%s NBRS: %zu bytes -> %zu bytes, %zu neighbours, %zu differing bytes (relationship order), %s\n", ud, c.Data.size(),
                   out.size(), a.NeighbourByID.size(), diff, same ? "OK" : "MISMATCH");
            if (!same) fails++;
        }
        // whole-file rewrite (drops XXXX, rebuilds rsmp) must re-read to the same chunks
        auto rewritten = CASFamilyWriter::WriteIff(chunks);
        std::vector<CASRawChunk> again;
        CASFamilyWriter::ReadIff(rewritten, again);
        size_t kept = 0;
        for (auto& c : chunks) if (c.Type != "XXXX" && c.Type != "rsmp") kept++;
        printf("%s rewrite: %zu chunks (%zu without XXXX/rsmp) -> %zu + rsmp\n", ud, chunks.size(), kept, again.size() - 1);
    }
    return fails ? 3 : 0;
}

int main(int argc, char** argv) {
    if (argc < 4) {
        fprintf(stderr, "usage: cas_host <dataRoot> <outDir> script <file|-> | catalog | roundtrip\n");
        return 2;
    }
    std::string root = argv[1], out = argv[2], mode = argv[3];
    FSO::Content::TS1::TS1AvatarContent::SetBasePath(root);
    if (mode == "roundtrip") return RoundTrip(root);
    if (mode == "movein" && argc >= 6) { // cas_host <root> <out> movein <familyId> <house>  (UserData or $CAS_USERDATA)
        std::string ud = getenv("CAS_USERDATA") ? getenv("CAS_USERDATA") : root + "/UserData", err;
        bool ok = CASFamilyWriter::MoveIn(ud, (int16_t)atoi(argv[4]), (int16_t)atoi(argv[5]), &err);
        printf("move family %s into house %s: %s %s\n", argv[4], argv[5], ok ? "ok" : "FAILED", err.c_str());
        return ok ? 0 : 1;
    }
    if (mode == "catalog") {
        CASCatalog cat;
        auto t0 = std::chrono::steady_clock::now();
        bool ok = cat.Build();
        double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        printf("catalog %s in %.0f ms: %zu heads, %zu bodies\n", ok ? "ok" : "FAILED", ms, cat.HeadCount(), cat.BodyCount());
        const char* ages[] = { "adult", "child" }; const char* gs[] = { "male", "female" };
        for (int a = 0; a < 2; a++) for (int g = 0; g < 2; g++) for (int s = 0; s < 3; s++) {
            auto& h = cat.Heads((CASAge)a, (CASGender)g, (CASSkin)s);
            auto& b = cat.Bodies((CASAge)a, (CASGender)g, (CASSkin)s);
            printf("  %s %-6s %s: %3zu heads (%s .. %s), %3zu bodies\n", ages[a], gs[g], SkinCode((CASSkin)s), h.size(),
                   h.empty() ? "-" : h.front().Texture.c_str(), h.empty() ? "-" : h.back().Texture.c_str(), b.size());
        }
        return ok ? 0 : 1;
    }
    if (mode != "script" || argc < 5) { fprintf(stderr, "unknown mode\n"); return 2; }

    std::string scriptPath = argv[4];
    std::istream* in = &std::cin;
    std::ifstream f;
    if (scriptPath != "-") { f.open(scriptPath); if (!f) { fprintf(stderr, "cannot open %s\n", scriptPath.c_str()); return 2; } in = &f; }

    CASScreen::Config cfg;
    cfg.DataRoot = root;
    cfg.UserDataDir = root + "/UserData";
    if (const char* ud = getenv("CAS_USERDATA")) cfg.UserDataDir = ud;
    CASScreen cas(cfg);
    std::vector<std::string> pendingText;
    cas.TextInput = [&](const CASTextRequest& req, std::string& result) {
        if (pendingText.empty()) { printf("[keyboard] '%s' requested, no scripted text -> cancelled\n", req.Hint.c_str()); return false; }
        result = pendingText.front();
        pendingText.erase(pendingText.begin());
        printf("[keyboard] '%s' -> \"%s\"\n", req.Hint.c_str(), result.c_str());
        return true;
    };
    Canvas top(400, 240), bot(320, 240);
    AppInput input;
    double frameMs = 1000.0 / 30.0;
    int frame = 0;
    auto step = [&](int n) {
        for (int i = 0; i < n; i++) {
            cas.Update(input, frameMs);
            input.Down = 0; input.TouchDown = false; input.TouchUp = false;
            frame++;
            if (!cas.Log.empty()) { printf("%s", cas.Log.c_str()); cas.Log.clear(); }
        }
    };
    auto tap = [&](int x, int y) {
        input.Touching = true; input.TouchDown = true; input.TouchX = x; input.TouchY = y;
        step(1);
        input.Touching = false; input.TouchUp = true;
        step(1);
    };
    std::string line;
    int lineNo = 0;
    while (std::getline(*in, line)) {
        lineNo++;
        auto hash = line.find('#');
        if (hash != std::string::npos) line = line.substr(0, hash);
        std::istringstream ss(line);
        std::string cmd;
        if (!(ss >> cmd)) continue;
        if (cmd == "frames") { int n = 1; ss >> n; step(n); }
        else if (cmd == "tap") { int x, y; ss >> x >> y; tap(x, y); }
        else if (cmd == "drag") {
            int x0, y0, x1, y1, n = 10; ss >> x0 >> y0 >> x1 >> y1 >> n;
            input.Touching = true; input.TouchDown = true; input.TouchX = x0; input.TouchY = y0; step(1);
            for (int i = 1; i <= n; i++) { input.TouchX = x0 + (x1 - x0) * i / n; input.TouchY = y0 + (y1 - y0) * i / n; step(1); }
            input.Touching = false; input.TouchUp = true; step(1);
        }
        else if (cmd == "press") {
            std::string b; int n = 1; ss >> b >> n;
            uint32_t bit = ButtonBit(b);
            input.Held |= bit; input.Down |= bit; step(n);
            input.Held &= ~bit; step(1);
        }
        else if (cmd == "circle") { float dx, dy; int n; ss >> dx >> dy >> n; input.CircleX = dx; input.CircleY = dy; step(n); input.CircleX = input.CircleY = 0; }
        else if (cmd == "text") { std::string rest; std::getline(ss, rest); size_t s = rest.find_first_not_of(' '); pendingText.push_back(s == std::string::npos ? "" : rest.substr(s)); }
        else if (cmd == "button") {
            std::string id; ss >> id; int x, y;
            if (!cas.ScriptButtonPos(id, x, y)) { fprintf(stderr, "line %d: no button '%s' on this screen\n", lineNo, id.c_str()); return 3; }
            tap(x, y);
        }
        else if (cmd == "seed") { unsigned s; ss >> s; cas.Seed(s); }
        else if (cmd == "shot") {
            std::string name; ss >> name;
            cas.Draw(top, bot);
            std::string p = out + "/" + name + ".png";
            if (!SavePair(top, bot, p)) fprintf(stderr, "cannot write %s\n", p.c_str());
            else printf("shot %s (frame %d)\n", p.c_str(), frame);
        }
        else if (cmd == "expect") {
            std::string what; ss >> what;
            bool ok = (what == "done" && cas.Done()) || (what == "cancel" && cas.Cancelled());
            if (!ok) { fprintf(stderr, "line %d: expected %s\n", lineNo, what.c_str()); return 3; }
            printf("expect %s: ok (family id %d)\n", what.c_str(), (int)cas.FamilyID());
        }
        else if (cmd == "log") { printf("%s", cas.Describe().c_str()); }
        else { fprintf(stderr, "line %d: unknown command %s\n", lineNo, cmd.c_str()); return 2; }
    }
    return 0;
}
