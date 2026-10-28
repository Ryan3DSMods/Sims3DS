/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims — system keyboard for CAS names (see CASTextInput.h).
#include "app/cas/CASTextInput.h"
#include "app/Font.h"
#include <vector>
#ifdef __3DS__
#include <3ds.h>
#endif

namespace simapp { namespace cas {

#ifdef __3DS__
// Latin-1 (the game's text) -> UTF-8 (swkbd)
static std::string ToUTF8(const std::string& s) {
    std::string o;
    for (unsigned char c : s) {
        if (c < 0x80) o += (char)c;
        else { o += (char)(0xC0 | (c >> 6)); o += (char)(0x80 | (c & 0x3F)); }
    }
    return o;
}

bool CASSystemKeyboard(const CASTextRequest& req, std::string& result) {
    SwkbdState kb;
    int maxLen = req.MaxLength > 0 ? req.MaxLength : 32;
    swkbdInit(&kb, SWKBD_TYPE_NORMAL, 2, maxLen);
    swkbdSetHintText(&kb, ToUTF8(req.Hint).c_str());
    if (!req.Initial.empty()) swkbdSetInitialText(&kb, ToUTF8(req.Initial).c_str());
    swkbdSetValidation(&kb, req.AllowEmpty ? SWKBD_ANYTHING : SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
    if (req.Multiline) swkbdSetFeatures(&kb, SWKBD_MULTILINE);
    swkbdSetButton(&kb, SWKBD_BUTTON_LEFT, "Cancel", false);
    swkbdSetButton(&kb, SWKBD_BUTTON_RIGHT, "OK", true);
    std::vector<char> buf((size_t)maxLen * 4 + 4, 0);
    SwkbdButton button = swkbdInputText(&kb, buf.data(), buf.size());
    if (button != SWKBD_BUTTON_CONFIRM && button != SWKBD_BUTTON_RIGHT) return false;
    result = ToLatin1(std::string(buf.data()));
    return true;
}
void CASQueueScriptedText(const std::string&) {}
#else
// TS1(sims): host test hook (sim3ds_host "text"): scripted answers, oldest first
static std::vector<std::string>& ScriptedTexts() { static std::vector<std::string> v; return v; }
void CASQueueScriptedText(const std::string& text) { ScriptedTexts().push_back(text); }
bool CASSystemKeyboard(const CASTextRequest&, std::string& result) {
    auto& v = ScriptedTexts();
    if (v.empty()) return false;
    result = v.front();
    v.erase(v.begin());
    return true;
}
#endif

}}
