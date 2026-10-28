/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/formats/iff/PIFFRegistry.cs
#include "files/formats/iff/PIFFRegistry.h"
#include "files/formats/iff/IffFile.h"
#include "files/formats/iff/chunks/PIFF.h"
#include "files/formats/iff/chunks/OBJD.h"
// Directory scanning: POSIX dirent on host and devkitPro (libctru/libsysbase). The bare
// arm-none-eabi newlib used for the cross-compile check has no dirent, so there the scan is
// compiled out (STUB(3ds): no PIFFs are discovered).
#if !defined(__3DS__) || __has_include(<3ds.h>)
#define PIFF_HAVE_DIRENT 1
#include <dirent.h>
#endif
#include <sys/stat.h>
#include <algorithm>

namespace FSO { namespace Files { namespace Formats { namespace IFF {

using namespace Chunks;

std::unordered_map<std::string, std::vector<IffFile*>> PIFFRegistry::PIFFsByName;
std::unordered_map<std::string, std::string> PIFFRegistry::OtfRewrite;
std::unordered_map<std::string, bool> PIFFRegistry::IsPIFFUser;
std::unordered_set<std::string> PIFFRegistry::OBJDAdded;
std::vector<std::unique_ptr<IffFile>> PIFFRegistry::OwnedPiffs;

namespace {
    bool DirectoryExists(const std::string& path)
    {
        struct stat st;
        return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
    }

    bool EndsWithNoCase(const std::string& s, const std::string& suffix)
    {
        if (s.size() < suffix.size()) return false;
        for (size_t i = 0; i < suffix.size(); i++)
        {
            char a = s[s.size() - suffix.size() + i], b = suffix[i];
            if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
            if (a != b) return false;
        }
        return true;
    }

    // Directory.GetFiles(basePath, "*.ext", SearchOption.AllDirectories)
    void GetFiles(const std::string& dir, const std::string& ext, std::vector<std::string>& out)
    {
#ifndef PIFF_HAVE_DIRENT
        (void)dir; (void)ext; (void)out;
        return;
#else
        DIR* d = opendir(dir.c_str());
        if (!d) return;
        struct dirent* ent;
        while ((ent = readdir(d)) != nullptr)
        {
            std::string name = ent->d_name;
            if (name == "." || name == "..") continue;
            std::string full = dir + "/" + name;
            struct stat st;
            if (stat(full.c_str(), &st) != 0) continue;
            if (S_ISDIR(st.st_mode)) GetFiles(full, ext, out);
            else if (EndsWithNoCase(name, ext)) out.push_back(full);
        }
        closedir(d);
        std::sort(out.begin(), out.end());
#endif
    }

    std::string GetFileName(const std::string& path)
    {
        size_t slash = path.find_last_of("/\\");
        return slash == std::string::npos ? path : path.substr(slash + 1);
    }
}

void PIFFRegistry::Init(const std::string& basePath)
{
    if (!DirectoryExists(basePath)) return;
    std::vector<std::string> paths;
    GetFiles(basePath, ".piff", paths);
    for (size_t i = 0; i < paths.size(); i++)
    {
        std::string entry = paths[i];
        std::replace(entry.begin(), entry.end(), '\\', '/');
        bool user = entry.find("User/") != std::string::npos;
        std::string filename = GetFileName(entry);
        (void)filename;

        PIFF* piff;
        IffFile* piffFile;
        try
        {
            std::unique_ptr<IffFile> loaded(new IffFile(entry));
            auto list = loaded->List<PIFF>();
            if (list.empty()) throw std::runtime_error("no PIFF chunk");
            piff = list[0];
            piffFile = loaded.get();
            OwnedPiffs.push_back(std::move(loaded));
        }
        catch (const std::exception&)
        {
            continue;
        }

        auto userIt = IsPIFFUser.find(piff->SourceIff);
        if (userIt != IsPIFFUser.end())
        {
            auto old = userIt->second;
            if (old != user)
            {
                if (user)
                {
                    //remove old piffs, as they have been overwritten by this user piff.
                    PIFFsByName[piff->SourceIff].clear();
                    IsPIFFUser[piff->SourceIff] = true;
                }
                else continue; //a user piff exists. ignore these ones.
            }
        }
        else IsPIFFUser[piff->SourceIff] = user;

        PIFFsByName[piff->SourceIff].push_back(piffFile);
    }

    std::vector<std::string> otfs;
    GetFiles(basePath, ".otf", otfs);

    for (auto& otf : otfs)
    {
        std::string entry = otf;
        std::replace(entry.begin(), entry.end(), '\\', '/');
        OtfRewrite[GetFileName(entry)] = entry;
    }

    for (auto& piffs : PIFFsByName)
    {
        for (auto piff : piffs.second)
        {
            //C#: piff.List<OBJD>() != null
            if (piff->HasChunkType<OBJD>())
            {
                OBJDAdded.insert(piffs.first);
                continue;
            }

            auto pList = piff->List<PIFF>();
            PIFF* pChunk = pList.empty() ? nullptr : pList[0];
            if (pChunk != nullptr && std::any_of(pChunk->Entries.begin(), pChunk->Entries.end(), [](const PIFFEntry& x) { return x.Type == "OBJD"; }))
            {
                OBJDAdded.insert(piffs.first); 
                continue;
            }
        }
    }
}

std::unordered_set<std::string>& PIFFRegistry::GetOBJDRewriteNames()
{
    return OBJDAdded;
}

std::string PIFFRegistry::GetOTFRewrite(const std::string& srcFile)
{
    auto it = OtfRewrite.find(srcFile);
    if (it == OtfRewrite.end()) return std::string();
    return it->second;
}

std::vector<IffFile*>* PIFFRegistry::GetPIFFs(const std::string& srcFile)
{
    auto it = PIFFsByName.find(srcFile);
    if (it == PIFFsByName.end()) return nullptr;
    return &it->second;
}

}}}} // namespace FSO::Files::Formats::IFF
