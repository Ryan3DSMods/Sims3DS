/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims front end — see FrontStrings.h
#include "app/front/FrontStrings.h"
#include "app/Font.h"
#include "app/front/TS1Font.h"
#include "files/formats/iff/IffFile.h"
#include "files/formats/iff/chunks/STR.h"
#include <cstdio>
#include <algorithm>

namespace simapp { namespace front {

FrontStrings& FrontStrings::Get() {
    static FrontStrings s;
    return s;
}

void FrontStrings::Load(const std::string& gameDataDir) {
    static const int tables[] = { 128, 129, 131, 163, 132, 134, 145, 151, 152, 153, 154, 155, 169, 170, 173, 174, 212, 213, 225, 260, 261 };
    _Strings.clear();
    try {
        FSO::Files::Formats::IFF::IffFile iff(gameDataDir + "/UIText.iff");
        for (int t : tables) {
            auto str = iff.Get<FSO::Files::Formats::IFF::Chunks::STR>((uint16_t)t);
            if (str == nullptr) continue;
            int n = std::min<int>((int)str->Length(), 200);
            for (int i = 0; i < n; i++) _Strings[t * 10000 + i] = ToLatin1(str->GetString(i));
        }
    } catch (std::exception& e) { printf("[front] UIText.iff not loaded: %s\n", e.what()); }
}

std::string FrontStrings::Get(int table, int index, const char* fallback) const {
    auto it = _Strings.find(table * 10000 + index);
    if (it == _Strings.end() || it->second.empty()) return fallback;
    return it->second;
}

std::string Format(const std::string& fmt, const std::vector<std::string>& args) {
    std::string out;
    size_t a = 0;
    for (size_t i = 0; i < fmt.size(); i++) {
        if (fmt[i] == '%' && i + 1 < fmt.size() && (fmt[i + 1] == 's' || fmt[i + 1] == 'd')) {
            if (a < args.size()) out += args[a++];
            i++;
        } else out += fmt[i];
    }
    return out;
}

std::string Simoleons(int v) {
    std::string digits = std::to_string(v < 0 ? -v : v), out;
    int c = 0;
    for (int i = (int)digits.size() - 1; i >= 0; i--) {
        out.insert(out.begin(), digits[(size_t)i]);
        if (++c % 3 == 0 && i > 0) out.insert(out.begin(), ',');
    }
    // the original font draws its Simoleon sign for '$'; the fallback font has the Latin-1 section sign
    return std::string(v < 0 ? "-" : "") + (TS1FontsLoaded() ? "$" : "\xA7") + out;
}

}}
