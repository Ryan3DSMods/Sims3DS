/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Host-only smoke test for the sims.files translation.
//   selftest <file.iff>                 - plain IFF file
//   selftest <archive.far> <member.iff> - IFF stored inside a FAR1 archive (e.g. Objects.far AlarmClock.iff)
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include "files/formats/iff/IffFile.h"
#include "files/formats/iff/chunks/OBJD.h"
#include "files/formats/iff/chunks/SPR2.h"
#include "files/formats/iff/chunks/SPR.h"
#include "files/formats/iff/chunks/STR.h"
#include "files/formats/iff/chunks/TTAs.h"
#include "files/formats/iff/chunks/DGRP.h"
#include "files/formats/iff/chunks/TTAB.h"
#include "files/formats/iff/chunks/BHAV.h"
#include "files/formats/iff/chunks/ARRY.h"
#include "files/formats/iff/chunks/OBJM.h"
#include "files/FAR1/FAR1Archive.h"

using namespace FSO::Files::Formats::IFF;
using namespace FSO::Files::Formats::IFF::Chunks;

int main(int argc, char** argv)
{
    if (argc < 2) { std::fprintf(stderr, "usage: %s <file.iff> | <archive.far> <member.iff>\n", argv[0]); return 2; }
    try
    {
        std::unique_ptr<IffFile> iffp;
        if (argc >= 3)
        {
            FSO::Files::FAR1::FAR1Archive far(argv[1], false);
            bool found = false;
            auto data = far.GetEntry(std::string(argv[2]), &found);
            if (!found) { std::fprintf(stderr, "member %s not found in %s (%u entries)\n", argv[2], argv[1], far.NumFiles()); return 1; }
            iffp.reset(new IffFile());
            iffp->Read(data);
            iffp->SetFilename(argv[2]);
            std::printf("%s: %u entries; member %s = %zu bytes\n", argv[1], far.NumFiles(), argv[2], data.size());
        }
        else
        {
            iffp.reset(new IffFile(argv[1]));
            std::printf("%s\n", argv[1]);
        }
        IffFile& iff = *iffp;

        // chunk histogram
        std::map<std::string, int> hist;
        for (auto c : iff.SilentListAll()) hist[c->ChunkType]++;
        std::printf("chunk histogram:\n");
        for (auto& kv : hist) std::printf("  %-4s %d\n", kv.first.c_str(), kv.second);
        // decode everything (exercises every chunk reader)
        int decoded = 0;
        for (auto c : iff.ListAll()) { (void)c; decoded++; }
        std::printf("decoded %d chunks\n", decoded);

        auto spr2s = iff.List<SPR2>();
        if (!spr2s.empty() && !spr2s[0]->Frames.empty())
        {
            auto f = spr2s[0]->Frames[0].get();
            f->DecodeIfRequired();
            auto pxs = f->ExpandColors(); // FIX(sims): palettised frames
            int opaque = 0; for (auto& px : pxs) if (px.A) opaque++;
            std::printf("SPR2 #%u frame 0: %dx%d pos(%g,%g) flags=%u palette=%u pixels=%zu opaque=%d z=%zu\n",
                spr2s[0]->ChunkID, f->Width, f->Height, f->Position.X, f->Position.Y, f->Flags, f->PaletteID, pxs.size(), opaque, f->ZBufferData.size());
            // decode every SPR2 frame in the file
            size_t frames = 0, pixels = 0;
            for (auto s : spr2s) for (auto& fr : s->Frames) { fr->DecodeIfRequired(); frames++; pixels += fr->PalData.size(); }
            std::printf("decoded %zu SPR2 frames (%zu pixels total)\n", frames, pixels);
        }
        auto sprs = iff.List<SPR>();
        if (!sprs.empty() && !sprs[0]->Frames.empty())
        {
            auto f = sprs[0]->Frames[0].get();
            f->DecodeIfRequired();
            std::printf("SPR# #%u frame 0: %dx%d\n", sprs[0]->ChunkID, f->Width, f->Height);
        }
        for (auto o : iff.List<OBJD>())
            std::printf("OBJD #%u '%s' GUID=%08X type=%d master=%u graphic=%u numgfx=%u price=%u version=%u\n", o->ChunkID, o->ChunkLabel.c_str(), o->GUID, (int)o->ObjectType, o->MasterID, o->BaseGraphicID, o->NumGraphics, o->Price, o->Version);
        for (auto t : iff.List<TTAB>())
        {
            auto ttas = iff.Get<TTAs>(t->ChunkID);
            std::printf("TTAB #%u: %zu interactions\n", t->ChunkID, t->Interactions.size());
            for (auto& i : t->Interactions)
                std::printf("   action=%u test=%u tta=%u '%s'\n", i->ActionFunction, i->TestFunction, i->TTAIndex, ttas ? ttas->GetString((int32_t)i->TTAIndex).c_str() : "?");
        }
        for (auto d : iff.List<DGRP>())
        {
            auto img = d->GetImage(0x10, 2, 0);
            std::printf("DGRP #%u: %zu images; LeftFront near: %zu sprites\n", d->ChunkID, d->Images.size(), img ? img->Sprites.size() : 0);
            break;
        }
        for (auto b : iff.List<BHAV>()) { std::printf("BHAV #%u '%s': %zu instructions\n", b->ChunkID, b->ChunkLabel.c_str(), b->Instructions.size()); break; }
        for (auto a : iff.List<ARRY>())
            std::printf("Arry #%u %dx%d type=%d bytes=%zu\n", a->ChunkID, a->Width, a->Height, (int)a->Type, a->Data.size());
        for (auto m : iff.List<OBJM>())
            std::printf("ObjM #%u objt-table=%zu mapped-objects=%zu\n", m->ChunkID, m->IDToOBJT.size(), m->ObjectData.size());
        return 0;
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }
}
