/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
#include "vitaboy/BCFReadProxy.h"
#include <cstdlib>
#include <stdexcept>

namespace FSO { namespace Vitaboy {

static std::string Trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) a++;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) b--;
    return s.substr(a, b - a);
}

CMXReadProxy::CMXReadProxy(const std::vector<uint8_t>& data, bool skipHeader) {
    std::string line;
    for (size_t i = 0; i <= data.size(); i++) {
        if (i == data.size() || data[i] == '\n') {
            std::string t = Trim(line);
            // comment lines and blank lines carry no data anywhere in the format
            if (!t.empty() && !(t.size() >= 2 && t[0] == '/' && t[1] == '/')) Lines.push_back(t);
            line.clear();
        } else line.push_back((char)data[i]);
    }
    if (skipHeader && LineIndex < Lines.size()) {
        const std::string& first = Lines[LineIndex];
        if (first.compare(0, 7, "version") == 0) {
            Version = atoi(first.c_str() + 7);
            LineIndex++;
        }
    }
}

bool CMXReadProxy::NextLine(std::string& out) {
    if (LineIndex >= Lines.size()) return false;
    out = Lines[LineIndex++];
    return true;
}

std::string CMXReadProxy::NextToken() {
    while (true) {
        if (!InLine) {
            if (!NextLine(Current)) throw std::runtime_error("CMX: unexpected end of file");
            InLine = true;
        }
        size_t a = 0;
        while (a < Current.size() && (Current[a] == ' ' || Current[a] == '\t' || Current[a] == '|')) a++;
        if (a >= Current.size()) { InLine = false; continue; }
        size_t b = a;
        while (b < Current.size() && Current[b] != ' ' && Current[b] != '\t' && Current[b] != '|') b++;
        std::string tok = Current.substr(a, b - a);
        Current = Current.substr(b);
        // drop the rest of the line once only separators remain
        size_t c = 0;
        while (c < Current.size() && (Current[c] == ' ' || Current[c] == '\t' || Current[c] == '|')) c++;
        if (c >= Current.size()) InLine = false;
        return tok;
    }
}

int32_t CMXReadProxy::ReadInt32() {
    auto tok = NextToken();
    return (int32_t)strtol(tok.c_str(), nullptr, 10);
}

float CMXReadProxy::ReadFloat() {
    auto tok = NextToken();
    return (float)strtod(tok.c_str(), nullptr);
}

std::string CMXReadProxy::ReadPascalString() {
    if (InLine) { // remainder of a line that already yielded numbers
        InLine = false;
        return Trim(Current);
    }
    std::string s;
    if (!NextLine(s)) throw std::runtime_error("CMX: unexpected end of file");
    return s;
}

bool CMXReadProxy::HasMore() { return InLine || LineIndex < Lines.size(); }

}}
