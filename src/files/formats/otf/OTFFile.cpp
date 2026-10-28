/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/. 
 */
// Translated from FreeSims sims.files/formats/otf/OTFFile.cs
#include "files/formats/otf/OTFFile.h"
#include "core/io.h"
#include <stdexcept>
#include <cstdlib>
#include <cctype>
#include <map>

namespace FSO { namespace Files { namespace Formats { namespace OTF {

OTFFile::OTFFile(const std::string& filepath)
{
    std::vector<uint8_t> data;
    if (!st::ReadAllBytes(filepath, data)) throw std::runtime_error("Could not open file: " + (st::LastFileError.empty() ? filepath : st::LastFileError));
    this->Read(data);
}

namespace {
    struct Tag { std::string name; std::map<std::string, std::string> attrs; bool closing = false; bool selfClosing = false; };

    // Minimal XML tag scanner sufficient for OTF files.
    bool NextTag(const std::string& s, size_t& pos, Tag& tag)
    {
        tag = Tag();
        while (true)
        {
            size_t lt = s.find('<', pos);
            if (lt == std::string::npos) return false;
            if (s.compare(lt, 4, "<!--") == 0) { size_t e = s.find("-->", lt); pos = (e == std::string::npos) ? s.size() : e + 3; continue; }
            if (s.compare(lt, 2, "<?") == 0 || s.compare(lt, 2, "<!") == 0) { size_t e = s.find('>', lt); pos = (e == std::string::npos) ? s.size() : e + 1; continue; }
            size_t gt = s.find('>', lt);
            if (gt == std::string::npos) return false;
            std::string body = s.substr(lt + 1, gt - lt - 1);
            pos = gt + 1;
            size_t i = 0;
            if (!body.empty() && body[0] == '/') { tag.closing = true; i = 1; }
            if (!body.empty() && body.back() == '/') { tag.selfClosing = true; body.pop_back(); }
            while (i < body.size() && std::isspace((unsigned char)body[i])) i++;
            size_t ns = i;
            while (i < body.size() && !std::isspace((unsigned char)body[i])) i++;
            tag.name = body.substr(ns, i - ns);
            while (i < body.size())
            {
                while (i < body.size() && std::isspace((unsigned char)body[i])) i++;
                size_t ks = i;
                while (i < body.size() && body[i] != '=' && !std::isspace((unsigned char)body[i])) i++;
                std::string key = body.substr(ks, i - ks);
                while (i < body.size() && std::isspace((unsigned char)body[i])) i++;
                if (i >= body.size() || body[i] != '=') break;
                i++;
                while (i < body.size() && std::isspace((unsigned char)body[i])) i++;
                if (i >= body.size()) break;
                char q = body[i];
                std::string val;
                if (q == '"' || q == '\'') { i++; size_t ve = body.find(q, i); if (ve == std::string::npos) ve = body.size(); val = body.substr(i, ve - i); i = ve + 1; }
                else { size_t vs = i; while (i < body.size() && !std::isspace((unsigned char)body[i])) i++; val = body.substr(vs, i - vs); }
                // decode the few entities that appear in labels
                std::string dec; for (size_t k = 0; k < val.size(); k++) {
                    if (val[k] == '&') {
                        if (val.compare(k, 5, "&amp;") == 0) { dec += '&'; k += 4; continue; }
                        if (val.compare(k, 4, "&lt;") == 0) { dec += '<'; k += 3; continue; }
                        if (val.compare(k, 4, "&gt;") == 0) { dec += '>'; k += 3; continue; }
                        if (val.compare(k, 6, "&quot;") == 0) { dec += '"'; k += 5; continue; }
                        if (val.compare(k, 6, "&apos;") == 0) { dec += '\''; k += 5; continue; }
                    }
                    dec += val[k];
                }
                tag.attrs[key] = dec;
            }
            return true;
        }
    }
}

void OTFFile::Read(const std::vector<uint8_t>& data)
{
    std::string s(data.begin(), data.end());
    Tables.clear();
    size_t pos = 0;
    Tag tag;
    OTFTable* current = nullptr;
    while (NextTag(s, pos, tag))
    {
        if (tag.name == "T")
        {
            if (tag.closing) { current = nullptr; continue; }
            OTFTable tableEntry;
            tableEntry.ID = (int32_t)std::strtol(tag.attrs["i"].c_str(), nullptr, 10);
            tableEntry.Name = tag.attrs["n"];
            Tables.push_back(tableEntry);
            current = tag.selfClosing ? nullptr : &Tables.back();
        }
        else if (tag.name == "K" && current != nullptr && !tag.closing)
        {
            OTFTableKey keyEntry;
            keyEntry.ID = (int32_t)std::strtol(tag.attrs["i"].c_str(), nullptr, 10);
            keyEntry.Label = tag.attrs["l"];
            keyEntry.Value = (int32_t)std::strtol(tag.attrs["v"].c_str(), nullptr, 10);
            current->Keys.push_back(keyEntry);
        }
    }
}

}}}} // namespace FSO::Files::Formats::OTF
