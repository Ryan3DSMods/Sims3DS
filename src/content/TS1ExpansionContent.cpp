/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// TS1(sims): see TS1ExpansionContent.h
#include "files/SafeFile.h" // FIX(sims) SAVE
#include "content/TS1ExpansionContent.h"
#include "content/ContentManager.h"
#include "files/formats/iff/IffFile.h"
#include "files/formats/iff/chunks/STR.h"
#include <cstdio>
#include <cstdlib>
#include <map>

namespace FSO { namespace Content { namespace TS1 {

using FSO::Files::Formats::IFF::IffFile;
using FSO::Files::Formats::IFF::Chunks::STR;

static std::string Clean(std::string s) {
    for (auto& c : s) if (c == '\t' || c == '\n' || c == '\r') c = ' ';
    return s;
}

static std::string KeyOf(const std::string& entry) {
    auto slash = entry.find_last_of("\\/");
    auto n = (slash == std::string::npos) ? entry : entry.substr(slash + 1);
    auto dot = n.find('.');
    if (dot != std::string::npos) n = n.substr(0, dot);
    return ToLowerInvariant(n);
}

std::string TS1ExpansionContent::MakeReference(const ArchiveItem& item)
{
    return "far:" + std::to_string(item.Far) + ":" + item.Entry;
}

std::unique_ptr<IffFile> TS1ExpansionContent::LoadArchiveIff(Content* content, const std::string& reference)
{
    if (!IsArchiveReference(reference) || content == nullptr || !content->WorldObjects) return nullptr;
    auto colon = reference.find(':', 4);
    if (colon == std::string::npos) return nullptr;
    int32_t far = atoi(reference.substr(4, colon - 4).c_str());
    auto entry = reference.substr(colon + 1);
    auto& archives = content->WorldObjects->Archives;
    if (far < 0 || far >= (int32_t)archives.size()) return nullptr;
    auto it = archives[(size_t)far].EntriesByName.find(ToLowerInvariant(entry));
    if (it == archives[(size_t)far].EntriesByName.end()) return nullptr;
    std::unique_ptr<IffFile> iff(new IffFile());
    try { iff->ReadFromFile(archives[(size_t)far].Path, it->second.DataOffset, (uint32_t)it->second.DataLength, IffFile::DEFER_HEAVY_CHUNKS); } // TS1(sims)
    catch (...) { return nullptr; }
    iff->Filename = entry;
    return iff;
}

std::vector<TS1ExpansionContent::ArchiveItem> TS1ExpansionContent::ListArchiveItems(Content* content, const std::string& ext)
{
    std::vector<ArchiveItem> result;
    if (content == nullptr || !content->WorldObjects) return result;
    auto& archives = content->WorldObjects->Archives;
    for (size_t fi = 0; fi < archives.size(); fi++) {
        std::map<std::string, std::string> sorted; // lower name -> real entry name, sorted (the far order is arbitrary)
        for (auto& kv : archives[fi].EntriesByName) {
            auto& l = kv.first;
            if (l.size() > ext.size() && l.compare(l.size() - ext.size(), ext.size(), ext) == 0) sorted[l] = kv.second.Filename;
        }
        for (auto& kv : sorted) {
            ArchiveItem item;
            item.Far = (int32_t)fi;
            item.Entry = kv.second;
            item.Key = KeyOf(kv.second);
            result.push_back(item);
        }
    }
    if (result.empty()) return result;

    // catalog strings: from the cache, else read every iff once and write the cache
    std::string cachePath = content->CacheDir + "/wallfloor" + ext.substr(1) + ".cache";
    std::map<std::string, ArchiveItem*> byRef;
    for (auto& r : result) byRef[MakeReference(r)] = &r;
    bool cached = false;
    std::vector<uint8_t> bytes;
    if (ReadAllBytes(cachePath, bytes)) {
        std::string text(bytes.begin(), bytes.end());
        size_t pos = text.find('\n');
        std::string header = text.substr(0, pos);
        // header: "STWF1 <archives> <items>" — rebuilt when the install changes
        if (pos != std::string::npos && header == "STWF1 " + std::to_string(archives.size()) + " " + std::to_string(result.size())) {
            cached = true;
            pos++;
            while (pos < text.size()) {
                size_t nl = text.find('\n', pos);
                if (nl == std::string::npos) nl = text.size();
                auto line = text.substr(pos, nl - pos);
                pos = nl + 1;
                std::vector<std::string> cols;
                size_t s = 0;
                while (true) {
                    auto t = line.find('\t', s);
                    cols.push_back(line.substr(s, (t == std::string::npos) ? std::string::npos : t - s));
                    if (t == std::string::npos) break;
                    s = t + 1;
                }
                if (cols.size() < 4) continue;
                auto it = byRef.find(cols[0]);
                if (it == byRef.end()) { cached = false; break; }
                it->second->Name = cols[1];
                it->second->Price = atoi(cols[2].c_str());
                it->second->Description = cols[3];
            }
        }
    }
    if (!cached) {
        for (auto& r : result) {
            auto iff = LoadArchiveIff(content, MakeReference(r));
            if (!iff) continue;
            auto cat = iff->Get<STR>(0);
            if (cat == nullptr) continue;
            if (cat->Length() > 0) r.Name = Clean(cat->GetString(0));
            if (cat->Length() > 1) r.Price = atoi(cat->GetString(1).c_str());
            if (cat->Length() > 2) r.Description = Clean(cat->GetString(2));
        }
        if (FILE* f = FSO::Files::SafeFile::BeginWrite(cachePath)) { // FIX(sims) SAVE
            fprintf(f, "STWF1 %u %u\n", (unsigned)archives.size(), (unsigned)result.size());
            for (auto& r : result) fprintf(f, "%s\t%s\t%d\t%s\n", MakeReference(r).c_str(), r.Name.c_str(), (int)r.Price, r.Description.c_str());
            FSO::Files::SafeFile::EndWrite(cachePath, f);
        }
    }
    return result;
}

}}}
