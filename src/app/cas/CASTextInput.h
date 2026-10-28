/*
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 */
// sims — text entry for Create-A-Family (last name), Create-A-Sim (first name, biography).
// On the 3DS this is the system software keyboard (libctru swkbd applet, blocking while it is shown);
// on the host CASScreen::TextInput is set by the driver (cas_host answers with scripted text).
#pragma once
#include <string>

namespace simapp { namespace cas {

struct CASTextRequest {
    std::string Hint;      // "Enter First Name:"
    std::string Initial;   // current text (Latin-1)
    int MaxLength = 32;    // characters
    bool Multiline = false;
    bool AllowEmpty = false;
};

/// The console keyboard. Returns true and the text (Latin-1) when the player confirmed. Always false on the host.
bool CASSystemKeyboard(const CASTextRequest& req, std::string& result);

/// TS1(sims) host test hook: texts queued here answer CASSystemKeyboard on the host (sim3ds_host "text" command).
void CASQueueScriptedText(const std::string& text);

}}
