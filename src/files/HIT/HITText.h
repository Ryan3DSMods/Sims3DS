/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims helper (not a FreeSims file): the System.String / StreamReader operations the HIT text formats
// (HSM, HOT, EVT, FSC, TRK) use, so the parsers read like the C#.
#pragma once
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace FSO { namespace Files { namespace HIT { namespace Text {

/// File.ReadAllBytes (throws like File.Open when the file is missing).
inline std::vector<uint8_t> ReadAllBytes(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) throw std::runtime_error("Could not open file: " + path);
    std::vector<uint8_t> data;
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (n > 0) { data.resize((size_t)n); data.resize(std::fread(data.data(), 1, (size_t)n, f)); }
    std::fclose(f);
    return data;
}

/// StreamReader.ReadLine over a whole buffer: splits on \n, \r\n or \r (line terminators are not included).
inline std::vector<std::string> ReadLines(const std::vector<uint8_t>& data) {
    std::vector<std::string> lines;
    std::string cur;
    size_t i = 0, n = data.size();
    if (n >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) i = 3; // UTF-8 BOM
    for (; i < n; i++) {
        char c = (char)data[i];
        if (c == '\r') { lines.push_back(cur); cur.clear(); if (i + 1 < n && data[i + 1] == '\n') i++; }
        else if (c == '\n') { lines.push_back(cur); cur.clear(); }
        else cur += c;
    }
    if (!cur.empty()) lines.push_back(cur);
    return lines;
}

/// String.Split(char)
inline std::vector<std::string> Split(const std::string& s, char sep) {
    std::vector<std::string> out;
    size_t start = 0;
    for (;;) {
        size_t p = s.find(sep, start);
        if (p == std::string::npos) { out.push_back(s.substr(start)); break; }
        out.push_back(s.substr(start, p - start));
        start = p + 1;
    }
    return out;
}

/// String.Trim()
inline std::string Trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) a++;
    while (b > a && std::isspace((unsigned char)s[b - 1])) b--;
    return s.substr(a, b - a);
}

/// String.ToLowerInvariant()
inline std::string ToLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

inline bool StartsWith(const std::string& s, const std::string& p) { return s.compare(0, p.size(), p) == 0; }
inline bool Contains(const std::string& s, const std::string& p) { return s.find(p) != std::string::npos; }

/// int.TryParse (decimal, optional sign, surrounding whitespace allowed).
inline bool TryParseInt(const std::string& in, int32_t& result) {
    std::string s = Trim(in);
    if (s.empty()) return false;
    size_t i = 0;
    if (s[0] == '-' || s[0] == '+') i = 1;
    if (i >= s.size()) return false;
    for (size_t j = i; j < s.size(); j++) if (!std::isdigit((unsigned char)s[j])) return false;
    result = (int32_t)std::strtoll(s.c_str(), nullptr, 10);
    return true;
}

/// Convert.ToUInt32(s, 16) (accepts a 0x prefix like the C#). Throws on garbage like the C# FormatException.
inline uint32_t ToUInt32Hex(const std::string& in) {
    std::string s = Trim(in);
    if (StartsWith(ToLower(s), "0x")) s = s.substr(2);
    if (s.empty()) throw std::invalid_argument("hex");
    for (char c : s) if (!std::isxdigit((unsigned char)c)) throw std::invalid_argument("hex");
    return (uint32_t)std::strtoul(s.c_str(), nullptr, 16);
}

/// Convert.ToUInt32(s) / uint.Parse(s). Throws on garbage like the C# FormatException.
inline uint32_t ToUInt32(const std::string& in) {
    std::string s = Trim(in);
    if (s.empty()) throw std::invalid_argument("uint");
    for (char c : s) if (!std::isdigit((unsigned char)c)) throw std::invalid_argument("uint");
    return (uint32_t)std::strtoul(s.c_str(), nullptr, 10);
}

/// Convert.ToInt32(s) / int.Parse(s). Throws on garbage.
inline int32_t ToInt32(const std::string& in) {
    int32_t r = 0;
    if (!TryParseInt(in, r)) throw std::invalid_argument("int");
    return r;
}

}}}} // namespace FSO::Files::HIT::Text
