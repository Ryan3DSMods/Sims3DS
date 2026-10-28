/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// Translated from FreeSims sims.files/formats/iff/PIFFRegistry.cs
#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace FSO { namespace Files { namespace Formats { namespace IFF {

class IffFile;

class PIFFRegistry
{
private:
    static std::unordered_map<std::string, std::vector<IffFile*>> PIFFsByName;
    static std::unordered_map<std::string, std::string> OtfRewrite;
    static std::unordered_map<std::string, bool> IsPIFFUser; //if a piff is User, all other piffs for that file are ignored.
    static std::unordered_set<std::string> OBJDAdded;
    static std::vector<std::unique_ptr<IffFile>> OwnedPiffs; //ownership of loaded piff files

public:
    static void Init(const std::string& basePath);

    static std::unordered_set<std::string>& GetOBJDRewriteNames();

    /// Returns empty string when there is no rewrite (C#: null).
    static std::string GetOTFRewrite(const std::string& srcFile);

    /// Returns nullptr when there are no piffs for the file (C#: null).
    static std::vector<IffFile*>* GetPIFFs(const std::string& srcFile);
};

}}}} // namespace FSO::Files::Formats::IFF
